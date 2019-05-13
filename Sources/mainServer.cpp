#include <iostream>
#include <csignal>
#include <atomic>

#include "SharedDevice.hpp"
#include "Timer.hpp"

namespace Globals {
	// Constantes
	const int PORT = 8888;
	
	// Variables
	volatile std::sig_atomic_t signalStatus = 0;
	std::atomic<bool> beginSend = {false};
}

// --- Signals ---
static void sigintHandler(int signal) {
	Globals::signalStatus = signal;
}

// --- Entry point ---
int main() {
	// -- Install signal handler
	std::signal(SIGINT, sigintHandler);
	
	// -- Create a SharedDevice --
	SharedDevice device;
	device.open(0);
	device.run(SharedDevice::EMITTER, "192.168.1.1", 8888);
	
	device.onFrame([&](const Device::Frame& frame){
		device.show(frame);
	});
	
	
	// -------- Main loop --------	
	for(Timer timer; Globals::signalStatus != SIGINT; timer.wait(100)) {
		/* ... Do other stuff ... */
	}
		
	// -- End
	device.release();
	
	std::cout << "Clean exit" << std::endl;
	std::cout << "Press a key to continue..." << std::endl;
	return std::cin.get();
}
