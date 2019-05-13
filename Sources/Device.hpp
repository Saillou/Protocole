#pragma once

/* ---- Platform specifics ---- */
// Linux
#ifdef __linux__
	
// Windows	
#elif _WIN32
	// Based on Opencv
	#include <opencv2/core.hpp>	
	#include <opencv2/videoio.hpp>	
	#include <opencv2/highgui.hpp>
#endif

// Standard
#include <iostream>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>

// ------------ Device : Pull frames in a dedicated thread ------------
class Device {	
public:
	// Types
	typedef cv::Mat Frame;
	
	// Constructor
	Device() : _running(false) {
		// Wait for open
	}
	
	// Destructor
	virtual ~Device() {
		// Only if release() wasn't call before
		if(_pThread) 
			release();
	}
	
	// - Methods
	virtual bool open(int idCamera) {
		if(_cap.isOpened())
			return false;
		
		deviceName = "Camera " + std::to_string(idCamera);
		return _cap.open(idCamera) && _start();
	}
	virtual bool open(const std::string& path) {
		if(_cap.isOpened())
			return false;
		
		deviceName = path;
		return _cap.open(path) && _start();
	}
	
	// Set a callback
	virtual void onFrame(const std::function<void(const Frame&)>& cbkFrame) {
		_mutCbk.lock();
		_cbkFrame = cbkFrame;
		_mutCbk.unlock();
	}
	
	virtual void show(const Frame& frameShowed) {
		cv::imshow(deviceName, frameShowed);
		cv::waitKey(1);		
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
		
		// Close _cap
		if(_cap.isOpened())
			_cap.release();
		
		// Close frame
		if(!frame.empty())
			frame.release();
		
		if(!deviceName.empty())
			cv::destroyWindow(deviceName);
	}
	
	// Getters
	bool isOpened() {
		return _cap.isOpened();
	}
	
protected:
	// - Members
	std::string deviceName;
	std::mutex mutFrame;
	cv::Mat frame;
	
	// - Methods	
	virtual void _onFrame() {
		_mutCbk.lock();
		if(_cbkFrame)
			_cbkFrame(frame);					// Call back if set
		_mutCbk.unlock();		
	}
	
private:
	// Start threading
	bool _start() {
		// Check opening conditions
		if(_running || _pThread || !_cap.isOpened())
			return false;
		
		_running = true;
		_pThread = std::make_shared<std::thread>(&Device::_pullCapture, this);
		
		return true;
	}
	
	// Threaded function : grab and retrieve frame
	void _pullCapture() {
		while(_running) {
			_cap.grab(); 							// Will wait until the camera is available
			
			mutFrame.lock();
			_cap.retrieve(frame);				// False if error happened
			
			_onFrame();
			
			mutFrame.unlock();			
		}
	}
	
	// Members
	std::atomic<bool> _running = {false};
	std::shared_ptr<std::thread> _pThread;
	
	cv::VideoCapture _cap;
	
	std::mutex _mutCbk;
	std::function<void(const Frame&)> _cbkFrame;
};



