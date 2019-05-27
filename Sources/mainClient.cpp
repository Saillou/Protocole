#include <iostream>
#include <csignal>
#include <atomic>
#include <map>
#include <deque>

#include "StreamDevice/ClientDevice.hpp"
#include "Timer.hpp"

#ifdef __linux__
	// Shall die

#elif _WIN32
	// Based on Opencv
	#include <opencv2/core.hpp>	
	#include <opencv2/videoio.hpp>	
	#include <opencv2/highgui.hpp>
	#include <opencv2/imgproc.hpp>
	#include <opencv2/imgcodecs.hpp>
	
	// Decode jpg
	#include <turbojpeg.h>
	
#endif

namespace Globals {
	// Constantes
	const std::string IP_ADDRESS = "192.168.11.24"; 	// Barnacle V4
	// const std::string IP_ADDRESS = "127.0.0.1"; 	// localhost V4
	const int PORT = 8888;										// Port v4
	
	// const std::string IP_ADDRESS = "fe80::b18:f81d:13a8:3a4"; 	// Barnacle V6
	// const std::string IP_ADDRESS = "::1"; 									// localhost V6
	// const int PORT = 8889;															// Port v6
	
	// Variables
	volatile std::sig_atomic_t signalStatus = 0;
	
	tjhandle decoder = nullptr;
	
	FrameMt frame0;
	FrameMt frame1;
}

// --- Signals ---
static void sigintHandler(int signal) {
	Globals::signalStatus = signal;
}

static bool decode(FrameMt& gbFrame, cv::Mat& cvFrame) {
	gbFrame.updated(false);
	
	// Re-Allocatation
	if(cvFrame.size().area() != gbFrame.area())
		cvFrame = cv::Mat::zeros(gbFrame.height(), gbFrame.width(), CV_8UC3);
	
	// Decode
	return (
		tjDecompress2 (
			Globals::decoder, 
			gbFrame.data(), gbFrame.length(), 
			cvFrame.data, 
			gbFrame.width(), 0, gbFrame.height(), 
			TJPF_BGR, TJFLAG_FASTDCT
		) >= 0);
}

// --- Entry point ---
int main(int argc, char* argv[]) {
	// - Inputs
	// None..
	
	// - Install signal handler
	std::signal(SIGINT, sigintHandler);
	
	// - Device
	ClientDevice device0(IAddress(Globals::IP_ADDRESS, 5000));
	ClientDevice device1(IAddress(Globals::IP_ADDRESS, 6000));
	
	// - Events
	device0.onFrame([&](const Gb::Frame& frame) {
		Globals::frame0.setFrame(frame);
	});
	device1.onFrame([&](const Gb::Frame& frame) {
		Globals::frame1.setFrame(frame);
	});
	
	// -------- Main loop --------  
	if(!device0.open() || !device1.open()) {
		std::cout << "Couldn't open device" << std::endl;
		std::cout << "Press a key to continue..." << std::endl;
		return std::cin.get();
	}
	
	// Alloc
	Globals::decoder = tjInitDecompress();
	
	cv::Mat frameDisp0 = cv::Mat::zeros(480, 640, CV_8UC3);
	cv::Mat frameDisp1 = cv::Mat::zeros(480, 640, CV_8UC3);
	
	// Loop
	while(Globals::signalStatus != SIGINT && cv::waitKey(10) != 27) {
		// Display 0
		if(Globals::frame0.updated()) {			
			Globals::frame0.lock();
			
			if(!Globals::frame0.empty()) 
				if(decode(Globals::frame0, frameDisp0))
					cv::imshow("Device 0", frameDisp0);
			
			Globals::frame0.unlock();
		}
		
		// Display 1
		if(Globals::frame1.updated()) {			
			Globals::frame1.lock();
			
			if(!Globals::frame1.empty()) 
				if(decode(Globals::frame1, frameDisp1))
					cv::imshow("Device 1", frameDisp1);
			
			Globals::frame1.unlock();
		}
	}
	
	// -- End
	device0.close();
	device1.close();
	tjDestroy(Globals::decoder);
	cv::destroyAllWindows();
	
	std::cout << "Clean exit" << std::endl;
	std::cout << "Press a key to continue..." << std::endl;
	return std::cin.get();
}
