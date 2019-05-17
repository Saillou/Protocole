#pragma once
#ifdef __linux__

#include "Device.hpp"

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
		_path(pathVideo), 
		_format({0, 0, 0}),
		_buffer({(void*)nullptr, (size_t)0}) 
	{
		// Wait open
	}
	~_Impl() {
		if(_fd != -1)
			close();		
	}
	
	// Methods
	bool open() {
		_fd = ::open(_path.c_str(), O_RDWR | O_NONBLOCK, 0);
		
		if(_fd == -1 || !_initDevice() || !_initMmap()) {
			_perror("Opening device");
			if(_fd != -1) 
				::close(_fd);
				
			return false;
		}
		
		return true;		
	}
	bool close() {
		// Stop capture
		enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if(_xioctl(_fd, VIDIOC_STREAMOFF, &type) == -1) {
			_perror("Stop Capture");
			return false;
		}
		
		if(munmap(_buffer.start, _buffer.length) == -1) {
			_perror("Memory unmap");
			return false;
		}
		
		if(_fd != -1) {
			::close(_fd);
			_fd = -1;
		}
		return true;		
	}
	
	bool grab() {
		struct v4l2_buffer buf = {0};
		buf.type 	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory 	= V4L2_MEMORY_MMAP;
				
		for(;;) {
			// Wait event on fd
			struct pollfd fdp;
			fdp.fd 			= _fd;
			fdp.events 		= POLLIN | POLLOUT; // inputs
			fdp.revents		= 0; // outputs
			
			int r = poll(&fdp, 1, 1000);
			
			// Error ?
			if(r < 1) {
				if(r == -1) {
					if(EINTR == errno) { // Interrupted 
						printf(".\n");
						continue;
					}
					_perror("Waiting for Frame");
				}
				else if(r == 0) {				
					_perror("Timeout"); 
				}
				
				return false;
			}
		
			// Grab frame
			if(_xioctl(_fd, VIDIOC_DQBUF, &buf) == -1) {
				if(EAGAIN == errno)
					continue;
					
				_perror("Grab Frame");
				return false;
			}
			
			// Check size
			_buffer.length = (buf.bytesused > 0) ? buf.bytesused : _buffer.length;	
			return true;
		}
		return false;		
	}
	bool retrieve(Gb::Frame& frame) {
		_rawData = Gb::Frame(
			reinterpret_cast<unsigned char*>(_buffer.start), 
			static_cast<unsigned long>(_buffer.length),
			Gb::Size(_format.width, _format.height)
		).clone();
		
		_askFrame();
			
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
		_format.format = formatPix == Device::MJPG ? V4L2_PIX_FMT_MJPEG : V4L2_PIX_FMT_YUYV;
		
		return open();
	}
	bool set(Device::Param code, double value) {
		struct v4l2_control control = {0};
		struct v4l2_queryctrl queryctrl = {0};
		
		// -- Check control		
		switch(code) {
			case Saturation: 		control.id = V4L2_CID_SATURATION; 				break;
			case Exposure: 		control.id = V4L2_CID_EXPOSURE_ABSOLUTE; 	break;
			
			default: return false;
		}
		if (_isControl(control.id, &queryctrl) < 0) {
			_perror("Setting Control");
			return false;
		}
		
		if (value > queryctrl.maximum || value < queryctrl.minimum) {
			_perror("Set value out of range");
			return false;			
		}
		
		// Need change mode ?
		if(code & Exposure) {
			struct v4l2_control autoControl = {0};
			autoControl.id = V4L2_CID_EXPOSURE_AUTO;
			
			if(code & Automatic)
				autoControl.value = V4L2_EXPOSURE_AUTO;
			else 
				autoControl.value = V4L2_EXPOSURE_MANUAL;

			if (_xioctl(_fd, VIDIOC_S_CTRL, &control) == -1) {
				_perror("Changing Mode");
				return false;
			}				
		}
		
		control.value = value;
		if (_xioctl(_fd, VIDIOC_S_CTRL, &control) == -1) {
			_perror("Setting Control");
			return false;
		}
		return true;
	}
	
	// Getters
	double get(Device::Param code) {
		struct v4l2_control control = {0};
		struct v4l2_queryctrl queryctrl = {0};
		
		// Define id
		if(code & Saturation)
			control.id = V4L2_CID_SATURATION;
		else if(code & Exposure)
			control.id = V4L2_CID_EXPOSURE_ABSOLUTE;
		else 
			return 0.0;
		
		// Check control
		if (_isControl(control.id, &queryctrl) < 0) {
			_perror("Getting Control");
			return 0.0;
		}
		
		// Return value if asked about a limit
		if(code & Minimum)
			return queryctrl.minimum;
		else if(code & Maximum)
			return queryctrl.maximum;
		else if(code & Default)
			return queryctrl.default_value > queryctrl.maximum ? (queryctrl.maximum+queryctrl.minimum)/2 : queryctrl.default_value;
		else if(code & Automatic)
			return 0.0;
		
		// -- Return value if not a limit	
		if (_xioctl(_fd, VIDIOC_G_CTRL, &control) == -1) {
			_perror("Getting Control");
			return 0.0;
		}
		
		return control.value;
	}
	const Gb::Size getSize() const {
		return Gb::Size(_format.width, _format.height);
	}
	
private:		
	// Statics
	static int _xioctl(int fd, int request, void *arg) {
		int r(-1);
		do {
			r = ioctl (fd, request, arg);
		} while (r == -1 && EINTR == errno);
		
		return r;	
	}
	
	// Methods
	bool _initDevice() {
		// Format
		struct v4l2_fmtdesc fmtdesc = {0};
		fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		
		char fourcc[5] = {0};
		char c, e;
		int support_grbg10 = 0;
		printf("  Format \n--------------------\n");
		while (_xioctl(_fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
			strncpy(fourcc, (char *)&fmtdesc.pixelformat, 4);
			if (fmtdesc.pixelformat == V4L2_PIX_FMT_SGRBG10)
				support_grbg10 = 1;
				
			c = fmtdesc.flags & 1? 'C' : ' ';
			e = fmtdesc.flags & 2? 'E' : ' ';
			printf("  %s: %c%c %s\n", fourcc, c, e, fmtdesc.description);
			fmtdesc.index++;
		}
		
		struct v4l2_format fmt = {0};
		if(_format.width == 0 || _format.height == 0) {
			fmt.type 					= V4L2_BUF_TYPE_VIDEO_CAPTURE;
			fmt.fmt.pix.width 		= 640;
			fmt.fmt.pix.height 		= 480;
			fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
			fmt.fmt.pix.field 		= V4L2_FIELD_ANY;
			
			_format.width  = fmt.fmt.pix.width;
			_format.height = fmt.fmt.pix.height;
			_format.format = fmt.fmt.pix.pixelformat;
		}
		else {
			fmt.type 					= V4L2_BUF_TYPE_VIDEO_CAPTURE;
			fmt.fmt.pix.width 		= _format.width;
			fmt.fmt.pix.height 		= _format.height;
			fmt.fmt.pix.pixelformat = _format.format;
			fmt.fmt.pix.field 		 = V4L2_FIELD_ANY;			
		}
		
		if (_xioctl(_fd, VIDIOC_S_FMT, &fmt) == -1) {
			_perror("Setting Pixel Format");
			return false;
		}
	 
		strncpy(fourcc, (char *)&fmt.fmt.pix.pixelformat, 4);
		printf( "Selected Camera Mode:\n--------------------\n   Width: %d\n  Height: %d\n PixFmt: %s\n  Field: %d\n",
					fmt.fmt.pix.width, fmt.fmt.pix.height, fourcc, fmt.fmt.pix.field);

		return true;		
	}
	bool _initMmap() {
		// Init buffers
		struct v4l2_requestbuffers req = {0};
		req.count 	= 1;
		req.type 	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		req.memory 	= V4L2_MEMORY_MMAP;
	 
		if (_xioctl(_fd, VIDIOC_REQBUFS, &req) == -1) {
			_perror("Requesting Buffer");
			return false;
		}
	 
		struct v4l2_buffer buf = {0};
		buf.type 	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory 	= V4L2_MEMORY_MMAP;
		buf.index 	= 0;
		
		if(-1 == _xioctl(_fd, VIDIOC_QUERYBUF, &buf)) {
			_perror("Querying Buffer");
			return false;
		}
	 
		// Memory map
		_buffer.start 		= mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, _fd, buf.m.offset);
		_buffer.length 	= buf.bytesused > 0 ? buf.bytesused : buf.length;
		if(_buffer.start == MAP_FAILED) {
			_perror("Mapping");
			return false;    
		}
		printf("Length: %d\n", _buffer.length);
		
		
		// Start capture
		if(!_askFrame())
			return false;
	 
		if(_xioctl(_fd, VIDIOC_STREAMON, &buf.type) == -1) {
			_perror("Start Capture");
			return false;
		}
		
		return true;		
	}
	bool _askFrame() {
		struct v4l2_buffer buf = {0};
		buf.type 	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory 	= V4L2_MEMORY_MMAP;
		
		if(_xioctl(_fd, VIDIOC_QBUF, &buf) == -1) {
			_perror("Query Buffer");
			return false;
		}
		return true;	
	}
	
	bool _treat(Gb::Frame& frame) {
		frame = _rawData.clone();
		return !frame.empty();
	}
	
	int _isControl(int control, struct v4l2_queryctrl* queryctrl) {
		int err = 0;
		queryctrl->id = control;
		if ((err = ioctl(_fd, VIDIOC_QUERYCTRL, queryctrl)) < 0)
			printf("ioctl querycontrol error %d \n", errno);
		else if (queryctrl->flags & V4L2_CTRL_FLAG_DISABLED)
			printf("control %s disabled \n", (char*) queryctrl->name);
		else if (queryctrl->flags & V4L2_CTRL_TYPE_BOOLEAN)
			return 1;
		else if (queryctrl->type & V4L2_CTRL_TYPE_INTEGER)
			return 0;
		else
			printf("contol %s unsupported  \n", (char*) queryctrl->name);
		return -1;
	}
	
	void _perror(const std::string& message) const {
		std::string mes = " [" + _path + ", " + std::to_string(_fd) + "] " + message + " - Errno: " +  std::to_string(errno);
		perror(mes.c_str());	
	}
	
	// Members
	int _fd;
	std::string _path;
	FrameFormat	_format;
	FrameBuffer _buffer;
	Gb::Frame 	_rawData;
};

#endif