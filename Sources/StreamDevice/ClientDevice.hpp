#pragma once

#include "../Network/Client.hpp"
#include "../Device/DeviceMt.hpp"
#include "../Timer.hpp"
#include "../Buffers.hpp"

#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>

class ClientDevice {
public:
	// -- Constructors --
	explicit ClientDevice(const IAddress& address) :
		_running(false),
		_port(address.port),
		_pathDest(address.ip),
		_format({640,480,Device::MJPG})
	{
		// client
	}
	
	~ClientDevice() {
		close();
	}
	
	// -- Methods --
	bool open(int timeoutMs = 0) {
		if(_running)
			return true;
		
		Timer timer;
		
		do {
			if(_client.connectTo(_pathDest, _port))
				return _initialization();
			
			timer.wait(500);
		}	while(timer.elapsed_mus()/1000 > timeoutMs);
		
		return false;
	}
	bool close() {
		_running = false;
		
		if(_pThreadBuffer && _pThreadBuffer->joinable())
			_pThreadBuffer->join();
		_pThreadBuffer.reset();
		
		_client.disconnect();
		
		return true;
	}
	
	
	// -- Getters --
	double get(Device::Param code) {
		// to do..
		return 0.0;
	}
	const Device::FrameFormat getFormat() {
		_client.sendInfo(Message(Message::DEVICE | Message::FORMAT, "?"));
	}
	bool isOpen() const {
		return _running;
	}
	
	// -- Setters --
	bool set(Device::Param code, double value) {
		MessageFormat command;
		command.add("code", 	code);
		command.add("value", 	value);
			
		return _client.sendInfo(Message(Message::DEVICE | Message::PROPERTIES, command.str()));
	}
	bool setFormat(int width, int height, Device::PixelFormat formatPix) {
		MessageFormat command;
		command.add("width", 	width);
		command.add("height", 	height);
		command.add("pixel", 	formatPix);
			
		return _client.sendInfo(Message(Message::DEVICE | Message::FORMAT, command.str()));
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
	
	// [Client is assumed connected.]
	bool _initialization() {
		// Set client events
		_client.onError(_cbkError);
		
		_client.onConnect([&]() {
			this->_onConnect();
		});
		_client.onInfo([&](const Message& message) {
			this->_onClientInfo(message);
		});
		_client.onData([&](const Message& message) {
			this->_onClientData(message);
		});
		
		return true;
	}
	
	void _bufferRead() {
		Gb::Frame frame;
		Message messageFrame;
		bool emitFrame = false;
		
		for(;_running; Timer::wait(2)) {
			// -- Get frame --
			_buffer.lock();
			if(_buffer.update(messageFrame)) {
				frame = Gb::Frame((unsigned char*)messageFrame.content(), (unsigned long)(messageFrame.size()), Gb::Size(_format.width, _format.height));
				emitFrame = true;
			}
			_buffer.unlock();
			
			// -- Send --
			if(emitFrame) {
				_mutCbk.lock();
				if(_cbkFrame)
					_cbkFrame(frame);
				_mutCbk.unlock();
				
				
				emitFrame = false;
			}
		}
	}
	
	bool _ready() {		
		_running = true;
		_pThreadBuffer = std::make_shared<std::thread>(&ClientDevice::_bufferRead, this);
		
		std::lock_guard<std::mutex> lockCbk(_mutCbk);
		if(_cbkOpen)
			_cbkOpen();
		
		_client.sendInfo(Message(Message::HANDSHAKE, "Start"));
		
		return true;
	}
	
	// Events
	void _onConnect() {
		_client.sendInfo(Message(Message::DEVICE | Message::FORMAT, "?"));
	}	
	void _onClientInfo(const Message& message) {		
		if(message.code() & Message::FORMAT)
			_treatDeviceFormat(message);
		
		if(message.code() & Message::PROPERTIES)
			_treatDeviceProperties(message);
	}	
	void _onClientData(const Message& message) {
		// Store frame's data
		if(message.code() == Message::DEVICE) {
			_buffer.lock();
			_buffer.push(message, message.timestamp());
			_buffer.unlock();
		}
	}
	
	// Treat
	void _treatDeviceFormat(const Message& message) {
		bool exist = false;
		MessageFormat command(message.str());
		
		int width 	= command.valueOf<int>("width");
		int height 	= command.valueOf<int>("height");
		
		if(width > 0 && height > 0) {
			// Should clear buffer : data size are wrong
			_buffer.lock();
			_buffer.clear();
			_buffer.unlock();
			
			// Change format
			_mutFormat.lock();
			_format.width = width;
			_format.height = height;
			_mutFormat.unlock();
			
			// Can begin running
			if(!_running) 
				_ready();
		}
	}
	void _treatDeviceProperties(const Message& message) {
		// To do..
	}
	
	// -- Members --
	std::atomic<bool> _running;
	
	int _port;
	std::string _pathDest;
	
	Device::FrameFormat _format;
	Client _client;
	
	std::shared_ptr<std::thread> _pThreadBuffer;
	MsgBuffer _buffer;
	
	mutable std::mutex _mutFormat;
	mutable std::mutex _mutCbk;
	std::function<void(const Error& error)> _cbkError;	
	std::function<void(const Gb::Frame&)> _cbkFrame;
	std::function<void(void)> _cbkOpen;
};

