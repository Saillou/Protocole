#pragma once

#include <cstring>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace Gb {
	enum FrameType : unsigned int {
		Data			= 0x0, // 000b
		Yuv420		= 0x1,
		Yuv422		= 0x2, // 010b
		Jpg420		= 0x3,
		Jpg422		= 0x4, // 100b
		Bgr24			= 0x5,
		H264			= 0x6, // 110b
	};
	enum SizeType : unsigned int {
		// Size are maybe not the realy one, it' just the for our camera that I wrote here
		UnknownS	= 0x0, // 00b,  ??
		QVGA		= 0x1, // 01b, 320x200
		VGA		= 0x2, // 10b, 640x480
		HD			= 0x3, // 11b, 1280x720
	};
	
	// Combine frame and size type: (SizeType << 3) | (FrameType)
	
	struct Size {
		Size(int w=0, int h=0) : width(w), height(h) {
		}
		Size(SizeType type) : width(0), height(0) {
			if(type== QVGA) {
				width = 320;
				height = 200;
			}
			else if(type== VGA) {
				width = 640;
				height = 480;
			}	
			else if(type== HD) {
				width = 1280;
				height = 720;
			}	
		}
		
		int width;
		int height;
		
		int area() const {
			return width * height;
		}
		
		SizeType type() const {
			if(width == 320)
				return QVGA;			
			else if(width == 640)
				return VGA;			
			else if(width == 1280)
				return HD;		
			
			return UnknownS;
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

