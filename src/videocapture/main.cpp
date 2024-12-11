//#include "OpenNISensor.h"
//
//int main(int argc, char** argv)
//{	
//	RGBDSensor* sensor = new OpenNISensor();
//	sensor->scan();
//	delete sensor;
//	return 0;
//}

#include "VideoStreamer.h"

int main() {
    VideoStreamer streamer;
    return streamer.run();
}
