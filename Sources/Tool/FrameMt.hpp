#pragma once

#include <mutex>

#ifdef __linux__
	// Shall die

#elif _WIN32
	// Based on Opencv
	#include <opencv2/core.hpp>
	#include <opencv2/highgui.hpp>
	#include <opencv2/imgproc.hpp>
	
	// Decode jpg
	#include <turbojpeg.h>
	
#endif

#include "Device/structures.hpp"

// -- Multi threaded Gb::frames : For decoding and displaying Gb::Frame (through turbojpg/opencv)
class FrameMt {
public:	
	FrameMt() : _cvFrame(cv::Mat::zeros(480, 640, CV_8UC3)), _frame(), _updated(false) {
	}
	
	void lock() const {
		_mut.lock();
	}
	void unlock() const {
		_mut.unlock();
	}
	void setFrame(const Gb::Frame& frame) {
		std::lock_guard<std::mutex> lock(_mut);
		
		_frame = frame;
		_updated = true;
	}
	
	bool decode(const tjhandle& decoder) {
		std::lock_guard<std::mutex> lock(_mut);
		
		if(!_updated || empty())
			return false;
		
		// Anyway, this frame will be read : fail or not, we don't want to read again
		_updated = false;
		
		// Re-Allocatation
		if(_cvFrame.size().area() != area())
			_cvFrame = cv::Mat::zeros(height(), width(), CV_8UC3);
		
		// Decode
		return (
			tjDecompress2 (
				decoder, 
				data(), length(), 
				_cvFrame.data, 
				width(), 0, height(), 
				TJPF_BGR, TJFLAG_FASTDCT
			) >= 0);
	}
	void show(const std::string& winName) {
		std::lock_guard<std::mutex> lock(_mut);
		
		if(!_cvFrame.empty())
			cv::imshow(winName, _cvFrame);	
	}
	
	// Getters
	const Gb::Frame& get() const {
		return _frame;
	}
	int width() const {
		return _frame.size.width;
	}
	int height() const {
		return _frame.size.height;
	}
	int area() const {
		return width() * height();
	}
	const unsigned char* data() const {
		return _frame.start();
	}
	unsigned long length() const {
		return _frame.length();
	}
	bool empty() const {
		return _frame.empty();
	}
	bool updated() const {
		return _updated;
	}
	
private:
	cv::Mat _cvFrame;
	Gb::Frame _frame;
	std::atomic<bool> _updated;
	mutable std::mutex _mut;
};
