#include <iostream>
#include <csignal>
#include <atomic>
#include <map>
#include <deque>

#include "StreamDevice/ClientDevice.hpp"
#include "Tool/Timer.hpp"
#include "Tool/FrameMt.hpp"

#include <wels/codec_api.h>
#include <turbojpeg.h>

namespace Globals {
	// Constantes
	const std::string IP_ADDRESS = "192.168.11.24"; 	// Barnacle V4
	// const std::string IP_ADDRESS = "127.0.0.1"; 	// localhost V4
	
	// const std::string IP_ADDRESS = "fe80::b18:f81d:13a8:3a4"; 		// Barnacle V6
	// const std::string IP_ADDRESS = "::1"; 									// localhost V6
	
	// Variables
	volatile std::sig_atomic_t signalStatus = 0;
}

// --- Signals ---
static void sigintHandler(int signal) {
	Globals::signalStatus = signal;
}

// --- Entry point ---
int main(int argc, char* argv[]) {	
	// - Install signal handler
	std::signal(SIGINT, sigintHandler);
	
	// Alloc decoder
	ISVCDecoder *decoder;
	WelsCreateDecoder(&decoder);
	
	SDecodingParam decParam;
	memset(&decParam, 0, sizeof (SDecodingParam));
	
	decParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;
	decoder->Initialize(&decParam);
	
	// --- Devices
	cv::Mat cvFrame0 = cv::Mat::zeros(480, 640, CV_8UC3);
	std::mutex frameMut0;
	
	ClientDevice device0(IAddress(Globals::IP_ADDRESS, 8888));
	
	std::atomic<size_t> bitrate = 0;
	
	// ----- Events -----
	tjhandle _jpgDecompressor = tjInitDecompress();
	
	device0.onFrame([&](const Gb::Frame& frame) {
		bitrate += frame.length();
		
		// --- Decode ---
		unsigned char* yuvDecode[3];
		memset(yuvDecode, 0, sizeof (yuvDecode));
		
		SBufferInfo decInfo;
		memset(&decInfo, 0, sizeof (SBufferInfo));
		
		int err = decoder->DecodeFrame2 (frame.start(), frame.length(), yuvDecode, &decInfo);
		if(err == 0 && decInfo.iBufferStatus == 1) {					
			int oStride = decInfo.UsrData.sSystemBuffer.iStride[0];
			int oWidth 	= decInfo.UsrData.sSystemBuffer.iWidth;
			int oHeight = decInfo.UsrData.sSystemBuffer.iHeight;
			
			std::lock_guard<std::mutex> frameLock(frameMut0);
			Convert::yuv420ToBgr24(yuvDecode, cvFrame0.data, oStride, oWidth, oHeight);
		}
	});
	
	// -------- Main loop --------  
	if(!device0.open(-1)) {
		std::cout << "Can't open device" << std::endl;
		std::cout << "Press a key to continue..." << std::endl;
		return std::cin.get();
	}
	
	Timer t;
	while(Globals::signalStatus != SIGINT && cv::waitKey(10) != 27) {
		std::lock_guard<std::mutex> frameLock(frameMut0);
		if(!cvFrame0.empty())
			cv::imshow("Camera", cvFrame0);
		
		if(t.elapsed_mus() > 1000000) {
			std::cout << 8.0*bitrate/t.elapsed_mus() << "MB/s" << std::endl;
			bitrate = 0;
			t.beg();
		}
	}

	// -- End
	device0.close();
	cv::destroyAllWindows();
	if (decoder) {
		decoder->Uninitialize();
		WelsDestroyDecoder(decoder);
	}
	
	
	std::cout << "Clean exit" << std::endl;
	std::cout << "Press a key to continue..." << std::endl;
	return std::cin.get();
}