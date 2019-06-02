#pragma once

#include "../Network/Client.hpp"
#include "../Device/DeviceMt.hpp"
#include "../Tool/Timer.hpp"
#include "../Tool/Decoder.hpp"
#include "../Tool/Buffers.hpp"

#include <map>
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
		_format({640,480,Device::MJPG}),
		_errCount(0)
	{
		// client
		_decoderH264.setup();
		_decoderJpg.setup();
	}
	
	~ClientDevice() {
		close();
		_decoderH264.cleanup();
		_decoderJpg.cleanup();
	}
	
	// -- Methods --
	bool open(const int timeoutMs = 0) {
		if(_running)
			return true;
		
		Timer timer;
		
		do {
			if(_client.connectTo(_pathDest, _port))
				return _initialization();
			
			timer.wait(500);
		}	while(timeoutMs < 0 || timer.elapsed_mus()/1000 < timeoutMs);
		
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
	bool refresh() {
		std::cout << "Refresh" << std::endl;
		return _client.sendInfo(Message(Message::DEVICE | Message::TEXT, "refresh"));
	}
	
	// -- Getters --
	double get(Device::Param code) {
		const uint64_t TIMEOUT_MUS = 500*1000; // 500ms
		double value = 0.0;
		
		// Create thread to get back the answer
		std::thread threadWaitForCbk([&](){
			Timer timeout;
			std::atomic<bool> gotIt = false;
			
			this->onGetParam(code, [&](double val) {
				gotIt = true;
				value = val;
			});
			
			while(timeout.elapsed_mus() < TIMEOUT_MUS && !gotIt)
				timeout.wait(2);
		});
		
		// Launch command
		MessageFormat command;
		command.add("code?", code);
		_client.sendInfo(Message(Message::DEVICE | Message::PROPERTIES, command.str()));
		
		// Wait for thread to finish
		if(threadWaitForCbk.joinable())
			threadWaitForCbk.join();
		
		// Finally return
		return value;
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
	bool setFrameType(Gb::FrameType ftype) {
		// Need to be done
		return false;
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
	
	void onGetParam(Device::Param code, const std::function<void(double)>& cbkParam) {
		std::lock_guard<std::mutex> lockCbk(_mutCbk);
		_mapCbkParam[code] = cbkParam;
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
		bool success = false;
		
		for(;_running; Timer::wait(2)) {
			emitFrame = false;
			
			// -- Get frame --
			_buffer.lock();
			if(_buffer.update(messageFrame)) {
				frame = Gb::Frame(
					(unsigned char*)messageFrame.content(), 
					(unsigned long)(messageFrame.size()), 
					Gb::Size(_format.width, _format.height), 
					(Gb::FrameType)(messageFrame.code() >> 10) // Decode frame type
				);
				emitFrame = true;
			}
			_buffer.unlock();
			
			// -- Emit --
			if(emitFrame) {	
				Gb::Frame frameEmit;
				
				// Treat
				if(_treatFrame(frame, frameEmit)) {	
					_errCount = 0;
					
					// Call cbk
					_mutCbk.lock();
					if(_cbkFrame)
						_cbkFrame(frameEmit);
					_mutCbk.unlock();
				}
				else {
					if(_errCount ++> 10) {
						refresh();
						_errCount = 0;
					}
				}
			} // !Emit
			
		} // !Loop
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
		if(message.code() & Message::DEVICE) {
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
		bool exist = false;
		MessageFormat command(message.str());
		
		Device::Param code = command.valueOf<Device::Param>("code", &exist);
		if(!exist)
			return;
		double value = command.valueOf<double>("value");
		
		std::lock_guard<std::mutex> lockCbk(_mutCbk);
		if(_mapCbkParam.find(code) != _mapCbkParam.end()) {
			_mapCbkParam[code](value);
			_mapCbkParam[code] = nullptr;
		}
	}
	bool _treatFrame(Gb::Frame& frameIn, Gb::Frame& frameOut) {
		bool success = false;
		// Decode 
		if(frameIn.type == Gb::FrameType::H264) {
			success = _decoderH264.decode(frameIn.buffer, frameOut.buffer, &frameIn.size.width, &frameIn.size.height);
			_format.width = frameIn.size.width;
			_format.height = frameIn.size.height;
		}
		else if(frameIn.type == Gb::FrameType::Jpg422 || frameIn.type == Gb::FrameType::Jpg420) {
			success = _decoderJpg.decode2bgr24(frameIn.buffer, frameOut.buffer, frameIn.size.width, frameIn.size.height);
		}
		else if(frameIn.type == Gb::FrameType::Bgr24) {
			frameOut.buffer = frameIn.buffer;
			success = true;
		}
		
		// Add info
		if(success) {
			frameOut.size = frameIn.size;
			frameOut.type = Gb::FrameType::Bgr24;
		}
		
		return success;
	}
	
	// -- Members --
	std::atomic<bool> _running;
	
	int _port;
	std::string _pathDest;
	
	Device::FrameFormat _format;
	Client _client;
	
	std::shared_ptr<std::thread> _pThreadBuffer;
	MsgBuffer _buffer;
	int _errCount;
	
	DecoderH264 _decoderH264;
	DecoderJpg _decoderJpg;
	
	mutable std::mutex _mutFormat;
	mutable std::mutex _mutCbk;
	std::function<void(const Error& error)> _cbkError;	
	std::function<void(const Gb::Frame&)> _cbkFrame;
	std::function<void(void)> _cbkOpen;
	std::map<Device::Param, std::function<void(double)>> _mapCbkParam;
};

