#pragma once

#include <vector>

#include "Decoder.hpp"
#include "Encoder.hpp"

#include "../Device/structures.hpp"

class Translator {
public:
	Translator() {
	}
	~Translator() {
		cleanup();
	}
	
	bool setup(int width, int height) {
		cleanup();
		
		_encoderJpg.setup();
		_decoderJpg.setup();
		return _encoderH264.setup(width, height);
	}
	void refresh() {
		_encoderH264.refresh();
	}
	bool cleanup() {
		_encoderH264.cleanup();
		_encoderJpg.cleanup();
		_decoderJpg.cleanup();
		
		return true;
	}
	
	bool treat(Gb::Frame& rawFrame, Gb::Frame& frame) {
		// Timer t;
		bool success = false;
		
		int w = rawFrame.size.width;
		int h = rawFrame.size.height;
		int area = w*h;
		
		// -- From jpg to h264: (Brief stop at YUV422 and YUV420 step)
		if(frame.type == Gb::FrameType::H264 || frame.type == Gb::FrameType::Yuv422 || frame.type == Gb::FrameType::Yuv420) {
			// jpg decompress : jpg422 -> yuv422
			if(!_decoderJpg.decode2yuv422(rawFrame.buffer, _yuv422Frame, w, h))
				goto End_Translation;
			
			if(frame.type == Gb::FrameType::Yuv422) {
				success = true;
				goto End_Translation;
			}

			// yuv422 -> yuv420
			if(_yuv420Frame.size() != area*3/2)
				_yuv420Frame.resize(area*3/2);
			
			Convert::yuv422ToYuv420(&_yuv422Frame[0], &_yuv420Frame[0], w, h);
			if(frame.type == Gb::FrameType::Yuv420) {
				success = true;
				goto End_Translation;
			}

			// h264 encode : yuv420 -> h264 packet
			if(_encoderH264.encodeYuv(&_yuv420Frame[0], frame.buffer))
				success = true;
		}
		
		// -- From jpg to smaller jpg
		else if(frame.type == Gb::FrameType::Jpg420) { 
			std::vector<unsigned char> bgrBuffer;
			
			// jpg decompress : jpg422 -> bgr24
			if(_decoderJpg.decode2bgr24(rawFrame.buffer, bgrBuffer, w, h)) {
				// jpg compress : bgr24 -> jpg420
				if(_encoderJpg.encodeBgr24(bgrBuffer, frame.buffer, w, h, 30)) {
					success = true;
				}
			}
		}
		
		// -- From jpg to bgr
		else if(frame.type == Gb::FrameType::Bgr24) { 
			// jpg decompress : jpg422 -> bgr24
			if(_decoderJpg.decode2bgr24(rawFrame.buffer, frame.buffer, w, h)) {
				success = true;
			}
		}
		
		// -- From jpg to rgb
		else if(frame.type == Gb::FrameType::Rgb24) { 
			// jpg decompress : jpg422 -> rgb24
			if(_decoderJpg.decode2rgb24(rawFrame.buffer, frame.buffer, w, h)) {
				success = true;
			}
		}
		
		// -- Raw
		else {
			frame.buffer = rawFrame.buffer;
			frame.type = rawFrame.type;
			success = true;
		}
		
		// -- Set final info
End_Translation:
		if(success) 
			frame.size = rawFrame.size;
		else 
			frame.clear();
		
		// std::cout << t.elapsed_mus()/1000.0 << std::endl;
		return success;
	}
	
private:
	std::vector<unsigned char> _yuv422Frame;
	std::vector<unsigned char> _yuv420Frame;
	EncoderH264 _encoderH264;
	EncoderJpg _encoderJpg;
	DecoderJpg _decoderJpg;
};

