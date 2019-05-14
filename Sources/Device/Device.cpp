#include "Device.hpp"

// Standard
#include <string>
#include <iostream>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>

// ---------- Implementation ubuntu ----------
#ifdef __linux__

	#include "Device_impl_linux.hpp"

// ---------- Implementation windows ----------
#elif _WIN32

	#include "Device_impl_windows.hpp"
	
#endif

// --------- Interface publique ------------
// Constructors
Device::Device(const std::string& pathVideo) : _impl(new _Impl(pathVideo)) {
	// Wait open
}
Device::~Device() {
	delete _impl;
}

// Flow
bool Device::open() {
	return _impl->open();
}
bool Device::close() {
	return _impl->close();
}

// Method
bool Device::grab() {
	return _impl->grab();
}
bool Device::retrieve(Gb::Frame& frame) {
	return _impl->retrieve(frame);
}
bool Device::read(Gb::Frame& frame) {
	return _impl->read(frame);
}

// Getters
const Gb::Size Device::getSize() const {
	return _impl->getSize();
}




