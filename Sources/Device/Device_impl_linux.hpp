#pragma once
#ifdef __linux__

#include "Device.hpp"
#include "helper_v4l2.hpp"
#include "../Tool/Decoder.hpp"

// Use v4l2
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

struct Device::_Impl {	
public:
	// Constructors
	explicit _Impl(const std::string& pathVideo) :
		_fd(-1), 
		_open(false),
		_path(pathVideo), 
		_format({640, 480, MJPG}),
		_buffer({(void*)nullptr, (size_t)0}),
		_bufferQueued(false)
	{
		// Wait open
	}
	~_Impl() {
		close();
	}
	
	// Methods
	bool open() {
		if(_open)
			return true;
		
		struct v4l2_buffer buf = {0};
		
		// -- Init file descriptor --
		if(!hvl::openfd(_fd, _path))
			goto failed;
		
		// -- Set format --
		if(_format.width == 0 || _format.height == 0) {
			_format.width 		= 640;
			_format.height	= 480;
			_format.format	= MJPG;
		}
		printf("Setting: [%d x %d] \n",  _format.width,  _format.height);
		
		if(!hvl::setFormat(_fd,  _format.width,  _format.height))
			goto failed;
					
		// -- Init buffers --
		if(!hvl::requestBuffer(_fd))
			goto failed;
		
		if(!hvl::queryBuffer(_fd, buf))
			goto failed;
	 
		// Link memory
		if(!hvl::memoryMap(_fd, buf, &_buffer.start, _buffer.length))
			goto failed;
		
		printf("Buffer max: %d KB\n", _buffer.length/1000);
		
		// -- Init encoder/decoder
		_encoderJpg.setup();		
		_decoderJpg.setup();
		if(!_encoderH264.setup(_format.width, _format.height))
			goto failed;
		
		// Start	 
		if(!_askFrame())
			goto failed;
		
		if(!hvl::startCapture(_fd))
			goto failed;
		
		// ----- Success -----
		_open = true;
		return true;		
	
		// ----- Failed -----
	failed:
		close();
		return false;		
	}
	bool close() {			
		if(_fd == -1)
			return true;
		
		_open = false;
		
		if(!hvl::stopCapture(_fd))
			goto failed;
		
		if(!hvl::memoryUnmap(_fd, &_buffer.start, _buffer.length))
			goto failed;
		
		if(!hvl::closefd(_fd))
			goto failed;

		_encoderH264.cleanup();
		_decoderJpg.cleanup();
		_encoderJpg.cleanup();
		
		// ----- Success -----
		return true;		
	
		// ----- Failed -----
	failed:
		_fd = -1;
		return false;
	}
	void refresh() {
		_encoderH264.refresh();
	}
	
	bool grab() {
		if(!_open)
			return false;
		printf("grabbing \n");
		
		struct v4l2_buffer buf = {0};
		struct pollfd fdp;
		fdp.fd 			= _fd;
		fdp.events 		= POLLIN | POLLOUT; // inputs
		fdp.revents		= 0; // outputs
		int err;
		
		// Wait event on fd : return : 1 => ok, 0 => timeout, -1 => error
		if((err = poll(&fdp, 1, 1000)) <= 0) {
			printf("poll : %d \n");
			return false;
		}
	
		// Grab frame
		if(!hvl::dequeueBuffer(_fd, buf)) 
			return false;

		_bufferQueued = false;
		
		// Check size
		_buffer.length = (buf.bytesused > 0) ? buf.bytesused : _buffer.length;			
		
		return true;		
	}
	bool retrieve(Gb::Frame& frame) {
		if(!_open)
			return false;
		
		_rawData.buffer 	= std::vector<unsigned char>((unsigned char*)_buffer.start, (unsigned char*)_buffer.start + _buffer.length);
		_rawData.size 		= Gb::Size(_format.width, _format.height);
		_rawData.type 		= (_format.format == MJPG) ? Gb::FrameType::Jpg422 : Gb::FrameType::Yuv422;
			
		return _treat(frame);		
	}
	bool read(Gb::Frame& frame) {
		return (grab() && retrieve(frame));
	}
	
	// Setters
	bool setFormat(int width, int height, PixelFormat formatPix) {
		close();
		_format.width  = width;
		_format.height = height;
		_format.format = formatPix;
		return open();
	}
	bool set(Device::Param code, double value) {
		if(!_open)
			return false;
		
		struct v4l2_control control = {0};
		struct v4l2_queryctrl queryctrl = {0};
		
		// -- Check control	
		// Id
		switch(code) {
			case Saturation: 		control.id = V4L2_CID_SATURATION; 				break;
			case Exposure: 		control.id = V4L2_CID_EXPOSURE_ABSOLUTE; 	break;
			case AutoExposure: 	control.id = V4L2_CID_EXPOSURE_AUTO; 			break;
			
			default: return false;
		}
		
		if(!hvl::queryControl(_fd, control.id, &queryctrl))
			return false;
		
		// Value
		if(code == AutoExposure)
			control.value = value != 0 ? V4L2_EXPOSURE_AUTO : V4L2_EXPOSURE_MANUAL;
		else
			control.value = value;
		
		if (control.value > queryctrl.maximum || control.value < queryctrl.minimum) {
			printf("Set value out of range \n");
			return false;			
		}
		
		// Need change mode ?
		if((code & Exposure) && !(code & Automatic)) {
			struct v4l2_control autoControl = {0};
			autoControl.id 	 = V4L2_CID_EXPOSURE_AUTO;
			autoControl.value = V4L2_EXPOSURE_MANUAL;

			if(hvl::setControl(_fd, &autoControl))
				return false;
		}
		
		// Change value
		if(hvl::setControl(_fd, &control))
			return false;

		return true;
	}
	
	// Getters
	double get(Device::Param code) {
		if(!_open)
			return 0.0;
		
		struct v4l2_control control = {0};
		struct v4l2_queryctrl queryctrl = {0};
		
		// Define id
		if(code & Saturation)
			control.id = V4L2_CID_SATURATION;
		else if(code & Exposure)
			control.id = code & Automatic ? V4L2_CID_EXPOSURE_AUTO : V4L2_CID_EXPOSURE_ABSOLUTE;
		else 
			return 0.0;
		
		// Check control
		if(!hvl::queryControl(_fd, control.id, &queryctrl))
			return 0.0;
		
		// Return value if asked about a limit
		if(code & Minimum)
			return queryctrl.minimum;
		else if(code & Maximum)
			return queryctrl.maximum;
		else if(code & Default)
			return queryctrl.default_value > queryctrl.maximum ? (queryctrl.maximum+queryctrl.minimum)/2 : queryctrl.default_value;
		
		// -- Return the control value if not asked for a limit	
		if(!hvl::getControl(_fd, &control))
			return 0.0;
		
		// Special case
		if(code == AutoExposure)
			return control.value == V4L2_EXPOSURE_MANUAL ? 0 : 1;
		
		return control.value;
	}
	const FrameFormat getFormat() const {
		return _format;
	}
	const std::string& getPath() const {
		return _path;
	}
	
private:
	// Methods
	bool _askFrame() {
		if(!_open)
			return false;
		
		// Already queued
		if(_bufferQueued)
			return true;
		
		if(!hvl::queueBuffer(_fd))
			return false;
		
		// Flag the queue
		_bufferQueued = true;
		return true;	
	}
	
	bool _treat(Gb::Frame& frame) {
		// Timer t;
		bool success = false;
		
		int w = _rawData.size.width;
		int h = _rawData.size.height;
		int area = w*h;
		
		// -- From jpg to h264:
		if(frame.type == Gb::FrameType::H264) {
			// jpg decompress : jpg422 -> yuv422
			_decoderJpg.decode2yuv422(_rawData.buffer, _yuv422Frame, w, h);

			// yuv422 -> yuv420
			if(_yuv420Frame.size() != area*3/2)
				_yuv420Frame.resize(area*3/2);
			
			Convert::yuv422ToYuv420(&_yuv422Frame[0], &_yuv420Frame[0], w, h);

			// h264 encode : yuv420 -> h264 packet
			if(_encoderH264.encodeYuv(&_yuv420Frame[0], frame.buffer))
				success = true;
		}
		
		// -- From jpg to smaller jpg
		else if(frame.type == Gb::FrameType::Jpg420) { 
			std::vector<unsigned char> bgrBuffer;
			
			// jpg decompress : jpg422 -> bgr24
			if(_decoderJpg.decode2bgr24(_rawData.buffer, bgrBuffer, w, h)) {
				// jpg compress : bgr24 -> jpg420
				if(_encoderJpg.encodeBgr24(bgrBuffer, frame.buffer, w, h, 30)) {
					success = true;
				}
			}
		}
		
		// -- From jpg to bgr
		else if(frame.type == Gb::FrameType::Bgr24) { 
			// jpg decompress : jpg422 -> bgr24
			if(_decoderJpg.decode2bgr24(_rawData.buffer, frame.buffer, w, h)) {
				success = true;
			}
		}
		
		// -- Raw
		else {
			frame.buffer = _rawData.buffer;
			frame.type = _rawData.type;
			success = true;
		}
		
		// -- Set final info
		if(success) 
			frame.size = _rawData.size;
		else 
			frame.clear();
		
		// std::cout << t.elapsed_mus()/1000.0 << std::endl;
		return success;
	}
	
	// Members
	int _fd;
	bool _open;
	std::string _path;
	FrameFormat	_format;
	FrameBuffer _buffer;
	Gb::Frame 	_rawData;
	std::atomic<bool> _bufferQueued;
	
	std::vector<unsigned char> _yuv422Frame;
	std::vector<unsigned char> _yuv420Frame;
	EncoderH264 _encoderH264;
	EncoderJpg _encoderJpg;
	DecoderJpg _decoderJpg;
};

#endif