#pragma once

#include "Client.hpp"
#include "Server.hpp"
#include "Device.hpp"

class SharedDevice : public Device {
public:
	enum Mode {
		NONE, EMITTER, RECEPTOR
	};
public:
	// Ctor
	SharedDevice() : _mode(NONE) {
		
	}
	~SharedDevice() {
		if(_mode != NONE)
			release();
	}
	
	// Methods
	bool run(const Mode mode, const std::string& ip, const int port) {
		if(_mode != NONE || !isOpened())
			return false;
		
		// Init sockets
		switch(_mode) {
			case EMITTER:
				_server.connectAt(port);
			break;
			
			case RECEPTOR:
				_client.connectTo(ip, port);
				/*
					Wait for connected signal
				*/
			break;
			
			default:
				return false;
		}
		
		// Change mode
		_mode = mode;		
	}
	
	void release() override {
		// Call mom
		Device::release();
		
		// Personalize
		switch(_mode) {
			case EMITTER:
				_server.disconnect();
			break;
			
			case RECEPTOR:
				_client.disconnect();
			break;
		}
		_mode = NONE;
	}

private:
	// Methods
	void _onFrame() override {
		// Call mom
		Device::_onFrame();	

		// Personalize		
		mutFrame.lock();
		
		if(_mode == EMITTER) {
			for(auto& client: _server.getClients()) {
				if(!client.connected)
					continue;
				
				_server.sendData(client, Message(Message::IMAGE, "frame"));
			}
		}
		
		mutFrame.unlock();
	}
	
	// Members
	Mode _mode;
	
	Server _server;
	Client _client;
	
};
