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
	// ServerDevice device0(Globals::PATH_0, 5000);
	// ServerDevice device1(Globals::PATH_1, 6000);
	
	Server server;
	server.connectAt(5000);
	
	server.onInfo([&](const Server::ClientInfo& client, const Message& message) {
		std::cout << "info" << std::endl;
	});
	server.onClientConnect([&](const Server::ClientInfo& client) {
		std::cout << "co" << std::endl;
	});
	server.onClientDisconnect([&](const Server::ClientInfo& client) {
		std::cout << "deco" << std::endl;
	});
	
	// - Events
	// device0.onOpen([&]() {
		// std::cout << "Device opened 0" << std::endl;
		// // device0.setFormat(1280, 720, Device::MJPG);
	// });
	// device1.onOpen([&]() {
		// std::cout << "Device opened 1" << std::endl;
		// // device1.setFormat(1280, 720, Device::MJPG);
	// });
	// device.onFrame([&](const Gb::Frame& frame) {
		// // nothing yet to do ..
	// });
	// device.onError([&](const Error& error) {
		// std::cout << "Error occured" << std::endl;
	// });
	
	
	// -------- Main loop --------  
	// device0.open();
	// device1.open();
	
	for(Timer timer; Globals::signalStatus != SIGINT; timer.wait(100)) {
		// ... Do other stuff ...
	}
	
	server.disconnect();
	
	// -- End
	// device0.close();
	// device1.close();
	
	std::cout << "Clean exit" << std::endl;
	std::cout << "Press a key to continue..." << std::endl;
	return std::cin.get();
}
