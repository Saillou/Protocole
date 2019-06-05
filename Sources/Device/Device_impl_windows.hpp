#pragma once
#ifdef _WIN32

struct _Impl {
public:
	// Constructors
	explicit _Impl(const std::string& pathVideo) : 
		_isOpen(false),
		_path(pathVideo), 
		_format({640, 480, MJPG}),
		_rawData(nullptr, 0),
		_pFormat(nullptr)		
	{
		CoInitialize(nullptr);
		// Wait for open
	}
	~_Impl() {
		close();
		
		CoUninitialize();
	}
	
	// Methods
	bool open() {
		// Check opening
		if(_format.width > 0 && _format.height > 0 && _path.size() != 1)
			return false;
		
		if(!_translator.setup(_format.width, _format.height))
			return false;
		
		_isOpen = _open((int)(_path[0]-'0'));
		
		if(_isOpen)
			_pMediaControl->Run();
		
		return _isOpen;
	}
	bool close() {
		if(!_isOpen)
			return true;
		
		if(_pMediaControl)
			_pMediaControl->Stop();
		
		_translator.cleanup();
		_isOpen = false;
		
		return true;
	}
	void refresh() {
		_translator.refresh();
	}
	
	bool grab() {
		if(!_isOpen)
			return false;
		
		 // Wait for callback		
		while(!_rendererCallback.updated)
			Timer::wait(1);
		
		// Get the frame
		_rendererCallback.loadBuffer(_rawData.buffer);
		
		return !_rawData.buffer.empty();
	}
	bool retrieve(Gb::Frame& frame) {
		if(!_isOpen)
			return false;
		
		// Convert to expected format
		_rawData.size 	= Gb::Size(_format.width, _format.height);
		_rawData.type 	= (_format.format == MJPG) ? Gb::FrameType::Jpg422 : Gb::FrameType::Yuv422;
		
		return _translator.treat(_rawData, frame);	
	}
	bool read(Gb::Frame& frame) {
		return (grab() && retrieve(frame));
	}
	
	// Setters
	bool setFormat(int width, int height, PixelFormat formatPix) {
		bool wasOpen = _isOpen;
		bool success = false;
		
		if(wasOpen) {
			if(_pMediaControl)
				_pMediaControl->Stop();
			_translator.cleanup();
		}

		if(FAILED(hvw::getFormat(_pDeviceConfig, 640, 480, formatPix == MJPG ? MEDIASUBTYPE_MJPG : MEDIASUBTYPE_YUY2, &_pFormat))) {
			hvw::echo("Can't find format");
			goto setFormatEnd;
		}
		
		if(FAILED(_pDeviceConfig->SetFormat(_pFormat))) {
			hvw::echo("Can't set device format");
			goto setFormatEnd;
		}
		
		if(FAILED(_pRendererGrabber->SetMediaType(_pFormat))) {
			hvw::echo("Can't set grabber format");	
			goto setFormatEnd;
		}
		
		_format.width	= width;
		_format.height = height;
		_format.format = formatPix;
		success = true;
		
setFormatEnd:
		if(wasOpen) {
			_translator.setup(_format.width, _format.height);
			if(_pMediaControl)
				_pMediaControl->Run();
		}
		
		return success;
	}
	bool set(Device::Param code, double value) {	
		if(value < 0.0 || value > 1.0) {
			printf("Warning - Set control : value outside range [0.0; 1.0], will be truncated. \n");
			
			if(value < 0.0) value = 0.0;
			if(value > 1.0) value = 1.0;
		}
		
		switch(code) {
			case Saturation:
				return false;
				
			case Brightness:
				return false;
				
			case Hue:
				return false;
				
			case Contrast:
				return false;
				
			case Whiteness:
				return false;
				
			case Exposure:
				return false;
				
			case AutoExposure:
				return false;
		}
		
		return false;
	}
	
	// Getters
	double get(Device::Param code) {
		switch(code) {
			case Saturation:
				return 0.0;
				
			case Brightness:
				return 0.0;
				
			case Hue:
				return 0.0;
				
			case Contrast:
				return 0.0;
				
			case Whiteness:
				return 0.0;
				
			case Exposure:
				return 0.0;
				
			case AutoExposure:
				return 0.0;
		}
		
		return 0.0;
	}
	
	const FrameFormat getFormat() const {
		return _format;
	}
	
	const std::string& getPath() const {
		return _path;
	}
	
private:
	// Methods
	bool _open(int idCamera) {
		if(_isOpen)
			return true;
		
		// ----- Device graph build -----
		// Create objects
		if(FAILED(CoCreateInstance(CLSID_CaptureGraphBuilder2, nullptr, CLSCTX_INPROC_SERVER, IID_ICaptureGraphBuilder2, (void **)&_pBuild)))
			return hvw::echo("Error builderGraph2");

		if(FAILED(CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void **)&_pGraph)))
			return hvw::echo("Error filterGraph");

		if(FAILED(CoCreateInstance(CLSID_SampleGrabber, nullptr, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void **)&_pRendererFilter)))
			return hvw::echo("Error sampleGrabber");
		
		
		// Device init
		if(FAILED(hvw::getMonikerDevice(0, &_pMonikerDevice)))
			return hvw::echo("Can't find device");
		
		if(FAILED(_pMonikerDevice->BindToObject(0, 0, IID_IBaseFilter, (void**)&_pDeviceFilter)))
			return hvw::echo("Can't bind device");
		
		if(FAILED(_pBuild->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, _pDeviceFilter, IID_IAMStreamConfig, (void **)&_pDeviceConfig)))
			return hvw::echo("Can't find config");

		
		// Query graph interfaces
		if(FAILED(_pGraph->QueryInterface(IID_IMediaControl, (void **)&_pMediaControl)))
			return hvw::echo("Error mediaControl");

		
		// Set renderer configuration
		if(FAILED(_pRendererFilter->QueryInterface(IID_ISampleGrabber, (void **)&_pRendererGrabber)))
			return hvw::echo("Error setRenderer");

		_pRendererGrabber->SetBufferSamples(true);
		_pRendererGrabber->SetCallback(&_rendererCallback, 1); // 0 : Sample | 1 : Buffer
		
		// Add filters
		if(FAILED(_pGraph->AddFilter(_pDeviceFilter, L"Capture_filter")))
			return hvw::echo("Error add capture filter");

		if(FAILED(_pGraph->AddFilter(_pRendererFilter, L"Renderer_filter")))
			return hvw::echo("Error add renderer filter");

		
		// Render
		if(FAILED(_pBuild->SetFiltergraph(_pGraph)))
			return hvw::echo("Error set filter graph ");

		if(FAILED(_pBuild->RenderStream(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, _pDeviceFilter, nullptr, _pRendererFilter)))	
			return hvw::echo("Error renderer stream");	
		
		// Format 
		return setFormat(640, 480, MJPG);
	}
	

	// Members
	bool _isOpen;
	std::string _path;
	FrameFormat	_format;
	Gb::Frame 	_rawData;
	Translator	_translator;

	
	CComPtr<ICaptureGraphBuilder2>	_pBuild;
	CComPtr<IGraphBuilder>				_pGraph;
	CComPtr<IMediaControl>				_pMediaControl;
	CComPtr<IMoniker> 					_pMonikerDevice;
	CComPtr<IBaseFilter>				_pDeviceFilter;
	CComPtr<IAMStreamConfig>			_pDeviceConfig;
	CComPtr<IBaseFilter>				_pRendererFilter;
	CComPtr<ISampleGrabber>			_pRendererGrabber;
	AM_MEDIA_TYPE* 						_pFormat;
	hvw::RendererCallback				_rendererCallback;
};

#endif