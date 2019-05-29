#pragma once

#include <cstring>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace Gb {
	enum FrameType {
		Data,
		Yuv420,
		Yuv422,
		Jpg420,
		Jpg422,
		Bgr24,
		H264,
	};
	
	struct Size {
		Size(int w=0, int h=0):width(w), height(h) {
		}
		
		int width;
		int height;
		
		int area() const {
			return width * height;
		}
	};
	
	struct Frame {
		// Constructors
		Frame(unsigned char* start = nullptr, unsigned long len = 0, const Size& s = Size(0,0), const FrameType t = FrameType::Data) : buffer(start, start+len), size(s), type(t) {	
		}
		Frame(const Frame& f) : buffer(f.buffer), size(f.size), type(f.type) {
		}
		Frame& operator=(const Frame& f) {
			buffer = f.buffer;
			size = f.size;
			type = f.type;
			return *this;
		}
		~Frame() {
			buffer.clear();
		}
		
		// Members
		std::vector<unsigned char> buffer;
		Size size;
		FrameType type;
		
		// Methods
		void clear() {
			buffer.clear();
			size = Size(0,0);
		}
		
		bool empty() const {
			return (size.area() == 0);
		}
		const unsigned char* start() const {
			return empty() ? nullptr : &buffer[0];
		}
		unsigned long length() const {
			return (unsigned long) buffer.size();
		}
		
		Frame clone() const {
			return *this;
		}
		
	};
}

