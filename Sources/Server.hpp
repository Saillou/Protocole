#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <algorithm>
#include <functional>

#include "WinLinConversion.hpp"
#include "Timer.hpp"
#include "Message.hpp"

class Server {
	// -------------- Nested struct --------------
public:
	struct ClientInfo {
		SOCKET id = INVALID_SOCKET;
		clock_t lastUpdate = 0;		
		bool connected = false;
		sockaddr_in tcpAddress;
		sockaddr_in udpAddress;
	};
	
private:
	class ConnectedClient {
	public:
		ConnectedClient(ClientInfo clientInfo) : info(clientInfo) {
			
		}
		ConnectedClient(const ConnectedClient& cc) {
			info = cc.info;
		}
		~ConnectedClient() {
			killThread();
		}
		
		void killThread() {
			if(pThread) {
				if(pThread->joinable()) {
					pThread->join();
				}
				
				pThread.reset();
			}
		}
		void disconnect() {	
			info.connected = false;
			wlc::closeSocket(info.id);
		}
		
		ClientInfo info;
		std::shared_ptr<std::thread> pThread;
	};
	
	
	
	// -------------- Main class --------------
public:
	Server() : _isConnected(false), _udpSock(INVALID_SOCKET), _tcpSock(INVALID_SOCKET) { 
		// Wait for connectAt()
	}
	~Server() {
		disconnect();
	}
	
	// Methods
	void disconnect() {
		_isConnected = false;
		
		// Server disconnecting .. Send something ?		
		if(_pRecvUdp && _pRecvUdp->joinable())
			_pRecvUdp->join();

		if(_pHandleTcp && _pHandleTcp->joinable())
			_pHandleTcp->join();

		wlc::closeSocket(_udpSock);
		wlc::closeSocket(_tcpSock);
		
		// After tcp has joined : no client will be accepted, and no clients will be deleted.
		// Therefore, just wait for the threads to finish and then delete it. (Avoid mutex deadlock)
		for(auto& client : _clients) {
			client.killThread();
			client.disconnect();
		}
		_clients.clear();
		
		wlc::uninitSockets();
	}
	
	void connectAt(const int port) {
		if(_isConnected)
			return;
		
		// Init sockets
		if(!wlc::initSockets())
			return;
		
		// Create server address
		_address.sin_family 		= AF_INET;
		_address.sin_port 			= htons(port);
		_address.sin_addr.s_addr	= INADDR_ANY;
		
		// Create server sockets
		_udpSock = socket(PF_INET, SOCK_DGRAM , IPPROTO_UDP);
		if(_udpSock == INVALID_SOCKET)
			return disconnect();
			
		_tcpSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		if(_tcpSock == INVALID_SOCKET)
			return disconnect();
			
		// Bind sockets to address
		int addrSize = sizeof(_address);
		if(bind(_udpSock, (sockaddr *)&_address, addrSize) == SOCKET_ERROR)
			return disconnect();
		
		if(bind(_tcpSock, (sockaddr *)&_address, addrSize) == SOCKET_ERROR)
			return disconnect();
		
		
		// Options
		if(wlc::setNonBlocking(_udpSock, true) < 0)
			return disconnect();
		
		if(wlc::setNonBlocking(_tcpSock, true) < 0)
			return disconnect();		
		
		// Create threads
		_isConnected = true;
		
		_pRecvUdp 	= std::make_shared<std::thread>(&Server::_recvUdp, this);
		_pHandleTcp = std::make_shared<std::thread>(&Server::_handleTcp, this);
	}
	
	// Send message with UDP
	void sendData(const ClientInfo& client, const Message& msg) const {
		if(sendto(_udpSock, msg.data(), (int)msg.length(), 0, (sockaddr*) &client.udpAddress, sizeof(client.udpAddress)) != (int)msg.length()) {
			std::lock_guard<std::mutex> lockCbk(_mutCbk);
			if(_cbkError) 
				_cbkError(Error(wlc::getError(), "UDP send Error"));
		}
	}
	
	// Send message with TCP
	void sendInfo(const ClientInfo& client, const Message& msg) const {
		if(send(client.id, msg.data(), (int)msg.length(), 0) != (int)msg.length()) {
			std::lock_guard<std::mutex> lockCbk(_mutCbk);
			if(_cbkError) 
				_cbkError(Error(wlc::getError(), "TCP send Error"));
		}
	}
	
	// Getters
	bool isConnected() const {
		return _isConnected;
	}
	std::vector<ClientInfo> getClients() const {
		std::vector<ClientInfo> clients;
	
		_mutClients.lock();
		for(const ConnectedClient& cc : _clients)
			clients.push_back(cc.info);
		
		_mutClients.unlock();
		
		return clients;
	}
	
	// Setters
	void onClientConnect(const std::function<void(const ClientInfo& client)>& cbkConnect) {
		std::lock_guard<std::mutex> lockCbk(_mutCbk);
		_cbkConnect = cbkConnect;
	}
	void onClientDisconnect(const std::function<void(const ClientInfo& client)>& cbkDisconnect) {
		std::lock_guard<std::mutex> lockCbk(_mutCbk);
		_cbkDisconnect = cbkDisconnect;
	}
	void onInfo(const std::function<void(const ClientInfo& client, const Message& message)>& cbkInfo) {
		std::lock_guard<std::mutex> lockCbk(_mutCbk);
		_cbkInfo = cbkInfo;	
	}
	void onData(const std::function<void(const ClientInfo& client, const Message& message)>& ckbData) {
		std::lock_guard<std::mutex> lockCbk(_mutCbk);
		_cbkData = ckbData;		
	}
	void onError(const std::function<void(const Error& error)>& cbkError) {
		std::lock_guard<std::mutex> lockCbk(_mutCbk);
		_cbkError = cbkError;		
	}
	
	
private:	
	// Methods in threads
	void _handleTcp() {
		// Listen
		if(!_isConnected || listen(_tcpSock, SOMAXCONN) == SOCKET_ERROR)
			return;
		
		// Accept new clients
		ClientInfo clientInfo;
		int slen(0);
		
		// Loop accept() until the server is stopped
		for(Timer timer; _isConnected; timer.wait(100)) {
			slen = sizeof(clientInfo.tcpAddress);
			clientInfo.id 	= accept(_tcpSock, (sockaddr*)&clientInfo.tcpAddress, &slen);
			
			if(clientInfo.id != SOCKET_ERROR) {
				// Update infos
				clientInfo.lastUpdate = clock();
				memset(&clientInfo.udpAddress, 0, sizeof(clientInfo.udpAddress)); 
				
				std::lock_guard<std::mutex> lockCbk(_mutClients);		
				_clients.push_back(ConnectedClient(clientInfo)); // Add to list
				
				ConnectedClient& client(_clients.back());																	// Need reference to the new client
				sendInfo(client.info, Message(Message::HANDSHAKE, "udp?"));											// Ask for its udp address
				client.pThread = std::make_shared<std::thread>(&Server::_clientTcp, this, client.info); 	// Start its thread
			}
			else {
				// Use this time to collect garbage
				if(!_garbageItClients.empty()) {
					std::lock_guard<std::mutex> lockCbk(_mutClients);
					for(auto& it : _garbageItClients) 
						_clients.erase(it);
					
					_garbageItClients.clear();
				}
			}
		}
	}
	
	void _clientTcp(ClientInfo& client) {
		// Loop Read 
		const int BUFFER_SIZE = 2048;
		char buf[BUFFER_SIZE] = {0};
		int recv_len = 0;
		
		for(Timer timer; _isConnected; ) {
			// Receive
			memset(buf, 0, BUFFER_SIZE);
			if((recv_len = recv(client.id, buf, BUFFER_SIZE, 0)) == SOCKET_ERROR) {
				// What kind of error ?
				int error = wlc::getError();
				if(error == WSAEWOULDBLOCK) { // Temporarily unavailable
					timer.wait(100);
					continue;
				}
				else if(error == WSAECONNRESET) { // Forcibly close
					break;
				}
				else {					
					std::lock_guard<std::mutex> lockCbk(_mutCbk);
					if(_cbkError) 
						_cbkError(Error(wlc::getError(), "TCP receive Error"));
					break;
				}
			}
			
			// Stopped connection
			if(recv_len == 0)
				break;
			
			// Update client
			_mutClients.lock();
			client.lastUpdate = clock();
			_mutClients.unlock();
			
			// Read messages
			if(recv_len < 8) // Bad message
				continue;
			
			for(const Message& message : MessageManager::readMessages(buf, recv_len)) {
				std::lock_guard<std::mutex> lockCbk(_mutCbk);
				if(_cbkInfo) 
					_cbkInfo(client, message);
			}
		}
		
		// End
		std::lock_guard<std::mutex> lockCbk(_mutCbk);
		if(_cbkDisconnect) 
			_cbkDisconnect(client);	
		
		std::lock_guard<std::mutex> lockClients(_mutClients);
		std::vector<ConnectedClient>::iterator itClient = _findClientFromAddress(client.tcpAddress);
		
		if(itClient != _clients.end()) {
			itClient->disconnect();
			_garbageItClients.push_back(itClient);
		}
	}
	
	void _recvUdp() {
		const int BUFFER_SIZE = 64000;
		char buf[BUFFER_SIZE] = {0};
		sockaddr_in clientAddress;
		int send_len = 0, recv_len = 0, slen = sizeof(clientAddress);
		
		for(Timer timer; _isConnected; ) {
			memset(buf, 0, BUFFER_SIZE);

			// Receive
			clock_t time = clock();
			if ((recv_len = recvfrom(_udpSock, buf, BUFFER_SIZE, 0, (sockaddr *) &clientAddress, &slen)) == SOCKET_ERROR) {
				// What kind of error ?
				int error = wlc::getError();
				if(error == WSAEWOULDBLOCK || error == WSAENOTCONN) {// Timeout || Waiting for connection
					timer.wait(100);
					continue; 
				}
				else if(error == WSAECONNRESET) { // Forcibly close
					break;
				}
				else {					
					std::lock_guard<std::mutex> lockCbk(_mutCbk);
					if(_cbkError) 
						_cbkError(Error(wlc::getError(), "UDP receive Error"));
					
					timer.wait(100);
					continue;
				}
			}
			if(recv_len == 0) 
				continue;
			
			// Read message
			if(recv_len < 8) // Bad message
				continue;
			Message message(buf, recv_len);
			
			// Update list
			std::lock_guard<std::mutex> lockMut(_mutClients); // Free mutex when scope end
			
			std::vector<ConnectedClient>::iterator itClient = _findClientFromAddress(clientAddress);
			if(itClient != _clients.end()) {
				itClient->info.lastUpdate = time;
				
				// First time ?
				if(!itClient->info.connected) {
					if(message.code() == Message::HANDSHAKE && message.str() == "udp.") {
						itClient->info.connected = true;
						itClient->info.udpAddress = clientAddress;
						
						sendInfo(itClient->info, Message(Message::HANDSHAKE, "ok."));	
						
						std::lock_guard<std::mutex> lockCbk(_mutCbk);
						if(_cbkConnect) 
							_cbkConnect(itClient->info);
					}
					else { // Shakehand error
						std::lock_guard<std::mutex> lockCbk(_mutCbk);
						if(_cbkError) 
							_cbkError(Error(Error::BAD_CONNECTION, "Handshake Error"));
					}
				}
				else { // Read data message
					std::lock_guard<std::mutex> lockCbk(_mutCbk);
					if(_cbkData) 
						_cbkData(itClient->info, message);
				}
			}			
			
		}
	}
	
	// Search in the list. Not thread safe - Please use mutex before calling.
	std::vector<ConnectedClient>::iterator _findClientFromAddress(const sockaddr_in& address) {		
		for(std::vector<ConnectedClient>::iterator itClient = _clients.begin(); itClient != _clients.end(); ++itClient) 
			if(itClient->info.tcpAddress.sin_addr.s_addr == address.sin_addr.s_addr) 
				return itClient;
				
		return _clients.end(); // If not found
	}
	
private:
	// Members
	std::atomic<bool> _isConnected;
	
	SOCKET _udpSock;
	SOCKET _tcpSock;
	sockaddr_in _address;
	
	// Callbacks
	mutable std::mutex _mutCbk;
	std::function<void(const Error& error)> _cbkError;
	std::function<void(const ClientInfo& client, const Message& message)> _cbkInfo;
	std::function<void(const ClientInfo& client, const Message& message)> _cbkData;
	std::function<void(const ClientInfo& client)> _cbkConnect;
	std::function<void(const ClientInfo& client)> _cbkDisconnect;
	
	// Threads
	std::shared_ptr<std::thread> _pHandleTcp;
	std::shared_ptr<std::thread> _pRecvUdp;
	
	// Clients
	mutable std::mutex _mutClients;
	std::vector<ConnectedClient> _clients;
	std::vector<std::vector<ConnectedClient>::iterator> _garbageItClients;
};

