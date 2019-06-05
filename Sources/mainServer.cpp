#include <iostream>
#include <csignal>

#include "Device/Device.hpp"
#include "Tool/Timer.hpp"

namespace Globals {
	// Variables
	volatile std::sig_atomic_t signalStatus = 0;
}

// --- Signals ---
static void sigintHandler(int signal) {
	Globals::signalStatus = signal;
}

int echo(const std::string& msg) {
	Timer::wait(100);
	std::cout << msg << std::endl;
	std::cout << "Press a key to continue..." << std::endl;
	return std::cin.get();
}

// --- Entry point ---
int main(int argc, char* argv[]) {	
	// - Install signal handler
	std::signal(SIGINT, sigintHandler);
	
	Device device("0");
	device.open();
	
	for(Timer timer; Globals::signalStatus != SIGINT; timer.wait(100)) {
		// ... Do other stuff ...
	}
	
	device.close();
	// End
	
	return echo("Success");
}