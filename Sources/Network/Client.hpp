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

class Client {
	// -------------- Main class --------------
public:
	Client() : _isConnected(false), _isAlive(false) {
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
		if(!wlc::initSockets())
			return;

		// Create address
		if(!_address.create(ipAddress, port))
			return disconnect();

		// Set sockets up
		if(!_udpSock.connect(_address, Proto_Udp)) {
			std::cout << "UDP connect: " << wlc::getError() << std::endl;
			return disconnect();
		}
		
		if(!_tcpSock.connect(_address, Proto_Tcp)) {
			std::cout << "TCP connect: " << wlc::getError() << std::endl;
			return disconnect();	
		}
		
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
		
		_udpSock.close();
		_tcpSock.close();
		
		wlc::uninitSockets();
	}
	
	void sendInfo(const Message& msg) const {
		_send(_tcpSock, msg, "TCP send Error");
	}
	
	void sendData(const Message& msg) const {		
		_send(_udpSock, msg, "UDP send Error");
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
		ssize_t recv_len = 0;
		
		for(Timer timer; _isAlive; ) {
			// Receive TCP maybe
			memset(buf, 0, BUFFER_SIZE);
			
			if((recv_len = recv(_tcpSock.get(), buf, BUFFER_SIZE, 0)) == SOCKET_ERROR) {
				// What kind of error ?
				int error = wlc::getError();
				if(wlc::errorIs(wlc::WOULD_BLOCK, error)) { // Temporary unavailable
					timer.wait(100);
					continue;
				}
				else if(wlc::errorIs(wlc::REFUSED_CONNECT, error)) { // Server connection forcibly closed
					break;
				}
				else {
					std::lock_guard<std::mutex> lockCbk(_mutCbk);
					if(_cbkError) 
						_cbkError(Error(error, "TCP receive Error"));
					break;
				}
			}
			
			if(recv_len == 0) {
				std::lock_guard<std::mutex> lockCbk(_mutCbk);
				if(_cbkError) 
					_cbkError(Error(wlc::REFUSED_CONNECT, "Server disconnected"));
				break;
			}
			
			// Read messages
			if(recv_len < 14) // Bad message
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
		const int BUFFER_SIZE	= 64000;
		char buf[BUFFER_SIZE]	= {0};
		ssize_t recv_len 	= 0;
		
		bool buffering = false;
		size_t sizeWaited = 0;
		std::vector<char> msgSerializedBuffer;
		
		for(Timer timer; _isAlive; ) {	
			// UDP - Receive
			memset(buf, 0, BUFFER_SIZE);
			
			if((recv_len = recv(_udpSock.get(), buf, BUFFER_SIZE, 0)) == SOCKET_ERROR) {		
				// What kind of error ?
				int error = wlc::getError();
				if(wlc::errorIs(wlc::WOULD_BLOCK, error) || wlc::errorIs(wlc::INVALID_ARG, error)) {
					timer.wait(buffering ? 1 : 100);
					continue;
				}
				else if(wlc::errorIs(wlc::REFUSED_CONNECT, error)) { // Forcibly disconnected
					break;
				}
				else if(wlc::errorIs(wlc::MSG_SIZE, error)) { // Message too big
					continue;
				}
				else {
					std::lock_guard<std::mutex> lockCbk(_mutCbk);
					if(_cbkError) 
						_cbkError(Error(error, "UDP receive Error"));
					break;
				}
			}
			
			// Read message
			if(!buffering && recv_len < 14) // Bad message
				continue;
			
			// Get only header (14bytes)
			if(recv_len == 14) {
				Message message(buf, recv_len); // Will only read the header
				
				sizeWaited 				= (size_t)message.length();
				msgSerializedBuffer	= std::vector<char>(buf, buf+14);
				buffering = true;

				continue;
			}
			
			if(!buffering) { // Already full message : send it
				Message message(buf, recv_len);
				
				std::lock_guard<std::mutex> lockCbk(_mutCbk);
				if(_cbkData) 
					_cbkData(message);
			}
			else {
				if(recv_len + msgSerializedBuffer.size() < sizeWaited) { // Buffering
					msgSerializedBuffer.insert(msgSerializedBuffer.end(), buf, buf+recv_len);
					
					continue;
				}
				else { // Ending message
					size_t sizeToTake = sizeWaited - msgSerializedBuffer.size();
					msgSerializedBuffer.insert(msgSerializedBuffer.end(), buf, buf+sizeToTake);
					
					Message message(msgSerializedBuffer.data(), msgSerializedBuffer.size());
					buffering = false;
					
					std::lock_guard<std::mutex> lockCbk(_mutCbk);
					if(_cbkData) 
						_cbkData(message);
					
					if(sizeToTake == recv_len)
						continue;
					
					if(sizeToTake - recv_len < 14) {
						std::cout << "Bug" << std::endl;
					}
					
					// Preparing new buffer
					message = Message(buf, recv_len); // Will only read the header
					
					sizeWaited 				= (size_t)message.length();
					msgSerializedBuffer	= std::vector<char>(buf, buf+14);
					buffering = true;
				}
			}
		}
		
		// Forcibly disconnected
		if(_isConnected)
			disconnect();
	}

	void _send(const Socket& connectSocked, const Message& msg, const std::string& msgOnError = "Send error") const {
		if(send(connectSocked.get(), msg.data(), (int)msg.length(), 0) != (int)msg.length()) {
			std::lock_guard<std::mutex> lockCbk(_mutCbk);
			if(_cbkError) 
				_cbkError(Error(wlc::getError(), msgOnError));
		}
	}
	
private:
	// Members
	std::atomic<bool> _isConnected;
	std::atomic<bool> _isAlive;		// Control threads
	
	Socket _udpSock;
	Socket _tcpSock;
	SocketAddress _address;
	
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