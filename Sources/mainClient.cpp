#include <iostream>
#include <csignal>
#include <atomic>
#include <map>
#include <deque>
#include <functional>

#include "StreamDevice/ClientDevice.hpp"
#include "Tool/Timer.hpp"
#include "Tool/FrameMt.hpp"

#include <wels/codec_api.h>

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

// --- Helper ----
void showDevice(const int port, cv::Mat& cvFrame, std::mutex& mutFrame) {
	// Device	
	ClientDevice device(IAddress(Globals::IP_ADDRESS, port));
	
	
	// ----- Events -----	
	device.onFrame([&](const Gb::Frame& frame) {
		mutFrame.lock();
		if(frame.size.width != cvFrame.cols || frame.size.height != cvFrame.rows)
			cvFrame = cv::Mat::zeros(frame.size.height, frame.size.width, CV_8UC3);
		memcpy(cvFrame.data, frame.start(), frame.length());
		mutFrame.unlock();
	});
	
	// -------- Main loop --------  
	if(!device.open(-1)) {
		std::cout << "Can't open device" << std::endl;
		std::cout << "Press a key to continue..." << std::endl;
		return;
	}
	
	while(Globals::signalStatus != SIGINT) {
		Timer::wait(100);
	}

	// -- End
	device.close();
}

// --- Entry point ---
int main(int argc, char* argv[]) {	
	// - Install signal handler
	std::signal(SIGINT, sigintHandler);
	
	cv::Mat cvFrame0(cv::Mat::zeros(1, 1, CV_8UC3));
	cv::Mat cvFrame1(cv::Mat::zeros(1, 1, CV_8UC3));
	std::mutex frameMut0;
	std::mutex frameMut1;
	
	std::thread thread0(showDevice, 6666, std::ref(cvFrame0), std::ref(frameMut0));
	std::thread thread1(showDevice, 8888, std::ref(cvFrame1), std::ref(frameMut1));
	
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
	}
	Globals::signalStatus = SIGINT;
	
	// Wait
	if(thread0.joinable())
		thread0.join();
	if(thread1.joinable())
		thread1.join();
	cv::destroyAllWindows();
	
	// --- End ----
	std::cout << "Clean exit" << std::endl;
	std::cout << "Press a key to continue..." << std::endl;
	return std::cin.get();
}
