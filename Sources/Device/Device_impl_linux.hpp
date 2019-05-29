#pragma once
#ifdef __linux__

#include "Device.hpp"
#include "Decoder.hpp"

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
		_format({640, 480, MJPG}),
		_buffer({(void*)nullptr, (size_t)0}),
		_bufferQuery(false)
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
		
		if(_fd == -1 || !_initDevice() || !_initMmap() || !_askFrame()) {
			close();
			return false;
		}
		
		return true;		
	}
	bool close() {
		if(_bufferQuery)
			grab();
		
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
			if(::close(_fd) < 0) {
				_perror("Closing");
				return false;
			}
		}
		
		_encoderH264.cleanup();
		_decoderJpg.cleanup();
		_encoderJpg.cleanup();
		
		_buffer.start = nullptr;
		_buffer.length = 0;
		_fd = -1;
		std::cout << "Device closed" << std::endl;
		return true;		
	}
	void refresh() {
		_encoderH264.refresh();
	}
	
	bool grab() {
		struct v4l2_buffer buf = {0};
		buf.type 	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory 	= V4L2_MEMORY_MMAP;
				
		struct pollfd fdp;
		fdp.fd 			= _fd;
		fdp.events 		= POLLIN | POLLOUT; // inputs
		fdp.revents		= 0; // outputs
		
		bool success = false;
		
		if(!_bufferQuery)
			_askFrame();
			
		for(int error = 0; error < 10;) {
			// Wait event on fd
			int r = poll(&fdp, 1, 1000); // 1s
			
			// Error ?
			if(r < 1) {
				if(r == -1) {
					if(EINTR == errno) { // Interrupted 
						printf(".\n");
						Timer::wait(1);
						error++;
						continue;
					}
					_perror("Waiting for Frame");
				}
				else if(r == 0) {				
					_perror("Timeout"); 
				}
				
				break;
			}
		
			// Grab frame
			if(_xioctl(_fd, VIDIOC_DQBUF, &buf) == -1) {
				if(EAGAIN == errno) {
					Timer::wait(1);
					error++;
					continue;
				}
				
				_perror("Grab Frame");
				break;
			}
			
			// Check size
			_buffer.length = (buf.bytesused > 0) ? buf.bytesused : _buffer.length;	
			success = true;
			break;
		}
		
		_bufferQuery = false;
		return success;		
	}
	bool retrieve(Gb::Frame& frame) {
		_rawData = Gb::Frame(
			reinterpret_cast<unsigned char*>(_buffer.start), 
			static_cast<unsigned long>(_buffer.length),
			Gb::Size(_format.width, _format.height),
			(_format.format == MJPG) ? Gb::FrameType::Jpg422 : Gb::FrameType::Yuv422
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
		_format.format = formatPix;
		return open();
	}
	bool set(Device::Param code, double value) {
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
		if (_isControl(control.id, &queryctrl) < 0) {
			_perror("Setting Control");
			return false;
		}
		
		// Value
		if(code == AutoExposure)
			control.value = value != 0 ? V4L2_EXPOSURE_AUTO : V4L2_EXPOSURE_MANUAL;
		else
			control.value = value;
		
		if (control.value > queryctrl.maximum || control.value < queryctrl.minimum) {
			_perror("Set value out of range");
			return false;			
		}
		
		// Need change mode ?
		if((code & Exposure) && !(code & Automatic)) {
			struct v4l2_control autoControl = {0};
			autoControl.id 	 = V4L2_CID_EXPOSURE_AUTO;
			autoControl.value = V4L2_EXPOSURE_MANUAL;

			if (_xioctl(_fd, VIDIOC_S_CTRL, &autoControl) == -1) {
				_perror("Changing Mode");
				return false;
			}	
		}
		
		// Change value
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
			control.id = code & Automatic ? V4L2_CID_EXPOSURE_AUTO : V4L2_CID_EXPOSURE_ABSOLUTE;
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
		
		// -- Return value if not a limit	
		if (_xioctl(_fd, VIDIOC_G_CTRL, &control) == -1) {
			_perror("Getting Control");
			return 0.0;
		}
		
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
	// Statics
	static int _xioctl(int fd, int request, void *arg) {
		int r(-1);
		do {
			r = ioctl (fd, request, arg);
		} while (r == -1 && errno == EINTR);
		
		return r;	
	}
	
	// Methods
	bool _initDevice() {
		// -- Set format --
		std::cout << "Set: [" << _format.width << "x" << _format.height << "]" << std::endl;
		
		struct v4l2_format fmt = {0};
		fmt.type 						= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		fmt.fmt.pix.pixelformat 	= V4L2_PIX_FMT_MJPEG;
		fmt.fmt.pix.field 			= V4L2_FIELD_ANY;
		
		if(_format.width == 0 || _format.height == 0) {
			fmt.fmt.pix.width 	= 640;
			fmt.fmt.pix.height 	= 480;
			
			_format.width 		= fmt.fmt.pix.width;
			_format.height	= fmt.fmt.pix.height;
			_format.format	= MJPG;
		}
		else {
			fmt.fmt.pix.width 	= _format.width;
			fmt.fmt.pix.height 	= _format.height;
		}
		
		if (_xioctl(_fd, VIDIOC_S_FMT, &fmt) == -1) {
			_perror("Setting Pixel Format");
			return false;
		}
		
		printf( "Camera opening: Width: %d | Height: %d \n", fmt.fmt.pix.width, fmt.fmt.pix.height);
					
		// -- Tools
		if(!_encoderH264.setup(_format.width, _format.height)) {
			_perror("Setting H264 encoder");
			return false;
		}
		_decoderJpg.setup();
		_encoderJpg.setup();

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
		
		if(_xioctl(_fd, VIDIOC_QUERYBUF, &buf) == -1) {
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
		printf("Buffer max: %f KB\n", _buffer.length/1000.0f);
		
		// Start capture	 
		if(_xioctl(_fd, VIDIOC_STREAMON, &buf.type) == -1) {
			_perror("Start Capture");
			return false;
		}
		
		return true;		
	}
	
	bool _askFrame() {
		if(_bufferQuery)
			return true;
		
		struct v4l2_buffer buf = {0};
		buf.type 	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory 	= V4L2_MEMORY_MMAP;
		
		if(_xioctl(_fd, VIDIOC_QBUF, &buf) == -1) {
			_perror("Query Buffer");
			return false;
		}
		_bufferQuery = true;
		
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
	std::atomic<bool> _bufferQuery;
	
	std::vector<unsigned char> _yuv422Frame;
	std::vector<unsigned char> _yuv420Frame;
	EncoderH264 _encoderH264;
	EncoderJpg _encoderJpg;
	DecoderJpg _decoderJpg;
};

#endif