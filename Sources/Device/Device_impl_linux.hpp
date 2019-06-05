#pragma once
#ifdef __linux__

struct _Impl {	
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
		if(!_translator.setup(_format.width, _format.height))
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
		
		if(!hvl::stopCapture(_fd))
			goto failed;
		
		if(!hvl::memoryUnmap(_fd, &_buffer.start, _buffer.length))
			goto failed;
		
		if(!hvl::freeBuffer(_fd))
			goto failed;
		
		if(!hvl::closefd(_fd))
			goto failed;

		_translator.cleanup();
		
		
		// ----- Success -----
		_open = false;
		_bufferQueued = false;
		return true;		
	
		// ----- Failed -----
	failed:
		_fd = -1;
		_open = false;
		return false;
	}
	void refresh() {
		_translator.refresh();
	}
	
	bool grab() {
		if(!_open)
			return false;
		
		struct v4l2_buffer buf = {0};
		struct pollfd fdp;
		fdp.fd = _fd;
		
		// Wait event on fd
		for(int iteration = 0; iteration < 50; Timer::wait(1)) {
			fdp.events 		= POLLIN; // inputs
			fdp.revents		= 0; // outputs
			
			int err = poll(&fdp, 1, 1000);
			if(err > 0) // Success
				break;
			else if(err < 0) // Error
				return false;
		}
		if(fdp.revents != POLLIN)
			return false;
	
		// Grab frame
		_bufferQueued = false;
		
		if(!hvl::dequeueBuffer(_fd, buf)) 
			return false;
		
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
		
		_askFrame();
			
		return _translator.treat(_rawData, frame);		
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
		
		if(value < 0.0 || value > 1.0) {
			printf("Warning - Set control : value outside range [0.0; 1.0], will be truncated. \n");
			
			if(value < 0.0) value = 0.0;
			if(value > 1.0) value = 1.0;
		}
		
		struct v4l2_control control		= {0};
		struct v4l2_queryctrl queryctrl	= {0};
		
		// Check control	
		if(!_getControlId(code, control.id))
			return false;
		
		if(!hvl::queryControl(_fd, control.id, &queryctrl))
			return false;
		
		// Value
		if(code == AutoExposure)
			control.value = value != 0 ? V4L2_EXPOSURE_AUTO : V4L2_EXPOSURE_MANUAL;
		else if(code == Exposure) {
			double minVal = std::log2((double)queryctrl.minimum);
			double maxVal = std::log2((double)queryctrl.maximum);
			double scaledVal = value * (maxVal - minVal) + minVal;
			control.value = std::exp2(scaledVal);
			
			if(control.value < queryctrl.minimum)
				control.value = queryctrl.minimum;
			
			if(control.value > queryctrl.maximum)
				control.value = queryctrl.maximum;
			
			// std::cout << "Set: " << value << " -> " << control.value << std::endl;
		}
		else {
			control.value = value * (queryctrl.maximum - queryctrl.minimum) + queryctrl.minimum;
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
		
		struct v4l2_control control 		= {0};
		struct v4l2_queryctrl queryctrl	= {0};
		
		// Check control
		if(!_getControlId(code, control.id))
			return 0.0;
		
		if(!hvl::queryControl(_fd, control.id, &queryctrl))
			return 0.0;
		
		// Return value between 0.0 and 1.0
		if(!hvl::getControl(_fd, &control))
			return 0.0;
		
		if(queryctrl.maximum == queryctrl.minimum)
			return 1.0;
		
		// Value
		if(code == AutoExposure)
			return (control.value == V4L2_EXPOSURE_AUTO) ? 1.0 : 0.0;
		else if(code == Exposure) {
			double minVal = std::log2((double)queryctrl.minimum);
			double maxVal = std::log2((double)queryctrl.maximum);
			double scaledVal = std::log2((double)control.value);
			double value = (scaledVal - minVal) / (maxVal - minVal);
			
			// std::cout << "Get: " << control.value << " -> " << value << std::endl;
			return value;
		}
		else
			return (double)(control.value - queryctrl.minimum) / (queryctrl.maximum - queryctrl.minimum);
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
		if(_fd < 0)
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
	
	bool _getControlId(const Param code, unsigned int& id) {
		id = 0;
		
		// Get v4l2 id
		if(code == Saturation)
			id = V4L2_CID_SATURATION;
		
		else if(code == Brightness)
			id = V4L2_CID_BRIGHTNESS;		
		
		else if(code == Hue)
			id = V4L2_CID_HUE;		
		
		else if(code == Contrast)
			id = V4L2_CID_CONTRAST;		
		
		else if(code == Whiteness)
			id = V4L2_CID_WHITENESS;
		
		else if(code == Exposure)
			id = V4L2_CID_EXPOSURE_ABSOLUTE;	
		
		else if(code == AutoExposure)
			id = V4L2_CID_EXPOSURE_AUTO;
		else 
			return false;
		
		return true;
	}
	
	// Members
	int _fd;
	bool _open;
	std::string _path;
	FrameFormat	_format;
	FrameBuffer _buffer;
	Gb::Frame 	_rawData;
	Translator _translator;
	std::atomic<bool> _bufferQueued;
};

#endif