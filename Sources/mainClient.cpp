#include <iostream>
#include <csignal>
#include <atomic>
#include <map>
#include <deque>

#include "StreamDevice/ClientDevice.hpp"
#include "Timer.hpp"
#include "Buffers.hpp"

namespace Globals {
	// Constantes
	const std::string IP_ADDRESS = "192.168.11.24"; 	// Barnacle V4
	// const std::string IP_ADDRESS = "127.0.0.1"; 	// localhost V4
	const int PORT = 8888;										// Port v4
	
	// const std::string IP_ADDRESS = "fe80::b18:f81d:13a8:3a4"; 	// Barnacle V6
	// const std::string IP_ADDRESS = "::1"; 									// localhost V6
	// const int PORT = 8889;															// Port v6
	
	// Variables
	volatile std::sig_atomic_t signalStatus = 0;
	
	FrameMt frame0;
	FrameMt frame1;
}

// --- Signals ---
static void sigintHandler(int signal) {
	Globals::signalStatus = signal;
}

// --- Entry point ---
int main(int argc, char* argv[]) {	
	// - Install signal handler
	std::signal(SIGINT, sigintHandler);
	
	// - Devices
	ClientDevice device0(IAddress(Globals::IP_ADDRESS, 5000));
	ClientDevice device1(IAddress(Globals::IP_ADDRESS, 6000));
	
	// ----- Events -----
	device0.onFrame([&](const Gb::Frame& frame) {
		Globals::frame0.setFrame(frame);
	});
	device1.onFrame([&](const Gb::Frame& frame) {
		Globals::frame1.setFrame(frame);
	});
	
	// -------- Main loop --------  
	if(!device0.open() || !device1.open()) {
		std::cout << "Couldn't open device" << std::endl;
		std::cout << "Press a key to continue..." << std::endl;
		return std::cin.get();
	}
	
	// Alloc
	tjhandle decoder = tjInitDecompress();
	
	// Loop
	while(Globals::signalStatus != SIGINT && cv::waitKey(10) != 27) {
		if(Globals::frame0.decode(decoder))
			Globals::frame0.show("Device 0");
		
		if(Globals::frame1.decode(decoder))
			Globals::frame1.show("Device 1");
	}
	
	// -- End
	cv::destroyAllWindows();
	tjDestroy(decoder);
	
	device0.close();
	device1.close();
	
	std::cout << "Clean exit" << std::endl;
	std::cout << "Press a key to continue..." << std::endl;
	return std::cin.get();
}
