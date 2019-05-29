#pragma once

#include <string>
#include <iostream>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>

#include "Encoder.hpp"
#include "structures.hpp"
#include "../Tool/Timer.hpp"

class Device {
public:
	// Structures
	struct FrameBuffer {
		void *start;
		size_t length;
	};
	struct FrameFormat {
		int width;
		int height;
		int format;	
	};
	
	// Enums
	enum PixelFormat {
		YUYV, MJPG
	};
	
	enum Param {
		Maximum		= (1 << 1),
		Minimum		= (1 << 2),
		Default		= (1 << 3),
		Automatic	= (1 << 4),
		
		Saturation 	= (1 << 5),
		MinSaturation 			= Saturation | Minimum,
		MaxSaturation 			= Saturation | Maximum,
		DefaultSaturation	= Saturation | Default,
		
		Exposure		= (1 << 6),
		MinExposure 		= Exposure | Minimum,
		MaxExposure 		= Exposure | Maximum,
		DefaultExposure 	= Exposure | Default,
		AutoExposure 		= Exposure | Automatic,
	};
	
	// Constructors
	explicit Device(const std::string& pathVideo);
	~Device();
	
	// Methods
	bool open();
	bool close();
	void refresh();
	
	bool grab();
	bool retrieve(Gb::Frame& frame);
	bool read(Gb::Frame& frame);
	
	
	// Setters
	bool setFormat(int width, int height, PixelFormat formatPix);
	bool set(Param code, double value);
	
	// Getters
	const FrameFormat getFormat() const;
	double get(Param code);
	

	
private:
	// Private implementation - OS dependent
	struct _Impl;
	_Impl* _impl = nullptr;
};
