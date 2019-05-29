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

// --- Entry point ---
int main(int argc, char* argv[]) {	
	// - Install signal handler
	std::signal(SIGINT, sigintHandler);
	
	// - Device
	ServerDevice device0(Globals::PATH_0, 6666);
	ServerDevice device1(Globals::PATH_1, 8888);

	
	// -------- Main loop --------  
	if(!device0.open(-1)) {
		std::cout << "Can't open device" << std::endl;
		std::cout << "Press a key to continue..." << std::endl;
		return std::cin.get();
	}
	if(!device1.open(-1)) {
		std::cout << "Can't open device" << std::endl;
		std::cout << "Press a key to continue..." << std::endl;
		return std::cin.get();
	}
	
	// std::vector<Gb::FrameType> typeList = {
		// Gb::FrameType::Jpg420, 
		// Gb::FrameType::H264
	// };
	// size_t i = 0;
	
	for(Timer timer; Globals::signalStatus != SIGINT; timer.wait(100)) {
		// // ... Do other stuff ...
		// if(timer.elapsed_mus() > 10000000) {
			// std::cout << "Change type" << std::endl;
			// device0.setFrameType(typeList[i]);
			// device1.setFrameType(typeList[i]);
			// i = (i + 1)%typeList.size();
			// timer.beg();
		// }
	}
	
	// -- End
	device0.close();
	device1.close();
	
	std::cout << "Clean exit" << std::endl;
	std::cout << "Press a key to continue..." << std::endl;
	return std::cin.get();
}
