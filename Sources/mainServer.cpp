#include <iostream>
#include <csignal>
#include <atomic>
#include <vector>

#include "Device/DeviceMt.hpp"
#include "Network/Server.hpp"
#include "Network/Message.hpp"
#include "Timer.hpp"

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

// --- Functions  ---
bool waitServer(Server& server) {
	while(Globals::signalStatus != SIGINT && !server.connectAt(Globals::PORT)) {
		std::cout << "Can create server" << std::endl;
		Timer::wait(1000);
	}
	
	if( Globals::signalStatus == SIGINT) 
		return false;
	
	return true;
}


// --- Entry point ---
int main(int argc, char* argv[]) {
	// -- Install signal handler
	std::signal(SIGINT, sigintHandler);
	
	// Variables
	Server server;
	
	// -- Events
	server.onClientConnect([&](const Server::ClientInfo& client) {
		std::cout << "New client, client_" << client.id() << std::endl;
	});
	server.onClientDisconnect([&](const Server::ClientInfo& client) {
		std::cout << "Client quit, client_" << client.id() << std::endl;
	});
	server.onError([&](const Error& error) {
		std::cout << "Error : " << error.msg() << " code: " << error.code() << std::endl;
	});
	
	server.onInfo([&](const Server::ClientInfo& client, const Message& message) {
		// std::cout << "Info received from client_" << client.id() << ": [Code:" << message.code() << "] " << message.str() << std::endl;
		// server.sendInfo(client, Message(message.str()));
	});
	
	server.onData([&](const Server::ClientInfo& client, const Message& message) {
		// std::cout << "Data received from client_" << client.id() << ": [Code:" << message.code() << "] " << message.str() << std::endl;
		server.sendData(client, Message("Pong"));
		// std::cout << "Udp ping: " << clock()<< " ms. \n";
	});
	
	// -- Create server --
	if(!waitServer(server)) {
		std::cout << "Press a key to continue..." << std::endl;
		return std::cin.get();
	}
	
	// -------- Main loop --------  
	for(Timer timer; Globals::signalStatus != SIGINT; timer.wait(100)) {
		// ... Do other stuff ...
	}
	
	// -- End
	server.disconnect();
	
	std::cout << "Clean exit" << std::endl;
	std::cout << "Press a key to continue..." << std::endl;
	return std::cin.get();
}
