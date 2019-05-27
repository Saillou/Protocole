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
	// const std::string IP_ADDRESS = "fe80::b18:f81d:13a8:3a4"; 	// Barnacle V6
	
	// Variables
	volatile std::sig_atomic_t signalStatus = 0;
}

// --- Signals ---
static void sigintHandler(int signal) {
	Globals::signalStatus = signal;
}

// --- Functions  ---
bool waitClient(Client& client) {
	while(Globals::signalStatus != SIGINT && !client.connectTo(Globals::IP_ADDRESS, 6000)) {
		std::cout << "Can reach server..." << std::endl;
		Timer::wait(1000);
	}
	
	if( Globals::signalStatus == SIGINT) 
		return false;
	
	return true;
}

// --- Entry point ---
int main(int argc, char* argv[]) {	
	// - Install signal handler
	std::signal(SIGINT, sigintHandler);
	
	//-- Create client
	Client client;
	std::atomic<unsigned int> totalBytesRead = 0;
	
	// -------- Callbacks --------
	client.onConnect([&]() {
		std::cout << "Connection to server success" << std::endl;
		
		client.sendData(Message("init data"));
		client.sendInfo(Message("init info"));
	});
	
	client.onData([&](const Message& message) {		
		// std::cout << "Datas : - code : [" << message.code() << "] - size : " << message.size()/1000.0 << "KB \n";
		totalBytesRead += 8*message.size();
	});
	
	client.onInfo([&](const Message& message) {
		// std::cout << "Infos : - code : [" << message.code() << "] - size : " << message.size()/1000.0 << "KB \n";
	});
	
	client.onError([&](const Error& error) {
		std::cout << "Error: " << error.msg() << std::endl;
	});
	
	// Connect client
	if(!waitClient(client)) {
		std::cout << "Press a key to continue..." << std::endl;
		return std::cin.get();
	}
	
	// Loop
	Timer t;
	while(Globals::signalStatus != SIGINT) {
		t.wait(1000);
		std::cout << (1000.0*totalBytesRead) / t.elapsed_mus() << "Kb/s" << " - (Total: " << totalBytesRead << "b)" << std::endl;
		totalBytesRead = 0;
		t.beg();
	}
	
	// -- End
	client.disconnect();
	
	std::cout << "Clean exit" << std::endl;
	std::cout << "Press a key to continue..." << std::endl;
	return std::cin.get();
}
