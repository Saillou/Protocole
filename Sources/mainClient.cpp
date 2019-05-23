#include <iostream>
#include <csignal>
#include <deque>

#include "Timer.hpp"
#include "Buffers.hpp"
#include "Network/Client.hpp"
#include "Network/Message.hpp"


// -- Globals space --
namespace Globals {
	// Constantes
	const std::string IP_ADDRESS = "192.168.11.24"; 	// Barnacle V4
	// const std::string IP_ADDRESS = "127.0.0.1"; 	// localhost V4
	const int PORT = 8888;										// Port v4
	
	// Variables
	volatile std::sig_atomic_t signalStatus = 0;
}

// --- Signals ---
static void sigintHandler(int signal) {
	Globals::signalStatus = signal;
}

// --- Functions  ---
bool waitClient(Client& client) {
	while(Globals::signalStatus != SIGINT && !client.connectTo(Globals::IP_ADDRESS, Globals::PORT)) {
		std::cout << "Can reach server..." << std::endl;
		Timer::wait(1000);
	}
	
	if( Globals::signalStatus == SIGINT) 
		return false;
	
	return true;
}

// --- Entry point ---
int main() {
	// -- Install signal handler
	std::signal(SIGINT, sigintHandler);
	
	//-- Create client
	Client client;
	
	// -------- Callbacks --------
	client.onConnect([&]() {
		std::cout << "Connection to server success" << std::endl;
	});
	
	client.onData([&](const Message& message) {		
		std::cout << "Datas : - code : [" << message.code() << "] - size : " << message.size()/1000.0 << "KB \n";
	});
	
	client.onInfo([&](const Message& message) {
		std::cout << "Info received: [Code:" << message.code() << "] " << message.str() << std::endl;
	});
	
	client.onError([&](const Error& error) {
		std::cout << "Error: " << error.msg() << std::endl;
	});
	
	
	// Connect client
	if(!waitClient(client)) {
		std::cout << "Press a key to continue..." << std::endl;
		return std::cin.get();
	}
	
	// -------- Main loop --------
	for(Timer timer; Globals::signalStatus != SIGINT; timer.wait(100)) {
		// .. Do something ..
	}

	// -- End
	client.disconnect();

	std::cout << "Clean exit" << std::endl;
	std::cout << "Press a key to continue..." << std::endl;
	return std::cin.get();
}
