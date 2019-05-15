#include <iostream>
#include <csignal>
#include <deque>

#include "Timer.hpp"
#include "Network/Client.hpp"
#include "Network/Message.hpp"

// Linux
#ifdef __linux__
	// Die
// Windows	
#elif _WIN32
	// Based on Opencv
	#include <opencv2/core.hpp>	
	#include <opencv2/videoio.hpp>	
	#include <opencv2/highgui.hpp>
	#include <opencv2/imgproc.hpp>
	#include <opencv2/imgcodecs.hpp>
#endif

namespace Globals {
	// Constantes
	const std::string IP_ADDRESS = "127.0.0.1";
	// const std::string IP_ADDRESS = "192.168.11.24";
	const int PORT = 8888;
	
	// Variables
	volatile std::sig_atomic_t signalStatus = 0;
	
	// Frames
	std::mutex mutFrame0;
	std::deque<cv::Mat> frames0;
	std::deque<uint64_t> timesFrames0;
	
	std::mutex mutFrame1;
	cv::Mat frame1;
}

// --- Signals ---
static void sigintHandler(int signal) {
	Globals::signalStatus = signal;
}

// --- Entry point ---
int main() {
	// -- Install signal handler
	std::signal(SIGINT, sigintHandler);
	
	//-- Connect client
	Client client;
	client.connectTo(Globals::IP_ADDRESS, Globals::PORT);
	
	// -------- Callbacks --------
	client.onConnect([&]() {
		std::cout << "Connection to server success" << std::endl;
		client.sendInfo(Message(Message::DEVICE_0_FORMAT, "?"));
		client.sendInfo(Message(Message::DEVICE_0_PROPERTIES, "?"));
		client.sendInfo(Message("Send"));
	});
	
	client.onData([&](const Message& message) {
		// std::cout << "Data received: [Code:" << message.code() << "] " << message.size() << std::endl;
		
		if(message.code() == Message::DEVICE_0 || message.code() == Message::DEVICE_1) {
			cv::Mat f = cv::imdecode(cv::Mat(1, message.size(), CV_8UC1, (void*)message.content()), cv::IMREAD_COLOR);
			
			if(!f.empty()) {
				if(message.code() == Message::DEVICE_0) {
					Globals::mutFrame0.lock();
					
					Globals::frames0.push_back(f.clone());
					Globals::timesFrames0.push_back(message.timestamp());
					
					Globals::mutFrame0.unlock();
					
					// cv::imshow("frame device 0", f);
				}
				
				// if(message.code() == Message::DEVICE_0) {
					// Globals::mutFrame1.lock();
					// f.copyTo(Globals::frame1);
					// Globals::mutFrame1.unlock();

					// cv::imshow("frame device 1", f);
				// }
				
				// cv::waitKey(1);
			}
		}
	});
	
	client.onInfo([&](const Message& message) {
		std::cout << "Info received: [Code:" << message.code() << "] " << message.str() << std::endl;
	});
	
	client.onError([&](const Error& error) {
		std::cout << "Error: " << error.msg() << std::endl;
	});
	
	
	// -------- Main loop --------
	Timer t;
	cv::Mat frameDisp;
	int64_t timeToWait = 0;
	
	for(; Globals::signalStatus != SIGINT && cv::waitKey(1) != 27; ) {
		/* ... Do stuff ... */
		Globals::mutFrame0.lock();
		
		if(Globals::frames0.size() > 1) {			
			if(t.elapsed_mus() >= timeToWait ) {
				t.beg();
				
				// Change frame disp
				Globals::frames0.front().copyTo(frameDisp);
				
				timeToWait = 1000*((int64_t)Globals::timesFrames0[1] - (int64_t)Globals::timesFrames0[0])/2;
				
				// Change buffer
				Globals::frames0.pop_front();
				Globals::timesFrames0.pop_front();
			}
		}		
		
		Globals::mutFrame0.unlock();
		
		// Display
		if(!frameDisp.empty()) {
			cv::imshow("frame device 0", frameDisp);
		}
		
	}
		
	// -- End
	client.disconnect();
	
	std::cout << "Clean exit" << std::endl;
	std::cout << "Press a key to continue..." << std::endl;
	return std::cin.get();
}
