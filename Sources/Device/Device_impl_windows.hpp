#pragma once
#ifdef _WIN32

#include "Device.hpp"

// Based on Opencv
#include <opencv2/core.hpp>	
#include <opencv2/videoio.hpp>	
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>


struct Device::_Impl {
public:
	// Constructors
	explicit _Impl(const std::string& pathVideo) : _path(pathVideo), _PARAMS{(int)cv::IMWRITE_JPEG_QUALITY, 40} {
		// Wait for open
	}
	~_Impl() {
		if(_cap.isOpened())
			_cap.release();
	}
	
	// Methods
	bool open() {
		// USB camera
		if(_path.size() == 1)
			_cap.open((int)(_path[0]-'0'));
		else 
			_cap.open(_path);
		
		return _cap.isOpened();
	}
	bool close() {
		if(_cap.isOpened())
			_cap.release();	
		
		return !_cap.isOpened();
	}
	
	bool grab() {
		return _cap.grab();
	}
	bool retrieve(Gb::Frame& frame) {
		cv::Mat cvFrame;
		if(!_cap.retrieve(cvFrame))
			return false;
		
		_size = Gb::Size(cvFrame.cols, cvFrame.rows);
		
		// Compress to jpg
		if(!cv::imencode(".jpg", cvFrame, frame.buffer, _PARAMS))
			return false;
		
		// Complete
		frame.size = _size;
		return frame.size.area() > 0;
	}
	bool read(Gb::Frame& frame) {
		return (grab() && retrieve(frame));
	}
	
	// Setters
	bool setFormat(int width, int height, PixelFormat formatPix) {
		_cap.set(cv::CAP_PROP_FRAME_WIDTH, width);
		_cap.set(cv::CAP_PROP_FRAME_HEIGHT, height);
		
		_size = Gb::Size(width, height);
		
		return true;
	}
	bool set(Device::Param code, double value) {
		switch(code) {
			case Saturation:
				return _cap.set(cv::CAP_PROP_SATURATION, value);
		}
		
		return false;
	}
	
	// Getters
	double get(Device::Param code) {
		switch(code) {
			case Saturation:
				return _cap.get(cv::CAP_PROP_SATURATION);
			case MaxSaturation:
				return 100.0;
			case MinSaturation:
				return 0.0;
			case DefaultSaturation:
				return 64.0;
		}
		
		return 0.0;
	}
	
	const Gb::Size getSize() const {
		return _size;
	}
	
private:
	// Members
	std::string _path;
	cv::VideoCapture _cap;
	Gb::Size _size;
	
	// Constantes
	const std::vector<int> _PARAMS;
};

#endif