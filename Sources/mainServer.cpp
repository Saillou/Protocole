#include <iostream>
#include <csignal>
#include <atomic>

#include "Device/DeviceMt.hpp"
#include "Network/Server.hpp"
#include "Network/Message.hpp"
#include "Timer.hpp"

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
	
	// Variables
	volatile std::sig_atomic_t signalStatus = 0;
}

// --- Signals ---
static void sigintHandler(int signal) {
	Globals::signalStatus = signal;
}

// --- Entry point ---
int main() {
	// -- Install signal handler
	std::signal(SIGINT, sigintHandler);
	
	// -- Create --
	Server server;
	server.connectAt(Globals::PORT);
	
	DeviceMt device0;
	if(device0.open(PATH_CAMERA_0)) {
		// Events
		device0.onFrame([&](const Gb::Frame& frame){
			for(auto& client: server.getClients()) {
				if(client.connected) {
					server.sendData(client, Message(Message::DEVICE_0, reinterpret_cast<const char*>(frame.start()), frame.length()));
				}
			}
		});
	}
	
	DeviceMt device1;
	if(device1.open(PATH_CAMERA_1)) {
		// Events
		device1.onFrame([&](const Gb::Frame& frame){
			for(auto& client: server.getClients()) {
				if(client.connected) {
					server.sendData(client, Message(Message::DEVICE_1, reinterpret_cast<const char*>(frame.start()), frame.length()));
				}
			}
		});
	}
	
	
	// -------- Main loop --------	
	for(Timer timer; Globals::signalStatus != SIGINT; timer.wait(100)) {
		/* ... Do other stuff ... */
	}
		
	// -- End
	device0.release();
	device1.release();
	server.disconnect();
	
	std::cout << "Clean exit" << std::endl;
	std::cout << "Press a key to continue..." << std::endl;
	return std::cin.get();
}
