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
	
	Gb::Frame dataFrame0;
	std::mutex mutFrame0;
	std::atomic<bool> updated0 = false;
	
	Gb::Frame dataFrame1;
	std::mutex mutFrame1;
	std::atomic<bool> updated1 = false;
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
	ClientDevice device0(IAddress(Globals::IP_ADDRESS, 5000));
	ClientDevice device1(IAddress(Globals::IP_ADDRESS, 6000));
	
	// - Events
	device0.onFrame([&](const Gb::Frame& frame) {
		Globals::mutFrame0.lock();
		Globals::dataFrame0 = frame;
		Globals::mutFrame0.unlock();
		Globals::updated0 = true;
	});
	device1.onFrame([&](const Gb::Frame& frame) {
		Globals::mutFrame1.lock();
		Globals::dataFrame1 = frame;
		Globals::mutFrame1.unlock();
		Globals::updated1 = true;
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
		if(Globals::updated0) {
			Globals::updated0 = false;
			
			Globals::mutFrame0.lock();
			if(!Globals::dataFrame0.empty()) {
				// Re-Allocatation
				if(frameDisp0.cols * frameDisp0.rows != Globals::dataFrame0.size.height * Globals::dataFrame0.size.width)
					frameDisp0 = cv::Mat::zeros(Globals::dataFrame0.size.height, Globals::dataFrame0.size.width, CV_8UC3);
				
				// Decode
				if(tjDecompress2(Globals::decoder, Globals::dataFrame0.start(), Globals::dataFrame0.length(), frameDisp0.data, Globals::dataFrame0.size.width, 0, Globals::dataFrame0.size.height, TJPF_BGR, TJFLAG_FASTDCT) >= 0) {
					if(!frameDisp0.empty()) {
						cv::imshow("Device 0", frameDisp0);
					}
				}
			}
			Globals::mutFrame0.unlock();
		}
		
		// Display 1
		if(Globals::updated0) {
			Globals::updated0 = false;
			
			Globals::mutFrame1.lock();
			if(!Globals::dataFrame1.empty()) {
				// Re-Allocatation
				if(frameDisp1.cols * frameDisp1.rows != Globals::dataFrame1.size.height * Globals::dataFrame1.size.width)
					frameDisp1 = cv::Mat::zeros(Globals::dataFrame1.size.height, Globals::dataFrame1.size.width, CV_8UC3);
				
				// Decode
				if(tjDecompress2(Globals::decoder, Globals::dataFrame1.start(), Globals::dataFrame1.length(), frameDisp1.data, Globals::dataFrame1.size.width, 0, Globals::dataFrame1.size.height, TJPF_BGR, TJFLAG_FASTDCT) >= 0) {
					if(!frameDisp1.empty()) {
						cv::imshow("Device 1", frameDisp1);
					}
				}
			}
			Globals::mutFrame1.unlock();
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
