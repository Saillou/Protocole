#pragma once

#include "../Tool/Timer.hpp"
#include "../Network/Server.hpp"
#include "../Device/DeviceMt.hpp"

#include <map>
#include <mutex>
#include <string>
#include <functional>

class ServerDevice {
public:
	// -- Constructors --
	explicit ServerDevice(const std::string& pathCamera, const int port = 8888) :
		_port(port),
		_pathDest(pathCamera)
	{
		// server
	}
	
	~ServerDevice() {
		close();
	}
	
	// -- Methods --
	bool open(int timeoutMs = 0) {
		if(!_device.open(_pathDest))
			return false;
		
		_device.setFormat(1280, 720, Device::MJPG);
		
		Timer timer;
		
		do {
			if(_server.connectAt(_port))
				return _initialization();
			
			timer.wait(500);
		}	while(timer.elapsed_mus()/1000 > timeoutMs);
		
		return false;
	}
	bool close() {
		_device.release();
		_server.disconnect();
		
		return true;
	}
	
	
	// -- Getters --
	double get(Device::Param code) const {
		return _device.get(code);
	}
	const Device::FrameFormat getFormat() const {
		return _device.getFormat();
	}
	bool isOpen() const {
		return _device.isOpened();
	}
	
	// -- Setters --
	bool set(Device::Param code, double value) {
		return _device.set(code, value);
	}
	bool setFormat(int width, int height, Device::PixelFormat formatPix) {
		return _device.setFormat(width, height, formatPix);
	}
	
	// -- Events --
	void onOpen(const std::function<void(void)>& cbkOpen) {
		std::lock_guard<std::mutex> lockCbk(_mutCbk);
		_cbkOpen = cbkOpen;
	}
	void onFrame(const std::function<void(const Gb::Frame&)>& cbkFrame) {
		std::lock_guard<std::mutex> lockCbk(_mutCbk);
		_cbkFrame = cbkFrame;
	}
	void onError(const std::function<void(const Error& error)>& cbkError) {
		std::lock_guard<std::mutex> lockCbk(_mutCbk);
		_cbkError = cbkError;
	}
	
private:	
	// -- Methods --
	
	// [Server is assumed connected.]
	bool _initialization() {		
		// Set server events
		_server.onError(_cbkError);
		
		_server.onInfo([&](const Server::ClientInfo& client, const Message& message) {
			this->_onServerInfo(client, message);
		});
		_server.onClientConnect([&](const Server::ClientInfo& client) {
			this->_onClientConnect(client);
		});
		_server.onClientDisconnect([&](const Server::ClientInfo& client) {
			this->_onClientDisconnect(client);
		});
	
		// Set device events
		_device.onFrame([&](const Gb::Frame& frame) {
			this->_onDeviceFrame(frame);
		});
		
		
		// Callback
		std::lock_guard<std::mutex> lockCbk(_mutCbk);
		if(_cbkOpen)
			_cbkOpen();
		
		return true;
	}
	
	// Events
	void _onClientConnect(const Server::ClientInfo& client) {
		_clientPlayer[client.id()] = false;
	}
	void _onClientDisconnect(const Server::ClientInfo& client) {
		_clientPlayer[client.id()] = false;
	}
	
	void _onDeviceFrame(const Gb::Frame& frame) {
		// Broadcast frame
		for(auto& client: _server.getClients()) {
			if(client.connected && _clientPlayer[client.id()]) {
				_server.sendData(client, Message(Message::DEVICE, reinterpret_cast<const char*>(frame.start()), frame.length()));
			}
		}
		
		// Callback
		std::lock_guard<std::mutex> lockCbk(_mutCbk);
		if(_cbkFrame)
			_cbkFrame(frame);
	}
	
	void _onServerInfo(const Server::ClientInfo& client, const Message& message) {
		// Analyse message
		std::string msg = message.str();
		
		if(message.code() & Message::FORMAT)
			_treatFormat(client, msg);
		
		if(message.code() & Message::PROPERTIES)
			_treatProperties(client, msg);
		
		if(message.code() & Message::HANDSHAKE)
			_clientPlayer[client.id()] = (msg == "Start");
	}
	
	// Treat
	void _treatFormat(const Server::ClientInfo& client, const std::string& msg) {
		if(msg == "?") {
			// --- get ---
			Device::FrameFormat fmt = isOpen() ? _device.getFormat() : Device::FrameFormat{640,480,Device::MJPG};
			
			MessageFormat command;
			command.add("width", 	fmt.width);
			command.add("height", 	fmt.height);
			command.add("pixel", 	fmt.format);
			
			_server.sendInfo(client, Message(Message::DEVICE | Message::FORMAT, command.str()));
		}
		else {
			// --- set ---
			bool exist = false;
			MessageFormat command(msg);
			
			int width 							= command.valueOf<int>("width");
			int height 							= command.valueOf<int>("height");
			Device::PixelFormat pixFmt 	= command.valueOf<Device::PixelFormat>("pixel", &exist);
			
			if(exist && width > 0 && height > 0) {
				_device.setFormat(width, height, pixFmt);
				
				// Confirm change
				_server.sendInfo(client, Message(Message::DEVICE | Message::FORMAT, msg));
			}
		}		
	}
	void _treatProperties(const Server::ClientInfo& client, const std::string& msg) {
		if(msg == "?") {
			// --- get ---
			MessageFormat command;
			command.add("saturation", 	_device.get(Device::Saturation));
			command.add("exposure", 		_device.get(Device::Exposure));
			command.add("auto_exposure", _device.get(Device::AutoExposure));
			
			_server.sendInfo(client, Message(Message::DEVICE | Message::PROPERTIES, command.str()));
		}
		else {
			// --- set ---
			bool exist = false;
			MessageFormat command(msg);
			
			// Couple Code/Value
			Device::Param code = command.valueOf<Device::Param>("code", &exist);
			if(exist) {
				set(code, command.valueOf<double>("value"));
				return;
			}
			
			// Directly
			double saturation	= command.valueOf<double>("saturation", &exist);
			if(exist)
				set(Device::Saturation, saturation);
			
			double exposure = command.valueOf<double>("exposure", &exist);
			if(exist)
				set(Device::Exposure, exposure);
			
			double autoExposure = command.valueOf<double>("auto_exposure", &exist);
			if(exist)
				set(Device::AutoExposure, autoExposure);
		}	
	}
	
	
	// -- Members --
	int _port;
	std::string _pathDest;
	
	Server _server;
	DeviceMt _device;
	
	mutable std::mutex _mutCbk;
	std::function<void(const Error& error)> _cbkError;	
	std::function<void(const Gb::Frame&)> _cbkFrame;
	std::function<void(void)> _cbkOpen;
	
	std::map<SOCKET, bool> _clientPlayer;
};

