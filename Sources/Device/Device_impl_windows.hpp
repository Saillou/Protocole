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
	explicit _Impl(const std::string& pathVideo) : 
		_path(pathVideo), 
		_format({480, 640, MJPG}) {
		// Wait for open
	}
	~_Impl() {
		if(_cap.isOpened())
			_cap.release();
		_encoderH264.cleanup();
	}
	
	// Methods
	bool open() {
		// USB camera
		if(_path.size() == 1)
			_cap.open((int)(_path[0]-'0'));
		else 
			_cap.open(_path);
		
		if(_format.width > 0 && _format.height > 0)
			setFormat(_format.width, _format.height, (PixelFormat)_format.format);
		
		if(!_encoderH264.setup(_format.width, _format.height))
			return false;
		
		return _cap.isOpened();
	}
	bool close() {
		if(_cap.isOpened())
			_cap.release();	
		
		_encoderH264.cleanup();
		
		return !_cap.isOpened();
	}
	void refresh() {
		_encoderH264.refresh();
	}
	
	bool grab() {
		return _cap.grab();
	}
	bool retrieve(Gb::Frame& frame) {
		cv::Mat cvFrame;
		if(!_cap.retrieve(cvFrame))
			return false;
		
		// Compress to h264
		if(!_encoderH264.encodeBgr(cvFrame.data, frame.buffer))
			return false;
		
		// Complete
		frame.size = Gb::Size(_format.width, _format.height);
		frame.type = Gb::FrameType::H264;
		
		return frame.size.area() > 0;
	}
	bool read(Gb::Frame& frame) {
		return (grab() && retrieve(frame));
	}
	
	// Setters
	bool setFormat(int width, int height, PixelFormat formatPix) {
		if(_cap.isOpened()) {
			_cap.set(cv::CAP_PROP_FRAME_WIDTH, width);
			_cap.set(cv::CAP_PROP_FRAME_HEIGHT, height);
		}
		
		_format.width		= width;
		_format.height	= height;
		
		return true;
	}
	bool set(Device::Param code, double value) {		
		switch(code) {
			case Saturation:
				return _cap.set(cv::CAP_PROP_SATURATION, value);
			case Exposure:
				return _cap.set(cv::CAP_PROP_EXPOSURE, value);
			case AutoExposure:
				return _cap.set(cv::CAP_PROP_AUTO_EXPOSURE, value != 0 ? 1 : 0);
		}
		
		return false;
	}
	
	// Getters
	double get(Device::Param code) {
		switch(code) {
			// Saturation
			case Saturation:
				return _cap.get(cv::CAP_PROP_SATURATION);
			case MaxSaturation:
				return 100.0;
			case MinSaturation:
				return 0.0;
			case DefaultSaturation:
				return 64.0;
				
			// Exposure
			case Exposure:
				return _cap.get(cv::CAP_PROP_EXPOSURE);
			case MaxExposure:
				return 0.0;
			case MinExposure:
				return -12.0;
			case DefaultExposure:
				return -6.0;
				
			// Auto
			case AutoExposure:
				return _cap.get(cv::CAP_PROP_AUTO_EXPOSURE);
		}
		
		return 0.0;
	}
	
	const FrameFormat getFormat() const {
		return _format;
	}
	
	const std::string& getPath() const {
		return _path;
	}
	
private:
	// Methods
	
	// Members
	std::string _path;
	cv::VideoCapture _cap;
	FrameFormat	_format;
	
	EncoderH264 _encoderH264;
};

#endif