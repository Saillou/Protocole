#include <iostream>
#include <csignal>
#include <deque>

#include "Timer.hpp"
#include "Buffers.hpp"
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


// -- Globals space --
namespace Globals {
	// Constantes
	const std::string IP_ADDRESS = "192.168.11.24"; 	// Barnacle V4
	// const std::string IP_ADDRESS = "127.0.0.1"; 			// localhost V4
	const int PORT = 8888;											// Port v4
	
	// const std::string IP_ADDRESS = "fe80::b18:f81d:13a8:3a4"; 	// Barnacle V6
	// const std::string IP_ADDRESS = "::1"; 								// localhost V6
	// const int PORT = 8889;														// Port v6
	
	// Variables
	volatile std::sig_atomic_t signalStatus = 0;
	
	// Frames
	FrameBuffer buffer0;
	FrameBuffer buffer1;
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
	});
	
	client.onData([&](const Message& message) {		
		if(message.code() == Message::DEVICE_0 || message.code() == Message::DEVICE_1) {
			std::cout << clock() << std::endl;
			// cv::Mat f = cv::imdecode(cv::Mat(1, message.size(), CV_8UC1, (void*)message.content()), cv::IMREAD_COLOR);
			
			// if(!f.empty()) {
				// if(message.code() == Message::DEVICE_0) {
					// Globals::buffer0.lock();
					// Globals::buffer0.push(f, message.timestamp());
					// Globals::buffer0.unlock();
				// }
				// if(message.code() == Message::DEVICE_1) {
					// Globals::buffer1.lock();
					// Globals::buffer1.push(f, message.timestamp());
					// Globals::buffer1.unlock();
				// }
			// }
		}
	});
	
	client.onInfo([&](const Message& message) {
		std::cout << "Info received: [Code:" << message.code() << "] " << message.str() << std::endl;
		if(message.code() == Message::DEVICE_0 && message.str() == "Started.") {
			client.sendInfo(Message(Message::DEVICE_0_FORMAT, "?"));
			client.sendInfo(Message("Send"));
		}
	});
	
	client.onError([&](const Error& error) {
		std::cout << "Error: " << error.msg() << std::endl;
	});
	
	
	// -------- Main loop --------
	cv::Mat frameDisp_0, frameDisp_1;
	for(; Globals::signalStatus != SIGINT && cv::waitKey(1) != 27; ) {
		// Update buffers
		Globals::buffer0.lock();
		bool updated_0 = Globals::buffer0.update(frameDisp_0);
		Globals::buffer0.unlock();
		
		Globals::buffer1.lock();
		bool updated_1 = Globals::buffer1.update(frameDisp_1);
		Globals::buffer1.unlock();
		
		// Display frames
		if(updated_0)
			cv::imshow("frame device 0", frameDisp_0);
		
		if(updated_1)
			cv::imshow("frame device 1", frameDisp_1);
	}
		
	// -- End
	cv::destroyAllWindows();
	client.disconnect();
	
	std::cout << "Clean exit" << std::endl;
	std::cout << "Press a key to continue..." << std::endl;
	return std::cin.get();
}
