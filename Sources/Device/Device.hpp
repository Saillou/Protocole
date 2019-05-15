#pragma once

#include "structures.hpp"

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
		Saturation,
		MinSaturation,
		MaxSaturation,
		DefaultSaturation,
	};
	
	// Constructors
	explicit Device(const std::string& pathVideo);
	~Device();
	
	// Methods
	bool open();
	bool close();
	
	bool grab();
	bool retrieve(Gb::Frame& frame);
	bool read(Gb::Frame& frame);
	
	// Setters
	bool setFormat(int width, int height, PixelFormat formatPix);
	bool set(Param code, double value);
	
	// Getters
	double get(Param code);
	const Gb::Size getSize() const;
	

	
private:
	// Private implementation - OS dependent
	struct _Impl;
	_Impl* _impl = nullptr;
};
