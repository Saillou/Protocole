#include <iostream>
#include <csignal>
#include <atomic>

#include "Server.hpp"
#include "Timer.hpp"
#include "Message.hpp"

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
	
	//-- Create server
	Server server;
	server.connectAt(Globals::PORT);
	
	if(!server.isConnected())
		return 1;
	
	// -------- Callbacks --------	
	server.onClientConnect([&](const Server::ClientInfo& client) {
		std::cout << "New client, client_" << client.id << std::endl;
	});
	server.onClientDisconnect([&](const Server::ClientInfo& client) {
		std::cout << "Client quit, client_" << client.id << std::endl;
	});
	server.onError([&](const Error& error) {
		std::cout << "Error : " << error.msg() << std::endl;
	});
	
	server.onInfo([&](const Server::ClientInfo& client, const Message& message) {
		std::cout << "Info received from client_" << client.id << ": [Code:" << message.code() << "] " << message.str() << std::endl;
		if(message.str() == "begin")
			Globals::beginSend = true;
	});
	server.onData([&](const Server::ClientInfo& client, const Message& message) {
		std::cout << "Data received from client_" << client.id << ": [Code:" << message.code() << "] " << message.str() << std::endl;
	});
	
	
	// -------- Main loop --------
	Message bigMessage("Hello world!");
	
	for(Timer timer; Globals::signalStatus != SIGINT; timer.wait(1)) {
		// /* ... Do stuff ... */
		static int i = 0;
		if(Globals::beginSend && i <= 0) {
			for(auto& client : server.getClients()) {
				server.sendData(client, bigMessage);
			}
			i++;
		}
	}
		
	// -- End
	server.disconnect();
	
	std::cout << "Clean exit" << std::endl;
	std::cout << "Press a key to continue..." << std::endl;
	return std::cin.get();
}
