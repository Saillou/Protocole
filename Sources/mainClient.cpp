#include <iostream>
#include <csignal>
#include <atomic>
#include <map>
#include <deque>
#include <functional>

#include "StreamDevice/ClientDevice.hpp"
#include "Tool/Timer.hpp"

#ifdef _WIN32
	// Based on Opencv
	#include <opencv2/core.hpp>
	#include <opencv2/highgui.hpp>
	#include <opencv2/imgproc.hpp>
#endif

#include <wels/codec_api.h>

namespace Globals {
	// Constantes
	// const std::string IP_ADDRESS = "192.168.11.52"; 	// Barnacle V4
	// const std::string IP_ADDRESS = "127.0.0.1"; 	// localhost V4
	
	const std::string IP_ADDRESS = "fe80::6d2a:7cb1:51df:3050"; 		// Barnacle V6
	// const std::string IP_ADDRESS = "::1"; 								// localhost V6
	
	// Variables
	volatile std::sig_atomic_t signalStatus = 0;
}

// --- Signals ---
static void sigintHandler(int signal) {
	Globals::signalStatus = signal;
}

// --- Helper ----
void showDevice(int port, cv::Mat& cvFrame, std::mutex& mutFrame) {
	// Device	
	if(SocketTool::determineType(Globals::IP_ADDRESS) == Ip_v6)
		port++;
		
	ClientDevice device(IAddress(Globals::IP_ADDRESS, port));
	
	// ----- Events -----	
	device.onFrame([&](const Gb::Frame& frame) {
		std::lock_guard<std::mutex> lock(mutFrame);
		
		// Size
		if(frame.size.width != cvFrame.cols || frame.size.height != cvFrame.rows)
			cvFrame = cv::Mat::zeros(frame.size.height, frame.size.width, CV_8UC3);
		
		// Data
		memcpy(cvFrame.data, frame.start(), frame.length());
	});
	
	std::atomic<bool> open = false;
	device.onOpen([&]() {
		open = true;
	});
	
	// -------- Main loop --------  
	
	if(!device.open(1000)) {
		std::cout << "Can't open device" << std::endl;
		return;
	}
	else {
		std::cout << "Device open" << std::endl;
	}

	while(Globals::signalStatus != SIGINT) {
		Timer::wait(100);
		
		if(open) {
			// do something
		}
	}

	// -- End
	device.close();
}

// --- Entry point ---
int main(int argc, char* argv[]) {	
	// - Install signal handler
	std::signal(SIGINT, sigintHandler);
	
	cv::Mat cvFrame0;
	cv::Mat cvFrame1;
	cv::Mat cvFrame2;
	std::mutex frameMut0;
	std::mutex frameMut1;
	std::mutex frameMut2;
	
	std::thread thread0(showDevice, 8000, std::ref(cvFrame0), std::ref(frameMut0));
	std::thread thread1(showDevice, 8002, std::ref(cvFrame1), std::ref(frameMut1));
	std::thread thread2(showDevice, 8004, std::ref(cvFrame2), std::ref(frameMut2));
	
	// --- Loop ----
	while(Globals::signalStatus != SIGINT && cv::waitKey(10) != 27) {
		// --- Frame 0 ----
		frameMut0.lock();
		if(!cvFrame0.empty())
			cv::imshow("Camera 0", cvFrame0);
		frameMut0.unlock();

		// --- Frame 1 ----
		frameMut1.lock();
		if(!cvFrame1.empty())
			cv::imshow("Camera 1", cvFrame1);	
		frameMut1.unlock();
		
		// // --- Frame 2 ----
		frameMut2.lock();
		if(!cvFrame2.empty())
			cv::imshow("Camera 2", cvFrame2);	
		frameMut2.unlock();
	}
	Globals::signalStatus = SIGINT;
	
	// Wait
	if(thread0.joinable())
		thread0.join();
	if(thread1.joinable())
		thread1.join();
	if(thread2.joinable())
		thread2.join();
	cv::destroyAllWindows();
	
	// --- End ----
	std::cout << "Clean exit" << std::endl;
	std::cout << "Press a key to continue..." << std::endl;
	return std::cin.get();
}
