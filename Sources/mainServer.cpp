#include <iostream>
#include <csignal>
#include <atomic>
#include <map>

#include "Device/DeviceMt.hpp"
#include "Network/Server.hpp"
#include "Network/Message.hpp"
#include "Timer.hpp"

#ifdef __linux__
    #define PATH_CAMERA_0 "/dev/video0"
    #define PATH_CAMERA_1 "/dev/video1"

#elif _WIN32
    #define PATH_CAMERA_0 "0"
    #define PATH_CAMERA_1 "1"
    
#endif

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

// --- Entry point ---
int main() {
    // -- Install signal handler
    std::signal(SIGINT, sigintHandler);
    
    // std::vector<char> buffer((int)(1e5), 'A');
    // Message bigMessage(Message::TEXT, buffer.data(), buffer.size());
    std::map<SOCKET, bool> clientsOrder;
    
    // -- Create server --
    Server server;
    server.connectAt(Globals::PORT);
    
    server.onClientConnect([&](const Server::ClientInfo& client) {
        std::cout << "New client, client_" << client.id << std::endl;
        clientsOrder[client.id] = false;
    });
    server.onClientDisconnect([&](const Server::ClientInfo& client) {
        std::cout << "Client quit, client_" << client.id << std::endl;
        clientsOrder[client.id] = false;
    });
    server.onError([&](const Error& error) {
        std::cout << "Error : " << error.msg() << std::endl;
    });
    
    server.onInfo([&](const Server::ClientInfo& client, const Message& message) {
        std::cout << "Info received from client_" << client.id << ": [Code:" << message.code() << "] " << message.str() << std::endl;
        if(message.str() == "Send") {
            clientsOrder[client.id] = true;
            // std::cout << "Send: " << bigMessage.size()  << "bytes" << std::endl;
            // server.sendData(client, bigMessage);
        }
    });
    server.onData([&](const Server::ClientInfo& client, const Message& message) {
        std::cout << "Data received from client_" << client.id << ": [Code:" << message.code() << "] " << message.str() << std::endl;
    });
    
    
    // -- Open devices --
    DeviceMt device0;
    if(device0.open(PATH_CAMERA_0)) {
        // Events
        device0.onFrame([&](const Gb::Frame& frame) {
            for(auto& client: server.getClients()) {
                if(client.connected && clientsOrder[client.id]) {
                    server.sendData(client, Message(Message::DEVICE_0, reinterpret_cast<const char*>(frame.start()), frame.length()));
                }
            }
        });
    }
    
    DeviceMt device1;
    if(device1.open(PATH_CAMERA_1)) {
        // Events
        device1.onFrame([&](const Gb::Frame& frame){
            for(auto& client: server.getClients()) {
                if(client.connected  && clientsOrder[client.id]) {
                    server.sendData(client, Message(Message::DEVICE_1, reinterpret_cast<const char*>(frame.start()), frame.length()));
                }
            }
        });
    }
    
    
    // -------- Main loop --------  
    for(Timer timer; Globals::signalStatus != SIGINT; timer.wait(100)) {
        /* ... Do other stuff ... */
    }
        
    // -- End
    device0.release();
    device1.release();
    server.disconnect();
    
    std::cout << "Clean exit" << std::endl;
    std::cout << "Press a key to continue..." << std::endl;
    return std::cin.get();
}
