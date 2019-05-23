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
#include "SocketTool.hpp"
#include "Message.hpp"
#include "../Timer.hpp"

class Server {
	// -------------- Nested struct --------------
public:
	struct ClientInfo {
		clock_t lastUpdate = 0;		
		bool connected = false;
		
		SOCKET udpSockServerId; // <-- Server
		Socket tcpSock;	// <-- Client
		SocketAddress udpAddress;
		
		SOCKET id() const {
			return tcpSock.get();
		}
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
					std::cout << " > Wait join" << std::endl;
					pThread->join();
				}
				
				pThread.reset();
			}
		}
		void disconnect() {	
			info.connected = false;
			info.tcpSock.close();
		}
		
		ClientInfo info;
		std::shared_ptr<std::thread> pThread;
	};
	
	
	
	// -------------- Main class --------------
public:
	Server() : _isConnected(false) { 
		// Wait for connectAt()
	}
	~Server() {
		disconnect();
	}
	
	// Methods
	bool disconnect() {
		_isConnected = false;
		
		// Server disconnecting .. Send something ?		
		if(_pRecvUdp4 && _pRecvUdp4->joinable())
			_pRecvUdp4->join();
		
		if(_pRecvUdp6 && _pRecvUdp6->joinable())
			_pRecvUdp6->join();

		if(_pHandleTcp4 && _pHandleTcp4->joinable())
			_pHandleTcp4->join();
		
		if(_pHandleTcp6 && _pHandleTcp6->joinable())
			_pHandleTcp6->join();

		_udpSock4.close();
		_udpSock6.close();
		_tcpSock4.close();
		_tcpSock6.close();
		
		// After tcp has joined : no client will be accepted, and no clients will be deleted.
		// Therefore, just wait for the threads to finish and then delete it. (Avoid mutex deadlock)
		std::cout << "Clients killing" << std::endl;
		for(auto& client : _clients) {
			std::cout << " > Thread killing" << std::endl;
			client.killThread();
			std::cout << " > Disconnect" << std::endl;
			client.disconnect();
		}
		_clients.clear();
		
		wlc::uninitSockets();
		
		return false;
	}
	
	bool connectAt(const int port) {
		if(_isConnected)
			return true;
		
		// Init sockets
		if(!wlc::initSockets())
			return false;

		// Create server address
		SocketAddress address_v4;
		
		if(!address_v4.create(Ip_v4, port))
			return disconnect();
		
		// Bind server sockets
		if(!_udpSock4.bind(address_v4, Proto_Udp))
			return disconnect();
		
		if(!_tcpSock4.bind(address_v4, Proto_Tcp))
			return disconnect();		
		
		// Create server address
		SocketAddress address_v6;
		
		if(!address_v6.create(Ip_v6, port+1))
			return disconnect();
		
		// Bind server sockets
		if(!_udpSock6.bind(address_v6, Proto_Udp))
			return disconnect();
		
		if(!_tcpSock6.bind(address_v6, Proto_Tcp))
			return disconnect();	
		
		// Create threads
		_isConnected = true;
		
		_pRecvUdp4 	= std::make_shared<std::thread>(&Server::_recvUdp, this, std::ref(_udpSock4));
		_pRecvUdp6 	= std::make_shared<std::thread>(&Server::_recvUdp, this, std::ref(_udpSock6));
		_pHandleTcp4 = std::make_shared<std::thread>(&Server::_handleTcp, this, std::ref(_tcpSock4));
		_pHandleTcp6 = std::make_shared<std::thread>(&Server::_handleTcp, this, std::ref(_tcpSock6));
		
		return true;
	}
	
	// Send message with UDP
	void sendData(const ClientInfo& client, const Message& msg) const {
		std::lock_guard<std::mutex> lockCbk(_mutSendData);
		const Socket& udpSock = client.udpSockServerId == _udpSock4.get() ? _udpSock4 : _udpSock6;
		
		if(!udpSock.sendTo(msg, client.udpAddress)) {
			std::lock_guard<std::mutex> lockCbk(_mutCbk);
			if(_cbkError) 
				_cbkError(Error(wlc::getError(), "UDP send Error"));			
		}
	}
	
	// Send message with TCP
	void sendInfo(const ClientInfo& client, const Message& msg) const {
		if(send(client.tcpSock.get(), msg.data(), (int)msg.length(), 0) != (int)msg.length()) {
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
	void _handleTcp(Socket& tcpSock) {
		// Listen
		if(!_isConnected || listen(tcpSock.get(), SOMAXCONN) == SOCKET_ERROR)
			return;

		// Accept new clients
		ClientInfo clientInfo;
		
		if(tcpSock.type() == Ip_v4)
			clientInfo.udpSockServerId = _udpSock4.get();
		else if(tcpSock.type() == Ip_v6)
			clientInfo.udpSockServerId = _udpSock6.get();
		
		// Loop accept() until the server is stopped
		for(Timer timer; _isConnected; timer.wait(100)) {
			if(tcpSock.accept(clientInfo.tcpSock)) {
				// Update infos
				clientInfo.lastUpdate = clock();
				clientInfo.udpAddress.memset(0);
				
				std::lock_guard<std::mutex> lockCbk(_mutClients);		
				_clients.push_back(ConnectedClient(clientInfo)); // Add to list
				
				ConnectedClient& client(_clients.back());																				// Need reference to the new client
				sendInfo(client.info, Message(Message::HANDSHAKE, "udp?"));														// Ask for its udp address
				client.pThread = std::make_shared<std::thread>(&Server::_clientTcp, this, std::ref(client.info)); 	// Start its thread
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
		ssize_t recv_len = 0;
		
		for(Timer timer; _isConnected; ) {
			// Receive
			memset(buf, 0, BUFFER_SIZE);
			std::cout << "recv()" << std::endl;
			if((recv_len = recv(client.tcpSock.get(), buf, BUFFER_SIZE, 0)) == SOCKET_ERROR) {
				// What kind of error ?
				int error = wlc::getError();
				if(wlc::errorIs(wlc::WOULD_BLOCK, error)) { // Temporarily unavailable
					timer.wait(100);
					continue;
				}
				else if(wlc::errorIs(wlc::REFUSED_CONNECT, error)) { // Forcibly close
					break;
				}
				else {					
					std::lock_guard<std::mutex> lockCbk(_mutCbk);
					if(_cbkError) 
						_cbkError(Error(error, "TCP receive Error"));
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
			if(recv_len < 14) // Bad message
				continue;
			
			for(const Message& message : MessageManager::readMessages(buf, recv_len)) {
				std::cout << "cbk lock()" << std::endl;
				std::lock_guard<std::mutex> lockCbk(_mutCbk);
				if(_cbkInfo) 
					_cbkInfo(client, message);
			}
		}
		std::cout << " > client thread ended" << std::endl;
		std::cout << " > client cbk lock()" << std::endl;
		// End
		std::lock_guard<std::mutex> lockCbk(_mutCbk);
		if(_cbkDisconnect) 
			_cbkDisconnect(client);	
		
		std::cout << " > client mut lock()" << std::endl;
		
		std::lock_guard<std::mutex> lockClients(_mutClients);
		std::vector<ConnectedClient>::iterator itClient = _findClientFromAddress(client.tcpSock.address());
		
		if(itClient != _clients.end()) {
			itClient->disconnect();
			_garbageItClients.push_back(itClient);
		}
	}
	
	void _recvUdp(Socket& udpSock) {
		const int BUFFER_SIZE = 2048;
		char buf[BUFFER_SIZE] = {0};
		ssize_t recv_len = 0;

		SocketAddress clientSockAddress;
		
		for(Timer timer; _isConnected; ) {
			memset(buf, 0, BUFFER_SIZE);

			// ----- Receive -----
			clock_t time = clock();
			if(!udpSock.receiveFrom(recv_len, buf, BUFFER_SIZE, clientSockAddress)) {
				// What kind of error ?
				int error = wlc::getError();
				if(wlc::errorIs(wlc::WOULD_BLOCK, error) || wlc::errorIs(wlc::NOT_CONNECT, error)) { // Timeout || Waiting for connection
					timer.wait(100);
					continue; 
				}
				else if(wlc::errorIs(wlc::REFUSED_CONNECT, error)) { // Forcibly close
					break;
				}
				else {					
					std::lock_guard<std::mutex> lockCbk(_mutCbk);
					if(_cbkError) 
						_cbkError(Error(error, "UDP receive Error"));
					
					timer.wait(100);
					continue;
				}
			}
			if(recv_len == 0) 
				continue;
			
			// ----- Read message -----
			if(recv_len < 14) // Bad message
				continue;
				
			Message message(buf, recv_len);

			// Update list
			std::lock_guard<std::mutex> lockMut(_mutClients); // Free mutex when scope end
			
			std::vector<ConnectedClient>::iterator itClient = _findClientFromAddress(clientSockAddress);
			if(itClient != _clients.end()) {
				itClient->info.lastUpdate = time;
				
				// First time ?
				if(!itClient->info.connected) {
					if(message.code() == Message::HANDSHAKE && message.str() == "udp.") {
						itClient->info.connected = true;
						itClient->info.udpAddress = clientSockAddress;
						
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
	std::vector<ConnectedClient>::iterator _findClientFromAddress(const SocketAddress& address) {			
		for(std::vector<ConnectedClient>::iterator itClient = _clients.begin(); itClient != _clients.end(); ++itClient) {
			if(SocketAddress::compareHost(itClient->info.tcpSock.address(), address)) {
				return itClient;
			}
		}
				
		return _clients.end(); // If not found
	}
	
private:
	// Members
	std::atomic<bool> _isConnected;
	
	Socket _udpSock4;
	Socket _udpSock6;
	Socket _tcpSock4;
	Socket _tcpSock6;
	
	// Callbacks
	mutable std::mutex _mutCbk;
	std::function<void(const Error& error)> _cbkError;
	std::function<void(const ClientInfo& client, const Message& message)> _cbkInfo;
	std::function<void(const ClientInfo& client, const Message& message)> _cbkData;
	std::function<void(const ClientInfo& client)> _cbkConnect;
	std::function<void(const ClientInfo& client)> _cbkDisconnect;
	
	// Threads
	std::shared_ptr<std::thread> _pHandleTcp4;
	std::shared_ptr<std::thread> _pHandleTcp6;
	std::shared_ptr<std::thread> _pRecvUdp4;
	std::shared_ptr<std::thread> _pRecvUdp6;
	
	// Clients
	mutable std::mutex _mutClients;
	std::vector<ConnectedClient> _clients;
	std::vector<std::vector<ConnectedClient>::iterator> _garbageItClients;
	
	mutable std::mutex _mutSendData;
};

