#pragma once

#include <string>
#include <iostream>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>

#include "structures.hpp"
#include "../Tool/Decoder.hpp"
#include "../Tool/Encoder.hpp"
#include "../Tool/Timer.hpp"

// ---------- Header for linux ----------
#ifdef __linux__
	// Use v4l2
	#include "helper_v4l2.hpp"

	#include <linux/videodev2.h>
	#include <errno.h>
	#include <sys/ioctl.h>
	#include <sys/mman.h>
	#include <sys/poll.h>

	#include <cstdint>
	#include <cstdio>
	#include <cstring>

	#include <fcntl.h>
	#include <unistd.h>

// ---------- Header for windows ----------
#elif _WIN32
	// Use Opencv
	#include <opencv2/core.hpp>	
	#include <opencv2/videoio.hpp>	
	#include <opencv2/highgui.hpp>
	#include <opencv2/imgproc.hpp>
	#include <opencv2/imgcodecs.hpp>
#endif

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
		Saturation, Brightness, Hue, Contrast, Whiteness
		Exposure, AutoExposure
	};
	
private:
// ----------  Implementation linux ----------
#ifdef __linux__
	#include "Device_impl_linux.hpp"

// ---------- Implementation windows ----------
#elif _WIN32
	#include "Device_impl_windows.hpp"
	
#endif

public:
	// Constructors
	explicit Device(const std::string& pathVideo): _impl(new _Impl(pathVideo)) {
		// Wait open
	}
	~Device() {
		delete _impl;
	}
	
	// Methods
	bool open() {
		return _impl->open();
	}
	bool close() {
		return _impl->close();
	}
	void refresh() {
		return _impl->refresh();
	}

	// Method
	bool grab() {
		return _impl->grab();
	}
	bool retrieve(Gb::Frame& frame) {
		return _impl->retrieve(frame);
	}
	bool read(Gb::Frame& frame) {
		return _impl->read(frame);
	}

	// Setters
	bool setFormat(int width, int height, PixelFormat formatPix) {
		return _impl->setFormat(width, height, formatPix);
	}
	bool set(Param code, double value) {
		return _impl->set(code, value);
	}

	// Getters
	const FrameFormat getFormat() const {
		return _impl->getFormat();
	}
	double get(Param code) {
		return _impl->get(code);
	}

	
private:
	// Private implementation - OS dependent
	struct _Impl;
	_Impl* _impl = nullptr;
};
