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
	
	Gb::Frame dataFrame;
	std::mutex mutFrame;
	std::atomic<bool> updated = false;
}

// --- Signals ---
static void sigintHandler(int signal) {
	Globals::signalStatus = signal;
}

// --- Entry point ---
int main(int argc, char* argv[]) {
	// - Inputs
	// None..
	
	// - Install signal handler
	std::signal(SIGINT, sigintHandler);
	
	// - Device
	ClientDevice device(IAddress(Globals::IP_ADDRESS, Globals::PORT));
	
	// - Events
	device.onOpen([&]() {
		std::cout << "Device opened" << std::endl;
	});
	device.onFrame([&](const Gb::Frame& frame) {
		Globals::mutFrame.lock();
		Globals::dataFrame = frame;
		Globals::mutFrame.unlock();
		Globals::updated = true;
	});
	device.onError([&](const Error& error) {
		std::cout << "Error occured" << std::endl;
	});
	
	
	// -------- Main loop --------  
	if(!device.open()) {
		std::cout << "Couldn't open device" << std::endl;
		std::cout << "Press a key to continue..." << std::endl;
		return std::cin.get();
	}
	
	// Alloc
	Globals::decoder = tjInitDecompress();
	cv::Mat frameDisp = cv::Mat::zeros(480, 640, CV_8UC3);
	
	// Loop
	while(Globals::signalStatus != SIGINT && cv::waitKey(10) != 27) {
		// Display
		if(Globals::updated) {
			Globals::updated = false;
			
			Globals::mutFrame.lock();
			if(!Globals::dataFrame.empty()) {
				// Re-Allocatation
				if(frameDisp.cols * frameDisp.rows != Globals::dataFrame.size.height * Globals::dataFrame.size.width)
					frameDisp = cv::Mat::zeros(Globals::dataFrame.size.height, Globals::dataFrame.size.width, CV_8UC3);
				
				// Decode
				if(tjDecompress2(Globals::decoder, Globals::dataFrame.start(), Globals::dataFrame.length(), frameDisp.data, Globals::dataFrame.size.width, 0, Globals::dataFrame.size.height, TJPF_BGR, TJFLAG_FASTDCT) >= 0) {
					if(!frameDisp.empty()) {
						cv::imshow("Device", frameDisp);
					}
				}
			}
			Globals::mutFrame.unlock();
		}
	}
	
	// -- End
	device.close();
	tjDestroy(Globals::decoder);
	cv::destroyAllWindows();
	
	std::cout << "Clean exit" << std::endl;
	std::cout << "Press a key to continue..." << std::endl;
	return std::cin.get();
}
