#pragma once

// OpenH.264
#include <wels/codec_api.h>
#include "convertColor.hpp"

#include <atomic>
#include <iostream>
#include <vector>
#include <cstring> // memset

class DecoderH264 {
public:	
	DecoderH264() : _decoder(nullptr)
	{
		
	}
	~DecoderH264() {
		_cleanDecoder();
	}
	
	bool setup() {
		if(_decoder)
			_cleanDecoder();

		return _setupDecoder();
	}
	void cleanup() {
		_cleanDecoder();
	}
	
	bool decode(const std::vector<unsigned char>& dataIn, std::vector<unsigned char>& dataOut) {
		return _decodeH264(dataIn, dataOut);
	}
	
private:
	// Methods
	bool _setupDecoder() {
		if(WelsCreateDecoder(&_decoder) != 0)
			return false;
		
		SDecodingParam decParam;
		memset(&decParam, 0, sizeof (SDecodingParam));
		
		decParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;
		
		if(_decoder->Initialize(&decParam) != 0)
			return false;
		
		return true;
	}
	void _cleanDecoder() {
		if (_decoder) {
			_decoder->Uninitialize();
			WelsDestroyDecoder(_decoder);
		}
		_decoder = nullptr;
	}
	
	bool _decodeH264(const std::vector<unsigned char>& dataIn, std::vector<unsigned char>& dataOut) {
		if(!_decoder)
			return false;
		
		// --- Decode ---
		unsigned char* yuvDecode[3];
		memset(yuvDecode, 0, sizeof (yuvDecode));
		
		SBufferInfo decInfo;
		memset(&decInfo, 0, sizeof (SBufferInfo));
		
		int err = _decoder->DecodeFrame2 (&dataIn[0], (int)dataIn.size(), yuvDecode, &decInfo);
		if(err == 0 && decInfo.iBufferStatus == 1) {					
			int oStride = decInfo.UsrData.sSystemBuffer.iStride[0];
			int oWidth 	= decInfo.UsrData.sSystemBuffer.iWidth;
			int oHeight = decInfo.UsrData.sSystemBuffer.iHeight;
		
			size_t area = (size_t)(oWidth*oHeight*3);
			if(dataOut.size() != area)
				dataOut.resize(area);
			
			Convert::yuv420ToBgr24(yuvDecode, &dataOut[0], oStride, oWidth, oHeight);
			return true;
		}
		return false;
	}
	
	// Members
	ISVCDecoder *_decoder;
};