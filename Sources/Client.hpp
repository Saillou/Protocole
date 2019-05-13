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

#ifdef __linux__ 
	#include <sys/select.h>
	#include <sys/socket.h>
	#include <sys/types.h>
	#include <netinet/in.h>	
	#include <arpa/inet.h>
	#include <fcntl.h>
	
#elif _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
	
#endif

#include "Timer.hpp"
#include "Message.hpp"

class Client {
	// -------------- Main class --------------
public:
	Client() : _isConnected(false), _isAlive(false), _udpSock(INVALID_SOCKET), _tcpSock(INVALID_SOCKET) {
		// Wait for connectTo
	}
	~Client() {
		disconnect();
	}
	
	
	// Methods
	void connectTo(const std::string& ipAddress, const int port) {		
		if(_isConnected)
			return;
		
		// Init windows sockets
		WSADATA wsa;
		if (WSAStartup(MAKEWORD(2,2), &wsa) != 0)
			return;
		
		// Create address
		memset((char *) &_address, 0, sizeof(_address));
		
		_address.sin_family	= AF_INET;
		_address.sin_port 	= htons(port);
		InetPton(AF_INET, ipAddress.c_str(), &_address.sin_addr.s_addr);
		
		// Create sockets
		_udpSock = socket(PF_INET, SOCK_DGRAM , IPPROTO_UDP);
		if(_udpSock == INVALID_SOCKET)
			return disconnect();

		_tcpSock = socket(PF_INET, SOCK_STREAM , IPPROTO_TCP);
		if(_tcpSock == INVALID_SOCKET)
			return disconnect();
		
		// Connect TCP
		if(connect(_tcpSock, (sockaddr *)&_address, sizeof(_address)) == SOCKET_ERROR)
			return disconnect();
		
		// Options
		unsigned long socketMode = 1; // 0 blocking is enabled, !0 otherwise
		if(ioctlsocket(_tcpSock, FIONBIO, &socketMode) != NO_ERROR) 
			return disconnect();
		
		if(ioctlsocket(_udpSock, FIONBIO, &socketMode) != NO_ERROR)
			return disconnect();

		// Thread
		_isAlive = true;
		_pRecvTcp = std::make_shared<std::thread>(&Client::_recvTcp, this);
		_pRecvUdp = std::make_shared<std::thread>(&Client::_recvUdp, this);
	}
	
	void disconnect() {
		_isConnected = false;
		_isAlive = false;
		
		if(_pRecvTcp)
			if(_pRecvTcp->joinable())
				_pRecvTcp->join();
			
		if(_pRecvUdp)
			if(_pRecvUdp->joinable())
				_pRecvUdp->join();
		
		closesocket(_tcpSock);
		closesocket(_udpSock);
		
		WSACleanup();	
	}
	
	void sendInfo(const Message& msg) const {
		if(send(_tcpSock, msg.data(), (int)msg.length(), 0) != (int)msg.length()) {
			std::lock_guard<std::mutex> lockCbk(_mutCbk);
			if(_cbkError) 
				_cbkError(Error(WSAGetLastError(), "TCP send Error"));
		}
	}
	
	void sendData(const Message& msg) const {		
		if(sendto(_udpSock, msg.data(), (int)msg.length(), 0, (sockaddr*) &_address, sizeof(_address)) != (int)msg.length()) {
			std::lock_guard<std::mutex> lockCbk(_mutCbk);
			if(_cbkError) 
				_cbkError(Error(WSAGetLastError(), "UDP send Error"));
		}
	}
	
	// Getters
	bool isConnected() const {
		return _isConnected;
	}
	
	// Setters
	void onConnect(const std::function<void(void)>& cbkConnect) {
		std::lock_guard<std::mutex> lockCbk(_mutCbk);
		_cbkConnect = cbkConnect;
	}
	void onInfo(const std::function<void(const Message& message)>& cbkInfo) {
		std::lock_guard<std::mutex> lockCbk(_mutCbk);
		_cbkInfo = cbkInfo;	
	}
	void onData(const std::function<void(const Message& message)>& ckbData) {
		std::lock_guard<std::mutex> lockCbk(_mutCbk);
		_cbkData = ckbData;		
	}
	void onError(const std::function<void(const Error& error)>& cbkError) {
		std::lock_guard<std::mutex> lockCbk(_mutCbk);
		_cbkError = cbkError;		
	}
	
private:	
	// Methods in threads
	void _recvTcp() {
		const int BUFFER_SIZE = 2048;
		char buf[BUFFER_SIZE] = {0};
		int recv_len = 0;
		
		for(Timer timer; _isAlive; ) {
			// Receive TCP maybe
			memset(buf, 0, BUFFER_SIZE);
			
			if((recv_len = recv(_tcpSock, buf, BUFFER_SIZE, 0)) == SOCKET_ERROR) {
				// What kind of error ?
				int error = WSAGetLastError();
				if(error == WSAEWOULDBLOCK) { // Temporary unavailable
					timer.wait(100);
					continue;
				}
				else if(error == WSAECONNRESET) { // Server connection forcibly closed
					break;
				}
				else {
					std::lock_guard<std::mutex> lockCbk(_mutCbk);
					if(_cbkError) 
						_cbkError(Error(WSAGetLastError(), "TCP receive Error"));
					break;
				}
			}
			
			if(recv_len == 0) {
				std::lock_guard<std::mutex> lockCbk(_mutCbk);
				if(_cbkError) 
					_cbkError(Error(WSAGetLastError(), "Server disconnected"));
				break;
			}
			
			// Read messages
			if(recv_len < 8) // Bad message
				continue;
			
			for(const Message& message :  MessageManager::readMessages(buf, recv_len)) {
				if(!_isConnected) {
					if(message.code() == Message::HANDSHAKE) {
						std::string strMessage = message.str();
						
						if(strMessage == "udp?") { 		// UDP needed ?
							sendData(Message(Message::HANDSHAKE, "udp."));
						}
						else if(strMessage == "ok.") {	// Handshake complete
							_isConnected = true;
							
							std::lock_guard<std::mutex> lockCbk(_mutCbk);
							if(_cbkConnect) 
								_cbkConnect();
						}
					}
				}
				else {
					std::lock_guard<std::mutex> lockCbk(_mutCbk);
					if(_cbkInfo) 
						_cbkInfo(message);
				}
			} // -- End messages
		} // -- End loop
	} // -- End function recv tcp
	
	void _recvUdp() {
		sockaddr_in serverAddress;
		const int BUFFER_SIZE = 64000;
		char buf[BUFFER_SIZE] = {0};
		int recv_len 	= 0;
		int slen 		= (int)sizeof(serverAddress);
		
		for(Timer timer; _isAlive; ) {	
			// UDP - Receive
			memset(buf, 0, BUFFER_SIZE);
			
			if ((recv_len = recvfrom(_udpSock, buf, BUFFER_SIZE, 0, (sockaddr *)&serverAddress, &slen)) == SOCKET_ERROR) {			
				// What kind of error ?
				int error = WSAGetLastError();
				if(error == WSAEWOULDBLOCK || error == WSAEINVAL) {
					timer.wait(100);
					continue;
				}
				else if(error == WSAECONNRESET) {
					break;
				}
				else if(error == WSAEMSGSIZE) { // Message too big
					continue;
				}
				else {
					std::lock_guard<std::mutex> lockCbk(_mutCbk);
					if(_cbkError) 
						_cbkError(Error(WSAGetLastError(), "UDP receive Error"));
					break;
				}
			}
			
			// Read message
			if(recv_len < 8) // Bad message
				continue;
			
			Message message(buf, recv_len);
			
			std::lock_guard<std::mutex> lockCbk(_mutCbk);
			if(_cbkData) 
				_cbkData(message);
		}
		
		// Forcibly disconnected
		if(_isConnected)
			disconnect();
	}

private:
	// Members
	std::atomic<bool> _isConnected;
	std::atomic<bool> _isAlive;		// Control threads
	
	SOCKET _udpSock;
	SOCKET _tcpSock;
	sockaddr_in _address;
	
	// Callbacks
	mutable std::mutex _mutCbk;
	std::function<void(const Error& error)> _cbkError;
	std::function<void(const Message& message)> _cbkInfo;
	std::function<void(const Message& message)> _cbkData;
	std::function<void(void)> _cbkConnect;
	
	// Threads
	std::shared_ptr<std::thread> _pRecvTcp;
	std::shared_ptr<std::thread> _pRecvUdp;
	
};