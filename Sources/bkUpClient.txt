#include <iostream>
#include <csignal>
#include <atomic>
#include <map>
#include <deque>

#include "StreamDevice/ClientDevice.hpp"
#include "Tool/Timer.hpp"
#include "Tool/FrameMt.hpp"

namespace Globals {
	// Constantes
	const std::string IP_ADDRESS = "192.168.11.24"; 	// Barnacle V4
	// const std::string IP_ADDRESS = "127.0.0.1"; 	// localhost V4
	
	// const std::string IP_ADDRESS = "fe80::b18:f81d:13a8:3a4"; 		// Barnacle V6
	// const std::string IP_ADDRESS = "::1"; 									// localhost V6
	
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
	
	// --- Devices
	FrameMt frame0;
	ClientDevice device0(IAddress(Globals::IP_ADDRESS, 5000));
	
	FrameMt frame1;
	ClientDevice device1(IAddress(Globals::IP_ADDRESS, 6000));
	
	
	// ----- Events -----
	device0.onFrame([&](const Gb::Frame& frame) {
		frame0.setFrame(frame);
	});
	device1.onFrame([&](const Gb::Frame& frame) {
		frame1.setFrame(frame);
	});
	
	
	// -------- Main loop --------  
	if(!device0.open() || !device1.open()) {
		std::cout << "Couldn't open device" << std::endl;
		std::cout << "Press a key to continue..." << std::endl;
		return std::cin.get();
	}
	
	// Alloc decoder
	tjhandle decoder = tjInitDecompress();
	
	// Loop
	while(Globals::signalStatus != SIGINT && cv::waitKey(10) != 27) {
		if(frame0.decode(decoder))
			frame0.show("Device 0");
		
		if(frame1.decode(decoder))
			frame1.show("Device 1");
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
