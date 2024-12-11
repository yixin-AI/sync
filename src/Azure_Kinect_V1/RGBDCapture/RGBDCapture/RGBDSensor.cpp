#include "RGBDSensor.h"

void RGBDSensor::createRGBDFolders()
{
	std::string folder = "./save";
	int i = 0;
	while (true){
		std::string dir = folder + std::to_string(i);
		if (std::filesystem::is_directory(dir))
		{
			i++;
		}
		else
		{
			m_strRGBDFolder = dir;
			std::string dirDepth = dir + "/depth/";
			std::string dirColor = dir + "/rgb/";
			std::filesystem::create_directory(dir);
			std::filesystem::create_directory(dirDepth);
			std::filesystem::create_directory(dirColor);
			break;
		}
	}
}