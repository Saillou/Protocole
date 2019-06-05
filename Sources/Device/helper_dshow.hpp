#pragma once
#ifdef _WIN32

// Use dshow
#include <atlbase.h>
#include <dshow.h>

#include <iostream>
#include <vector>
#include <utility>
#include <mutex>

#include "qedit.h"

namespace hvw {
	// ------ Structues ------
	// Callback for grabber
	struct RendererCallback : public ISampleGrabberCB {
		RendererCallback() : _lCount(1), updated(false) {
		}
		~RendererCallback() {
			_lCount = 0;
			updated = false;
			buffer.clear();
		}
		
		// Methods
		HRESULT STDMETHODCALLTYPE QueryInterface(const IID &rInterfaceID, void **ppObject) override {
			if (ppObject == nullptr) return E_POINTER;

			// Cast
			*ppObject = nullptr;
			if (IsEqualIID(rInterfaceID, __uuidof(IUnknown))) {
				*ppObject = static_cast<IUnknown *>(this);
			}
			else if (IsEqualIID(rInterfaceID, __uuidof(ISampleGrabberCB))) {
				*ppObject = static_cast<ISampleGrabberCB *>(this);
			}

			// Incremente
			if (*ppObject != nullptr) {
				static_cast<IUnknown *>(*ppObject)->AddRef();
				return S_OK;
			}

			// Failed
			return E_NOINTERFACE;
		}
		ULONG STDMETHODCALLTYPE AddRef() override {
			return InterlockedIncrement(&_lCount);
		}
		ULONG STDMETHODCALLTYPE Release() override {
			if (!InterlockedDecrement(&_lCount)) {
				delete this;
				return 0;
			}
			return _lCount;
		}
		HRESULT STDMETHODCALLTYPE SampleCB(double dTime, IMediaSample *pSample) override {
			// IGrabber::setCallback : Param 0
			return S_OK;		
		}
		HRESULT STDMETHODCALLTYPE BufferCB(double dTime, BYTE *pBuffer, long lSize) override {
			// IGrabber::setCallback : Param 1
			updated = true;
			
			std::lock_guard<std::mutex> lockBuffer(_mut);
			buffer = std::vector<unsigned char>(pBuffer, pBuffer+lSize);
			
			return S_OK;
		}
		
		void loadBuffer(std::vector<unsigned char>& bufferOut) {
			updated = false;
			
			std::lock_guard<std::mutex> lockBuffer(_mut);
			bufferOut = std::move(buffer);
		}
		
		// Members
		std::atomic<bool> updated;
		std::vector<unsigned char> buffer;
		
	private:
		ULONG _lCount;
		std::mutex _mut;
	};
	
	
	// ---- Functions -----
	// Get a moniker (help for describing) object for the corresponding device id
	HRESULT getMonikerDevice(const int cameraId, IMoniker** ppMonikerDevice) {
		*ppMonikerDevice = nullptr;
		
		// Create enum system
		CComPtr<ICreateDevEnum> pSysDevEnum = nullptr;
		if(FAILED(CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void **)&pSysDevEnum)))
			return E_FAIL;
		
		// Create device enum
		CComPtr<IEnumMoniker> pEnumCatDevice = nullptr;
		if(FAILED(pSysDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnumCatDevice, NULL)))
			return E_FAIL;
		
		// Get device descriptor
		pEnumCatDevice->Skip(cameraId);
		if(FAILED(pEnumCatDevice->Next(1, ppMonikerDevice, nullptr)))
			return E_FAIL;
		
		return S_OK;
	}

	// Seach a corresponding format from the choosen device
	HRESULT getFormat(CComPtr<IAMStreamConfig> pDeviceConfig, const long width, const long height, const GUID type, AM_MEDIA_TYPE** ppFormat) {
		*ppFormat = nullptr;
		
		// Enum formats
		int iCount, iSize;
		if(FAILED(pDeviceConfig->GetNumberOfCapabilities(&iCount, &iSize)))
			return E_FAIL;
		
		// Read video config
		VIDEO_STREAM_CONFIG_CAPS fmtConfig;
		
		for(int iType = 0; iType < iCount; iType++) {
			if(FAILED(pDeviceConfig->GetStreamCaps(iType, ppFormat, reinterpret_cast<BYTE*>(&fmtConfig))))
				return E_FAIL;
			
			// Check
			AM_MEDIA_TYPE* pFormat = *ppFormat;
			if(pFormat->formattype != FORMAT_VideoInfo || pFormat->cbFormat < sizeof(VIDEOINFOHEADER))
				continue;
			
			// Compare and choose
			if(type == pFormat->subtype && width == HEADER(pFormat->pbFormat)->biWidth && height == HEADER(pFormat->pbFormat)->biHeight)
				return S_OK;
		}
		
		*ppFormat = nullptr;
		return E_FAIL;
	}
	
	// Just write a message
	bool echo(const std::string& msg) {
		printf("Error : %s", msg.c_str());
		return false;
	}
}

#endif