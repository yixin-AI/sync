/*
 * Copyright (c) 2016 SEBASTIEN DERONNE
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Sebastien Deronne <sebastien.deronne@gmail.com>
 */

#include "ns3/boolean.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/double.h"
#include "ns3/enum.h"
#include "ns3/he-phy.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/log.h"
#include "ns3/mobility-helper.h"
#include "ns3/multi-model-spectrum-channel.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/string.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/udp-server.h"
#include "ns3/uinteger.h"
#include "ns3/wifi-acknowledgment.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/delay-jitter-estimation.h"
#include "ns3/onoff-application.h"

#include <chrono>
#include <functional>

using namespace ns3;

/** two host <--> WIFI <--> two host
 */

NS_LOG_COMPONENT_DEFINE("he-wifi-network");


uint32_t totalPacketsReceivedVideo = 0;
uint32_t totalPacketsReceivedHaptic = 0;
uint32_t totalPacketsReceivedCtrl = 0;
uint32_t totalPacketsSendVideo = 0;
uint32_t totalPacketsSendHaptic = 0;
uint32_t totalPacketsSendCtrl = 0;

static void TagTx(Ptr<const ns3::Packet> p)
{
    DelayJitterEstimation delayJitter;
    delayJitter.PrepareTx(p);
}

void CalculateDelay(Ptr<const ns3::Packet> p, const Address &address, std::string filename) {

    DelayJitterEstimation delayJitter;
    delayJitter.RecordRx(p);
    Time t = delayJitter.GetLastDelay();

    // Record delay in .csv file
    std::ofstream delayFile(filename, std::ios::app);
    if (delayFile.is_open()) {
        delayFile << Simulator::Now().GetSeconds() << "," << t.GetMilliSeconds() << std::endl;
        delayFile.close();
    }
    else {
        std::cout << "file " << filename << " can not open!" << std::endl;
    }
}

void CalculateDelay1(Ptr<const ns3::Packet> p, const Address &address)
{
    CalculateDelay(p, address, "./data/openwrt/delay_n0n5.csv");
}

void CalculateDelay2(Ptr<const ns3::Packet> p, const Address &address)
{
    CalculateDelay(p, address, "./data/openwrt/delay_n1n4.csv");
}

void CalculateDelay3(Ptr<const ns3::Packet> p, const Address &address)
{
    CalculateDelay(p, address, "./data/openwrt/delay_n2n3.csv");
}

void IncrementSendVideo(Ptr<const ns3::Packet> p){
    totalPacketsSendVideo++;
}

void IncrementSendHaptic(Ptr<const ns3::Packet> p){
    totalPacketsSendHaptic++;
}

void IncrementSendCtrl(Ptr<const ns3::Packet> p){
    totalPacketsSendCtrl++;
}

void IncrementReceivedVideo(Ptr<const ns3::Packet> p, const Address &address){
    totalPacketsReceivedVideo++;
}

void IncrementReceivedHaptic(Ptr<const ns3::Packet> p, const Address &address){
    totalPacketsReceivedHaptic++;
}

void IncrementReceivedCtrl(Ptr<const ns3::Packet> p, const Address &address){
    totalPacketsReceivedCtrl++;
}

int
main(int argc, char* argv[])
{

    LogComponentEnable("he-wifi-network", LOG_LEVEL_LOGIC);
     auto mainStart = std::chrono::high_resolution_clock::now();

    // ================ Config ================
    int mcs{4}; // set 5 by default
    int channelWidth = 40; // MHz
    int gi = 1600;         // Nanoseconds (3200 ns, 1600 ns (OFDM min), 800 ns)
    
    bool udp{true};             // true = udp , false = tcp
    bool downlink{true};
    bool useRts{true};
    bool useExtendedBlockAck{false};
    Time simulationTime{"10s"};
    double distance{1.0}; // meters
    double frequency{5};  // whether 2.4, 5 or 6 GHz
    std::size_t nStations{4};
    std::string dlAckSeqType{"NO-OFDMA"};
    bool enableUlOfdma{false};
    bool enableBsrp{false};
    
    uint32_t payloadSize = 700; // must fit in the max TX duration when transmitting at MCS 0 over an RU of 26 tones
    std::string phyModel{"Yans"};
    double minExpectedThroughput{0};
    double maxExpectedThroughput{0};
    Time accessReqInterval{0};


    CommandLine cmd(__FILE__);
    cmd.AddValue("frequency",
                 "Whether working in the 2.4, 5 or 6 GHz band (other values gets rejected)",
                 frequency);
    cmd.AddValue("distance",
                 "Distance in meters between the station and the access point",
                 distance);
    cmd.AddValue("simulationTime", "Simulation time", simulationTime);
    cmd.AddValue("udp", "UDP if set to 1, TCP otherwise", udp);
    cmd.AddValue("downlink",
                 "Generate downlink flows if set to 1, uplink flows otherwise",
                 downlink);
    cmd.AddValue("useRts", "Enable/disable RTS/CTS", useRts);
    cmd.AddValue("useExtendedBlockAck", "Enable/disable use of extended BACK", useExtendedBlockAck);
    cmd.AddValue("nStations", "Number of non-AP HE stations", nStations);
    cmd.AddValue("dlAckType",
                 "Ack sequence type for DL OFDMA (NO-OFDMA, ACK-SU-FORMAT, MU-BAR, AGGR-MU-BAR)",
                 dlAckSeqType);
    cmd.AddValue("enableUlOfdma",
                 "Enable UL OFDMA (useful if DL OFDMA is enabled and TCP is used)",
                 enableUlOfdma);
    cmd.AddValue("enableBsrp",
                 "Enable BSRP (useful if DL and UL OFDMA are enabled and TCP is used)",
                 enableBsrp);
    cmd.AddValue(
        "muSchedAccessReqInterval",
        "Duration of the interval between two requests for channel access made by the MU scheduler",
        accessReqInterval);
    cmd.AddValue("mcs", "if set, limit testing to a specific MCS (0-11)", mcs);
    cmd.AddValue("payloadSize", "The application payload size in bytes", payloadSize);
    cmd.AddValue("phyModel",
                 "PHY model to use when OFDMA is disabled (Yans or Spectrum). If OFDMA is enabled "
                 "then Spectrum is automatically selected",
                 phyModel);
    cmd.AddValue("minExpectedThroughput",
                 "if set, simulation fails if the lowest throughput is below this value",
                 minExpectedThroughput);
    cmd.AddValue("maxExpectedThroughput",
                 "if set, simulation fails if the highest throughput is above this value",
                 maxExpectedThroughput);
    cmd.Parse(argc, argv);

    if (useRts)
    {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("0"));
        Config::SetDefault("ns3::WifiDefaultProtectionManager::EnableMuRts", BooleanValue(true));
    }

    if (dlAckSeqType == "ACK-SU-FORMAT")
    {
        Config::SetDefault("ns3::WifiDefaultAckManager::DlMuAckSequenceType",
                           EnumValue(WifiAcknowledgment::DL_MU_BAR_BA_SEQUENCE));
    }
    else if (dlAckSeqType == "MU-BAR")
    {
        Config::SetDefault("ns3::WifiDefaultAckManager::DlMuAckSequenceType",
                           EnumValue(WifiAcknowledgment::DL_MU_TF_MU_BAR));
    }
    else if (dlAckSeqType == "AGGR-MU-BAR")
    {
        Config::SetDefault("ns3::WifiDefaultAckManager::DlMuAckSequenceType",
                           EnumValue(WifiAcknowledgment::DL_MU_AGGREGATE_TF));
    }
    else if (dlAckSeqType != "NO-OFDMA")
    {
        NS_ABORT_MSG("Invalid DL ack sequence type (must be NO-OFDMA, ACK-SU-FORMAT, MU-BAR or "
                     "AGGR-MU-BAR)");
    }

    if (phyModel != "Yans" && phyModel != "Spectrum")
    {
        NS_ABORT_MSG("Invalid PHY model (must be Yans or Spectrum)");
    }
    if (dlAckSeqType != "NO-OFDMA")
    {
        // SpectrumWifiPhy is required for OFDMA
        phyModel = "Spectrum";
    }

    // double prevThroughput[12] = {0};
    
    int minMcs = 0;
    int maxMcs = 11;
    
    if (mcs >= 0 && mcs <= 11)
    {
        minMcs = mcs;
        maxMcs = mcs;
    }
    else {
        NS_LOG_ERROR("Invalid MCS value!");
        exit(1);
    }
    for (int mcs = minMcs; mcs <= maxMcs; mcs++)
    {
        /* Purpose: Iterate over all Modulation and Coding Scheme (MCS) values
         from minMcs to maxMcs. Each MCS value represents a different combination
          of modulation and coding, which affects the data rate and robustness 
          of the Wi-Fi signal.
         */
        
        // uint8_t index = 0;
        // double previous = 0; // Stores the previous throughput value for comparison.
        
        // Maximum channel width depends on the frequency band
        // uint8_t maxChannelWidth = frequency == 2.4 ? 40 : 160;
        int minGi = enableUlOfdma ? 1600 : 800;
        
        if (gi < minGi)
        {
            gi = minGi;
        }

        if (!udp)
        {
            Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(payloadSize));
        }

        // Setup Wi-Fi Nodes and Devices
        NodeContainer wifiStaNodes;
        wifiStaNodes.Create(nStations);
        NodeContainer wifiApNode;
        wifiApNode.Create(1);

        NetDeviceContainer apDevice;
        NetDeviceContainer staDevices;

        // Configure Wi-Fi Parameters
        WifiMacHelper mac;
        WifiHelper wifi;
        std::string channelStr("{0, " + std::to_string(channelWidth) + ", ");
        StringValue ctrlRate;
        auto nonHtRefRateMbps = HePhy::GetNonHtReferenceRate(mcs) / 1e6;

        std::ostringstream ossDataMode;
        ossDataMode << "HeMcs" << mcs;

        // Configure Wi-Fi Standards and Frequencies
        if (frequency == 6)
        {
            wifi.SetStandard(WIFI_STANDARD_80211ax);
            ctrlRate = StringValue(ossDataMode.str());
            channelStr += "BAND_6GHZ, 0}";
            Config::SetDefault("ns3::LogDistancePropagationLossModel::ReferenceLoss",
                                DoubleValue(48));
        }
        else if (frequency == 5)
        {
            wifi.SetStandard(WIFI_STANDARD_80211ax);
            std::ostringstream ossControlMode;
            ossControlMode << "OfdmRate" << nonHtRefRateMbps << "Mbps";
            ctrlRate = StringValue(ossControlMode.str());
            channelStr += "BAND_5GHZ, 0}";
        }
        else if (frequency == 2.4)
        {
            wifi.SetStandard(WIFI_STANDARD_80211ax);
            std::ostringstream ossControlMode;
            ossControlMode << "ErpOfdmRate" << nonHtRefRateMbps << "Mbps";
            ctrlRate = StringValue(ossControlMode.str());
            channelStr += "BAND_2_4GHZ, 0}";
            Config::SetDefault("ns3::LogDistancePropagationLossModel::ReferenceLoss",
                                DoubleValue(40));
        }
        else
        {
            NS_FATAL_ERROR("Wrong frequency value!");
        }

        // Install Wi-Fi Devices
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                        "DataMode",
                                        StringValue(ossDataMode.str()),
                                        "ControlMode",
                                        ctrlRate);
        // Set guard interval
        wifi.ConfigHeOptions("GuardInterval", TimeValue(NanoSeconds(gi)));

        Ssid ssid = Ssid("ns3-80211ax");

        // if (phyModel == "Spectrum")
        // {
        //     /*
        //         * SingleModelSpectrumChannel cannot be used with 802.11ax because two
        //         * spectrum models are required: one with 78.125 kHz bands for HE PPDUs
        //         * and one with 312.5 kHz bands for, e.g., non-HT PPDUs (for more details,
        //         * see issue #408 (CLOSED))
        //         */
        //     Ptr<MultiModelSpectrumChannel> spectrumChannel =
        //         CreateObject<MultiModelSpectrumChannel>();

        //     Ptr<LogDistancePropagationLossModel> lossModel =
        //         CreateObject<LogDistancePropagationLossModel>();
        //     spectrumChannel->AddPropagationLossModel(lossModel);

        //     SpectrumWifiPhyHelper phy;
        //     phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        //     phy.SetChannel(spectrumChannel);

        //     mac.SetType("ns3::StaWifiMac",
        //                 "Ssid",
        //                 SsidValue(ssid),
        //                 "MpduBufferSize",
        //                 UintegerValue(useExtendedBlockAck ? 256 : 64));
        //     phy.Set("ChannelSettings", StringValue(channelStr));
        //     staDevices = wifi.Install(phy, mac, wifiStaNodes);

        //     if (dlAckSeqType != "NO-OFDMA")
        //     {
        //         mac.SetMultiUserScheduler("ns3::RrMultiUserScheduler",
        //                                     "EnableUlOfdma",
        //                                     BooleanValue(enableUlOfdma),
        //                                     "EnableBsrp",
        //                                     BooleanValue(enableBsrp),
        //                                     "AccessReqInterval",
        //                                     TimeValue(accessReqInterval));
        //     }
        //     mac.SetType("ns3::ApWifiMac",
        //                 "EnableBeaconJitter",
        //                 BooleanValue(false),
        //                 "Ssid",
        //                 SsidValue(ssid));
        //     apDevice = wifi.Install(phy, mac, wifiApNode);
        // }
        // else
        // {

        // PHY and MAC Layer Configuration
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy;
        phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        phy.SetChannel(channel.Create());

        mac.SetType("ns3::StaWifiMac",
                    "Ssid",
                    SsidValue(ssid),
                    "MpduBufferSize",
                    UintegerValue(useExtendedBlockAck ? 256 : 64));
        phy.Set("ChannelSettings", StringValue(channelStr));
        staDevices = wifi.Install(phy, mac, wifiStaNodes);

        mac.SetType("ns3::ApWifiMac",
                    "EnableBeaconJitter",
                    BooleanValue(false),
                    "Ssid",
                    SsidValue(ssid));
        apDevice = wifi.Install(phy, mac, wifiApNode);

        // }

        int64_t streamNumber = 150;
        streamNumber += WifiHelper::AssignStreams(apDevice, streamNumber);
        streamNumber += WifiHelper::AssignStreams(staDevices, streamNumber);

        // Mobility setup
        MobilityHelper mobility;

        // Create a position allocator
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

        // Add positions
        positionAlloc->Add(Vector(4.0, 0.0, 4.0));       // AP position
        positionAlloc->Add(Vector(4.0, -2.0, 0.5));      // STA 1 position
        positionAlloc->Add(Vector(6.0, -2.0, 0.5));      // STA 2 position
        positionAlloc->Add(Vector(4.0, 2.0, 0.5));       // STA 3 position
        positionAlloc->Add(Vector(4.0, 2.0, 0.5));       // STA 4 position

        // Set the position allocator to the mobility helper
        mobility.SetPositionAllocator(positionAlloc);

        // Set the mobility model
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

        // Install mobility model on AP node
        mobility.Install(wifiApNode.Get(0));

        // Install mobility model on STA nodes
        for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
        {
            mobility.Install(wifiStaNodes.Get(i));
        }

        /* Internet stack*/
        InternetStackHelper stack;
        stack.Install(wifiApNode);
        stack.Install(wifiStaNodes);
        streamNumber += stack.AssignStreams(wifiApNode, streamNumber);
        streamNumber += stack.AssignStreams(wifiStaNodes, streamNumber);

        Ipv4AddressHelper address;
        address.SetBase("10.1.3.0", "255.255.255.0");
        Ipv4InterfaceContainer staInterfaces;
        Ipv4InterfaceContainer apInterface;

        staInterfaces = address.Assign(staDevices);
        apInterface = address.Assign(apDevice);

        /* Setting applications */
        uint16_t port1 = 8000;
        uint16_t port2 = 8001;
        uint16_t port3 = 8002;

        // Host 1 to Host 3
        OnOffHelper onOffHelper1("ns3::UdpSocketFactory", InetSocketAddress(staInterfaces.GetAddress(2), port1));
        onOffHelper1.SetAttribute("DataRate", StringValue("20Mbps")); 
        onOffHelper1.SetAttribute("PacketSize", UintegerValue(1400)); // video data
        ApplicationContainer app1 = onOffHelper1.Install(wifiStaNodes.Get(0));
        app1.Start(Seconds(1.0));
        app1.Stop(Seconds(10.0));

        // Host 2 to Host 4
        OnOffHelper onOffHelper2("ns3::UdpSocketFactory", InetSocketAddress(staInterfaces.GetAddress(3), port2));
        onOffHelper2.SetAttribute("DataRate", StringValue("5.16Mbps"));  // 1000 packets per second
        onOffHelper2.SetAttribute("PacketSize", UintegerValue(270));    // Haptic data
        ApplicationContainer app2 = onOffHelper2.Install(wifiStaNodes.Get(1));
        app2.Start(Seconds(1.0));
        app2.Stop(Seconds(10.0));

        // Host 4 to Host 2
        OnOffHelper onOffHelper3("ns3::UdpSocketFactory", InetSocketAddress(staInterfaces.GetAddress(1), port3));
        onOffHelper3.SetAttribute("DataRate", StringValue("0.56Mbps"));  // 200 packets per second
        onOffHelper3.SetAttribute("PacketSize", UintegerValue(70));     // Control data
        ApplicationContainer app3 = onOffHelper3.Install(wifiStaNodes.Get(3));
        app1.Start(Seconds(1.0));
        app1.Stop(Seconds(10.0));

        // Set up PacketSink to receive the packets
        PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port1));
        ApplicationContainer sinkApps1 = packetSinkHelper.Install(wifiStaNodes.Get(2));
        sinkApps1.Start(Seconds(0.0));
        sinkApps1.Stop(Seconds(13.0));

        packetSinkHelper.SetAttribute("Local", AddressValue(InetSocketAddress(Ipv4Address::GetAny(), port2)));
        ApplicationContainer sinkApps2 = packetSinkHelper.Install(wifiStaNodes.Get(3));
        sinkApps2.Start(Seconds(0.0));
        sinkApps2.Stop(Seconds(13.0));

        packetSinkHelper.SetAttribute("Local", AddressValue(InetSocketAddress(Ipv4Address::GetAny(), port3)));
        ApplicationContainer sinkApps3 = packetSinkHelper.Install(wifiStaNodes.Get(1));
        sinkApps3.Start(Seconds(0.0));
        sinkApps3.Stop(Seconds(13.0));

        // Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback(&PacketReceived));
        // Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/Mac/MacTx", MakeCallback(&PacketTransmitted));

        Simulator::Schedule(Seconds(0), &Ipv4GlobalRoutingHelper::PopulateRoutingTables);

        FlowMonitorHelper flowmon;
        Ptr<FlowMonitor> monitor = flowmon.InstallAll();

        // 启用PCAP捕获
        // ============================== pcap ============================== 

        // Enable PCAP tracing individually for each device
        phy.EnablePcap("wifi-ap", apDevice.Get(0));

        for (uint32_t i = 0; i < staDevices.GetN(); ++i)
        {
            std::ostringstream oss;
            oss << "wifi-sta" << i;
            phy.EnablePcap(oss.str(), staDevices.Get(i));
        }

        // ============================== delay tracing ============================== 

        Ptr<OnOffApplication> ptr_app1 = DynamicCast<OnOffApplication> (wifiStaNodes.Get(0)->GetApplication(0));  // video
        Ptr<OnOffApplication> ptr_app2 = DynamicCast<OnOffApplication> (wifiStaNodes.Get(1)->GetApplication(0));  // haptic
        Ptr<OnOffApplication> ptr_app4 = DynamicCast<OnOffApplication> (wifiStaNodes.Get(3)->GetApplication(0));  // control

        ptr_app1->TraceConnectWithoutContext("Tx", MakeCallback(&TagTx)); // add tag when TX
        sinkApps1.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&CalculateDelay1)); // calculate delay when RX
        
        ptr_app2->TraceConnectWithoutContext("Tx", MakeCallback(&TagTx));
        sinkApps2.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&CalculateDelay2));

        ptr_app4->TraceConnectWithoutContext("Tx", MakeCallback(&TagTx));
        sinkApps3.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&CalculateDelay3));

        ptr_app1->TraceConnectWithoutContext("Tx", MakeCallback(&IncrementSendVideo));
        sinkApps1.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&IncrementReceivedVideo));

        ptr_app2->TraceConnectWithoutContext("Tx", MakeCallback(&IncrementSendHaptic));
        sinkApps2.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&IncrementReceivedHaptic));

        ptr_app4->TraceConnectWithoutContext("Tx", MakeCallback(&IncrementSendCtrl));
        sinkApps3.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&IncrementReceivedCtrl));

        // ============================== simulation ============================== 

        auto simulateStart = std::chrono::high_resolution_clock::now();
        Simulator::Stop(simulationTime + Seconds(5.0));
        Simulator::Run();

        // monitor->SerializeToXmlFile("flowmon-results.xml", true, true);

        // // Animation interface
        // AnimationInterface anim("wifi-animation.xml");
        // // Update node descriptions
        // anim.UpdateNodeDescription(wifiApNode.Get(0), "AP");
        // anim.UpdateNodeDescription(wifiStaNodes.Get(0), "h1 (camera)");
        // anim.UpdateNodeDescription(wifiStaNodes.Get(1), "h2 (haptic1)");
        // anim.UpdateNodeDescription(wifiStaNodes.Get(2), "h3 (monitor)");
        // anim.UpdateNodeDescription(wifiStaNodes.Get(3), "h4 (haptic2)");
        // // Update node positions to include z-coordinates for 3D representation
        // anim.SetConstantPosition(wifiApNode.Get(0), 4.0, 0.0, 4.0);
        // anim.SetConstantPosition(wifiStaNodes.Get(0), 4.0, -2.0, 0.5);
        // anim.SetConstantPosition(wifiStaNodes.Get(1), 6.0, -2.0, 0.5);
        // anim.SetConstantPosition(wifiStaNodes.Get(2), 4.0, 2.0, 0.5);
        // anim.SetConstantPosition(wifiStaNodes.Get(3), 4.0, 2.0, 0.5);

        Simulator::Destroy();
        auto SimEnd = std::chrono::high_resolution_clock::now();
        
        // ============================== result summary ============================== 

        double drop_video = (double)(totalPacketsSendVideo - totalPacketsReceivedVideo) / totalPacketsSendVideo * 100;
        double drop_haptic = (double)(totalPacketsSendHaptic - totalPacketsReceivedHaptic) / totalPacketsSendHaptic * 100;
        double drop_ctrl = (double)(totalPacketsSendCtrl - totalPacketsReceivedCtrl) / totalPacketsSendCtrl * 100;
        auto program_duration = std::chrono::duration_cast<std::chrono::milliseconds>(SimEnd - mainStart).count();
        auto simulation_duration = std::chrono::duration_cast<std::chrono::milliseconds>(SimEnd - simulateStart).count();
        NS_LOG_LOGIC(
            "====WIFI HE 6====================================================================" << "\n" <<
            "Frequency: " << frequency << " GHz" << "\n" <<
            "Distance: (future disable)" << distance << " meters" << "\n" <<
            "Simulation time: " << simulationTime << "\n" <<
            "UDP: (disable)" << udp << "\n" <<
            "Downlink: (disable) " << downlink << "\n" <<
            "Use RTS/CTS: " << useRts << "\n" <<
            "Use Extended Block Ack: " << useExtendedBlockAck << "\n" <<
            "Number of non-AP HE stations: " << nStations << "\n" <<
            "DL Ack Sequence Type: " << dlAckSeqType << "\n" <<
            "Enable UL OFDMA: " << enableUlOfdma << "\n" <<
            "Enable BSRP: " << enableBsrp << "\n" <<
            "Access Req Interval: " << accessReqInterval << "\n" <<
            "MCS: " << mcs << "\n" <<
            "PHY Model: " << phyModel << "\n" <<
            "Guard Interval: " << gi << " ns" << "\n" <<
            "====WIFI HE 6====================================================================" << "\n"
        );
        
        NS_LOG_LOGIC(
                "total Packets send video: " << totalPacketsSendVideo  << "\n" <<
                "total Packets received video: " << totalPacketsReceivedVideo << "\n" <<              
                "total Packets send haptic: " << totalPacketsSendHaptic  << "\n" <<
                "total Packets received haptic: " << totalPacketsReceivedHaptic << "\n" <<
                "total Packets send control: " << totalPacketsSendCtrl  << "\n" <<
                "total Packets received control: " << totalPacketsReceivedCtrl << "\n" <<
                "Pakcet loss video in %: " << drop_video << "%" << "\n" <<
                "Pakcet loss haptic in %: " << drop_haptic << "%" << "\n" <<
                "Pakcet loss control in %: " << drop_ctrl << "%" << "\n"
                "Program Running in cpu time: " << program_duration << "ms" << "\n"
                "Simulate Running in cpu time: " << simulation_duration << "ms" << "\n"
                );
    }
    return 0;
}
