#pragma once

// OpenH.264
#include <wels/codec_api.h>
#include "convertColor.hpp"

#include <iostream>
#include <vector>
#include <cstring> // memset

class EncoderH264 {
public:	
	EncoderH264() : _width(0), _height(0), _encoder(nullptr)
	{
		
	}
	~EncoderH264() {
		_cleanEncoder();
	}
	
	bool setup(int width, int height) {
		if(_encoder)
			_cleanEncoder();
		
		_width 	= width;
		_height 	= height;
		
		if(_width < 1 || _height < 1)
			return false;
		
		return _setupEncoder();
	}
	void cleanup() {
		_cleanEncoder();
	}
	
	bool encodeBgr(unsigned char* dataEncode, std::vector<unsigned char>& buffer) {
		if(!_encoder)
			return false;
		
		// First need Yuv
		Convert::bgr24ToYuv420(dataEncode, _pic.pData, _pic.iPicWidth, _pic.iPicHeight);
		
		return _encodeH264(buffer);
	}
	bool encodeYuv(unsigned char* dataEncode, std::vector<unsigned char>& buffer) {
		if(!_encoder)
			return false;
		
		// Put yuvEncode in the _pic
		memcpy(&_yuvBuffer[0], dataEncode, _yuvBuffer.size());
		
		return _encodeH264(buffer);
	}
	
private:
	// Methods
	bool _setupEncoder() {
		// -- Allocate memory
		if(WelsCreateSVCEncoder(&_encoder) != 0) {
			std::cout << "Couldn't create h264 encoder" << std::endl;
			return false;
		}
		
		// Create extension parameters sheet
		SEncParamExt encoderParemeters;
		memset(&encoderParemeters, 0, sizeof(SEncParamExt));

		_encoder->GetDefaultParams(&encoderParemeters); // Default

		encoderParemeters.iUsageType 					= CAMERA_VIDEO_REAL_TIME;
		encoderParemeters.iComplexityMode 				= LOW_COMPLEXITY; // LOW_, MEDIUM_, HIGH_
		encoderParemeters.bEnableFrameCroppingFlag = true;

		encoderParemeters.iRCMode = RC_QUALITY_MODE;
		encoderParemeters.iMinQp 	= 12;
		encoderParemeters.iMaxQp 	= 42;

		encoderParemeters.bEnableBackgroundDetection 	= true;
		encoderParemeters.bEnableFrameSkip 				= true;
		encoderParemeters.bEnableLongTermReference 	= true;
		encoderParemeters.iSpatialLayerNum 				= 1;

		// Layer param
		SSpatialLayerConfig *spartialLayerConfiguration = &encoderParemeters.sSpatialLayers[0];
		spartialLayerConfiguration->uiProfileIdc = PRO_UNKNOWN;
		encoderParemeters.iPicWidth 			= spartialLayerConfiguration->iVideoWidth 			= _width;
		encoderParemeters.iPicHeight 		= spartialLayerConfiguration->iVideoHeight 			= _height;
		encoderParemeters.fMaxFrameRate 	= spartialLayerConfiguration->fFrameRate 			= 1/30.0f;
		encoderParemeters.iTargetBitrate 	= spartialLayerConfiguration->iSpatialBitrate 	= 100000;
		encoderParemeters.iTargetBitrate 	= spartialLayerConfiguration->iMaxSpatialBitrate = 100000;
		
		// Color space
		int videoFormat = videoFormatI420;
		_encoder->SetOption(ENCODER_OPTION_DATAFORMAT, &videoFormat);
		
		// Init
		if(_encoder->InitializeExt(&encoderParemeters) != 0) {
			std::cout << "Couldn't init encoder" << std::endl;
			return false;
		}
		
		// Source
		memset(&_pic, 0, sizeof(SSourcePicture));
		
		_pic.iPicWidth 		= _width;
		_pic.iPicHeight 		= _height;
		_pic.iColorFormat	= videoFormat;
		
		_pic.iStride[0] = _pic.iPicWidth;
		_pic.iStride[1] = _pic.iPicWidth >> 1;
		_pic.iStride[2] = _pic.iPicWidth >> 1;
		
		size_t area = _width*_height;
		
		_yuvBuffer.resize(area + (area>>1)); 	// Y = area, U = area/4, area/4
		_pic.pData[0] 	= &_yuvBuffer[0];							// Begin array
		_pic.pData[1] 	= &_yuvBuffer[area]; 					// End Y 
		_pic.pData[2] 	= &_yuvBuffer[area + (area>>2)];	// End U (Y + U => area + area/4)
		
		return true;
	}
	void _cleanEncoder() {
		if (_encoder) {
			_encoder->Uninitialize();
			WelsDestroySVCEncoder(_encoder);
		}
		_encoder = nullptr;
		_width = 0;
		_height = 0;
		_yuvBuffer.clear();
	}
	
	bool _encodeH264(std::vector<unsigned char>& buffer) {
		if(!_encoder)
			return false;
		
		// Info about encoding result
		SFrameBSInfo encInfo;
		memset (&encInfo, 0, sizeof(SFrameBSInfo));
		
		if(_encoder->EncodeFrame(&_pic, &encInfo) == 0) {		
			if (encInfo.eFrameType != videoFrameTypeSkip && encInfo.eFrameType != videoFrameTypeInvalid) {
				// Create buffer result:
				unsigned char* start = encInfo.sLayerInfo->pBsBuf;
				int size 				= encInfo.iFrameSizeInBytes;
				buffer = std::vector<unsigned char>(start, start + size);
				
				return true;
			}
		}
		return false;
	}
	
	// Members
	int _width;
	int _height;
	ISVCEncoder* _encoder;
	
	SSourcePicture _pic;
	std::vector<unsigned char> _yuvBuffer;
	
};