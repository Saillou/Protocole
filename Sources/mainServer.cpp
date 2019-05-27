#include <iostream>
#include <csignal>

#include "StreamDevice/ServerDevice.hpp"
#include "Tool/Timer.hpp"

#ifdef __linux__
	#define PATH_CAMERA_0 "/dev/video0"
	#define PATH_CAMERA_1 "/dev/video1"

#elif _WIN32
	#define PATH_CAMERA_0 "0"
	#define PATH_CAMERA_1 "1"
	
#endif

namespace Globals {
	// Constantes
	const std::string PATH_0 = PATH_CAMERA_0;
	const std::string PATH_1 = PATH_CAMERA_1;
	
	// Variables
	volatile std::sig_atomic_t signalStatus = 0;
}

// --- Signals ---
static void sigintHandler(int signal) {
	Globals::signalStatus = signal;
}

// --- Functions  ---
bool waitServer(Server& server) {
	while(Globals::signalStatus != SIGINT && !server.connectAt(8888)) {
		std::cout << "Can create server" << std::endl;
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
		std::cout << "Info received from client_" << client.id() << ": [Code:" << message.code() << "] " << message.str() << std::endl;
		server.sendInfo(client, Message("Pong"));
	});
	
	server.onData([&](const Server::ClientInfo& client, const Message& message) {
		std::cout << "Data received from client_" << client.id() << ": [Code:" << message.code() << "] " << message.str() << std::endl;
		server.sendData(client, Message("Pong"));
	});
	
	// -- Create server --
	if(!waitServer(server)) {
		std::cout << "Press a key to continue..." << std::endl;
		return std::cin.get();
	}

	std::string messageStr(60000, 'A');
	
	for(Timer timer; Globals::signalStatus != SIGINT; timer.wait(20)) {
		// Spam broadcast
		for(auto& client: server.getClients()) {
			if(client.connected) {
				server.sendData(client, Message(messageStr));
			}
		}
	}
	
	// -- End
	server.disconnect();
	
	std::cout << "Clean exit" << std::endl;
	std::cout << "Press a key to continue..." << std::endl;
	return std::cin.get();
}
