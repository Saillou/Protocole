#include <iostream>
#include <csignal>

#include "StreamDevice/ServerDevice.hpp"
#include "Tool/Timer.hpp"

#ifdef __linux__
	#define PATH_CAMERA_0 "/dev/video0"
	#define PATH_CAMERA_1 "/dev/video1"

#elif _WIN32
	#define PATH_CAMERA_0 "0"
	#define PATH_CAMERA_1 "1"
	
#endif

namespace Globals {
	// Constantes
	const std::string PATH_0 = PATH_CAMERA_0;
	const std::string PATH_1 = PATH_CAMERA_1;
	
	// Variables
	volatile std::sig_atomic_t signalStatus = 0;
}

// --- Signals ---
static void sigintHandler(int signal) {
	Globals::signalStatus = signal;
}

int main() {
	Device device(Globals::PATH_0);
	device.open();
	
	Gb::Frame frame;
	
	std::vector<std::pair<int,int>> fmtList = {
		std::pair<int,int>(1280, 720), 
		std::pair<int,int>(640, 480), 
		std::pair<int,int>(320, 200), 
	};
	size_t k = 0;
	int i = 1;
	for(; i <= 35; i++) {
		device.grab();
		device.retrieve(frame);
		
		if(i%30 == 0) {
			device.setFormat(fmtList[k].first, fmtList[k].second, Device::MJPG);
			k = (k + 1) % fmtList.size();
		}
	}
	std::cout << i << "%" << std::endl;
	device.close();
	return 0;
}

// --- Entry point ---
/*
int main(int argc, char* argv[]) {	
	// - Install signal handler
	std::signal(SIGINT, sigintHandler);
	
	// - Device
	ServerDevice device0(Globals::PATH_0, 6666);
	ServerDevice device1(Globals::PATH_1, 8888);

	
	// -------- Main loop --------  
	// if(!device0.open(-1)) {
		// std::cout << "Can't open device" << std::endl;
		// std::cout << "Press a key to continue..." << std::endl;
		// return std::cin.get();
	// }
	if(!device1.open(-1)) {
		std::cout << "Can't open device" << std::endl;
		std::cout << "Press a key to continue..." << std::endl;
		return std::cin.get();
	}
	
	std::vector<std::pair<int,int>> fmtList = {
		std::pair<int,int>(1280, 720), 
		std::pair<int,int>(640, 480), 
		std::pair<int,int>(320, 200), 
	};
	size_t i = 0;
	
	for(Timer timer; Globals::signalStatus != SIGINT; timer.wait(100)) {
		// // ... Do other stuff ...
		if(timer.elapsed_mus() > 10000000) {
			std::cout << "Change format" << std::endl;
			// device0.setFormat(fmtList[i].first, fmtList[i].second, Device::MJPG);
			device1.setFormat(fmtList[i].first, fmtList[i].second, Device::MJPG);
			
			i = (i + 1) % fmtList.size();
			timer.beg();
		}
	}
	
	// -- End
	// device0.close();
	device1.close();
	
	std::cout << "Clean exit" << std::endl;
	std::cout << "Press a key to continue..." << std::endl;
	return std::cin.get();
}
*/