#include <iostream>
#include <csignal>

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
#endif

namespace Globals {
	// Constantes
	const std::string IP_ADDRESS = "127.0.0.1";
	// const std::string IP_ADDRESS = "192.168.11.24";
	const int PORT = 8888;
	
	// Variables
	volatile std::sig_atomic_t signalStatus = 0;
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
		client.sendInfo(Message("Send"));
	});
	
	client.onData([&](const Message& message) {
		// std::cout << "Data received: [Code:" << message.code() << "] " << message.size() << std::endl;
		
		if(message.code() == Message::DEVICE_0 || message.code() == Message::DEVICE_1) {
			cv::Mat f = cv::imdecode(cv::Mat(1, message.size(), CV_8UC1, (void*)message.content()), -1);
			if(!f.empty()) {
				if(message.code() == Message::DEVICE_0)
					cv::imshow("frame device 0", f);
				
				if(message.code() == Message::DEVICE_1)
					cv::imshow("frame device 1", f);
				
				cv::waitKey(1);
			}
		}
		
		// if(message.code() == Message::TEXT) {
			// std::cout << "Timestamp: " << message.timestamp() << std::endl;
			
			// if(message.size() < 50) {
				// std::cout << "Message size exepected: " << message.str() << std::endl;
			// }
			// else {
				// std::cout << "Receive: " << message.size()  << "bytes" << std::endl;
			// }
		// }
	});
	
	client.onInfo([&](const Message& message) {
		std::cout << "Info received: [Code:" << message.code() << "] " << message.str() << std::endl;
	});
	
	client.onError([&](const Error& error) {
		std::cout << "Error: " << error.msg() << std::endl;
	});
	
	
	// -------- Main loop --------
	for(Timer timer; Globals::signalStatus != SIGINT; timer.wait(100)) {
		/* ... Do stuff ... */
	}
		
	// -- End
	client.disconnect();
	
	std::cout << "Clean exit" << std::endl;
	std::cout << "Press a key to continue..." << std::endl;
	return std::cin.get();
}
