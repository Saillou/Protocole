#include <iostream>
#include <csignal>
#include <conio.h>

#include "Client.hpp"
#include "Timer.hpp"
#include "Message.hpp"

namespace Globals {
	// Constantes
	const std::string IP_ADDRESS = "127.0.0.1";
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
	
	//-- Connect client
	Client client;
	client.connectTo(Globals::IP_ADDRESS, Globals::PORT);
	
	// -------- Callbacks --------
	client.onConnect([&]() {
		std::cout << "Connection to server success" << std::endl;
		
		Message message("begin");
		client.sendInfo(message);
	});
	
	client.onData([&](const Message& message) {
		std::cout << "Data received: [Code:" << message.code() << "] " << message.str() << std::endl;
	});
	
	client.onInfo([&](const Message& message) {
		std::cout << "Info received: [Code:" << message.code() << "] " << message.str() << std::endl;
	});
	
	client.onError([&](const Error& error) {
		std::cout << "Error: " << error.msg() << std::endl;
	});
	
	
	// -------- Main loop --------
	for(Timer timer; Globals::signalStatus != SIGINT; timer.wait(100)) {
		/* ... Do stuff ... */
	}
		
	// -- End
	client.disconnect();
	
	std::cout << "Clean exit" << std::endl;
	std::cout << "Press a key to continue " << std::endl;
	return _getch();
}
