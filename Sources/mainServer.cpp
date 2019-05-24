#include <iostream>
#include <csignal>

#include "StreamDevice/ServerDevice.hpp"

#ifdef __linux__
	#define PATH_CAMERA_0 "/dev/video0"
	#define PATH_CAMERA_1 "/dev/video1"

#elif _WIN32
	#define PATH_CAMERA_0 "0"
	#define PATH_CAMERA_1 "1"
	
#endif

namespace Globals {
	// Constantes
	const int PORT = 8888;
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
	// - Inputs
	// None..
	
	// - Install signal handler
	std::signal(SIGINT, sigintHandler);
	
	// - Device
	ServerDevice device(Globals::PATH_0, Globals::PORT);
	
	// - Events
	device.onOpen([&]() {
		std::cout << "Device opened" << std::endl;
	});
	device.onFrame([&](const Gb::Frame& frame) {
		// nothing yet to do ..
	});
	device.onError([&](const Error& error) {
		std::cout << "Error occured" << std::endl;
	});
	
	
	// -------- Main loop --------  
	if(!device.open()) {
		std::cout << "Couldn't open device" << std::endl;
		std::cout << "Press a key to continue..." << std::endl;
		return std::cin.get();
	}
	
	for(Timer timer; Globals::signalStatus != SIGINT; timer.wait(100)) {
		// ... Do other stuff ...
	}
	
	// -- End
	device.close();
	
	std::cout << "Clean exit" << std::endl;
	std::cout << "Press a key to continue..." << std::endl;
	return std::cin.get();
}
