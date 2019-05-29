#pragma once

#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>

#include "Device.hpp"
#include "structures.hpp"
#include "../Tool/Timer.hpp"

// ------------ Device : Pull frames in a dedicated thread ------------
class DeviceMt {	
public:
	// Constructor
	DeviceMt() : _running(false) {
		// Wait for open
	}
	
	// Destructor
	virtual ~DeviceMt() {
		// Only if release() wasn't call before
		if(_pThread || _pDevice) 
			release();
	}
	
	// - Methods
	virtual bool open(const std::string& path) {
		if(_running || _pThread || _pDevice) // Already running
			release();
		
		// New path
		deviceName = path;
		
		// Start device
		std::lock_guard<std::mutex> lockDevice(_mutDevice);
		_pDevice = std::make_shared<Device>(path);
		if(!_pDevice->open()) {
			_pDevice.reset();
			return false;
		}
		
		// Start threading		
		_running = true;
		_pThread = std::make_shared<std::thread>(&DeviceMt::_pullCapture, this);
		
		return true;
	}
	virtual void refresh() {
		if(_pDevice)
			_pDevice->refresh();
	}
	
	// Set a callback
	virtual void onFrame(const std::function<void(const Gb::Frame&)>& cbkFrame) {
		_mutCbk.lock();
		_cbkFrame = cbkFrame;
		_mutCbk.unlock();
	}
	
	// Stop threading and releasing _cap
	virtual void release() {
		// Nothing to update
		_running = false;
		
		_mutCbk.lock();
		_cbkFrame = nullptr;
		_mutCbk.unlock();
		
		// Wait thread to end
		if(_pThread)
			if(_pThread->joinable())
				_pThread->join();
			
		_pThread.reset();
		
		// Close device
		std::lock_guard<std::mutex> lockDevice(_mutDevice);
		if(_pDevice)
			_pDevice->close();
		
		_pDevice.reset();
	}
	
	// Setters
	bool setFormat(int width, int height, Device::PixelFormat formatPix) {
		if(_pDevice) {
			std::lock_guard<std::mutex> lockDevice(_mutDevice);
			return _pDevice->setFormat(width, height, formatPix);
		}
		return false;
	}
	bool set(Device::Param code, double value) {
		if(_pDevice)
			return _pDevice->set(code, value);
		return false;
	}
	
	// Getters
	bool isOpened() const {
		return _running;
	}
	
	const Device::FrameFormat getFormat() const {
		if(_pDevice)
			return _pDevice->getFormat();
		
		return Device::FrameFormat {0,0,0};
	}
	double get(Device::Param code) const {
		if(_pDevice)
			return _pDevice->get(code);
		
		return false;
	}
	
protected:
	// - Members
	Gb::Frame frame;
	std::string deviceName;

	// - Methods	
	virtual void _onFrame() {
		_mutCbk.lock();
		
		if(_cbkFrame) 
			_cbkFrame(frame);					// Call back if set
		
		_mutCbk.unlock();		
	}
	
private:	
	// Threaded function : grab and retrieve frame
	void _pullCapture() {
		while(_running && _pDevice) {
			std::lock_guard<std::mutex> lockDevice(_mutDevice);
			_pDevice->grab(); 				// Will wait until the camera is available
			
			_mutFrame.lock();
			// frame.type = Gb::FrameType::H264;
			// frame.type = Gb::FrameType::Jpg422;
			frame.type = Gb::FrameType::Jpg420;
			if(_pDevice->retrieve(frame))
				_onFrame();
			
			_mutFrame.unlock();			
		}
	}
	
	// Members	
	std::atomic<bool> _running = {false};
	std::shared_ptr<std::thread> _pThread;
	
	mutable std::mutex _mutFrame;
	mutable std::mutex _mutDevice;
	mutable std::mutex _mutCbk;
	
	std::shared_ptr<Device> _pDevice;
	std::function<void(const Gb::Frame&)> _cbkFrame;
};
