#include <iostream>
#include <csignal>
#include <atomic>
#include <map>
#include <deque>

#include "Device/DeviceMt.hpp"
#include "Network/Server.hpp"
#include "Network/Message.hpp"
#include "Timer.hpp"

#ifdef __linux__
	#define PATH_CAMERA_0 "/dev/video0"
	#define PATH_CAMERA_1 "/dev/video1"

#elif _WIN32
	#define PATH_CAMERA_0 "0"
	#define PATH_CAMERA_1 "1"
	
	// Based on Opencv
	#include <opencv2/core.hpp>	
	#include <opencv2/videoio.hpp>	
	#include <opencv2/highgui.hpp>
	#include <opencv2/imgproc.hpp>
	#include <opencv2/imgcodecs.hpp>

#endif

namespace Globals {
	// Constantes
	const int PORT = 8888;
	
	// Variables
	volatile std::sig_atomic_t signalStatus = 0;
}

// --- Structures ---
struct ClientRequest {
	bool play;
};

// --- Signals ---
static void sigintHandler(int signal) {
	Globals::signalStatus = signal;
}

// --- Entry point ---
int main() {
	// -- Install signal handler
	std::signal(SIGINT, sigintHandler);
	
	// Variables
	Server server;
	DeviceMt device0;
	DeviceMt device1;
	std::map<SOCKET, ClientRequest> mapRequests;
	
	// -- Connect server --
	server.connectAt(Globals::PORT);
	
	server.onClientConnect([&](const Server::ClientInfo& client) {
		std::cout << "New client, client_" << client.id() << std::endl;
		
		server.sendInfo(client, Message(Message::DEVICE_0, device0.isOpened() ? "Started." : "Not Started."));
		mapRequests[client.id()].play = false;
	});
	server.onClientDisconnect([&](const Server::ClientInfo& client) {
		std::cout << "Client quit, client_" << client.id() << std::endl;
		mapRequests[client.id()].play = false;
	});
	server.onError([&](const Error& error) {
		std::cout << "Error : " << error.msg() << " code: " << error.code() << std::endl;
	});
	
	server.onInfo([&](const Server::ClientInfo& client, const Message& message) {
		std::cout << "Info received from client_" << client.id() << ": [Code:" << message.code() << "] " << message.str() << std::endl;
		if(message.code() == Message::TEXT && message.str() == "Send") {
			mapRequests[client.id()].play = true;
		}
		else {
			if(message.code() & Message::DEVICE_0) {
				std::string msg = message.str();
				// --- get ---
				if(msg == "?") {
					if(message.code() == Message::DEVICE_0_FORMAT) {
						Device::FrameFormat fmt = device0.getFormat();
						
						MessageFormat command;
						command.add("width", fmt.width);
						command.add("height", fmt.height);
						command.add("pixel", fmt.format);
						server.sendInfo(client, Message(Message::DEVICE_0_FORMAT, command.str()));
					}
					if(message.code() == Message::DEVICE_0_PROPERTIES) {
						MessageFormat command;
						command.add("saturation", device0.get(Device::Saturation));
						server.sendInfo(client, Message(Message::DEVICE_0_FORMAT, command.str()));
					}
				}
				else {
					// --- set ---
					bool exist = false;
					MessageFormat command(msg);
					
					if(message.code() == Message::DEVICE_0_FORMAT) {
						int width 	= command.valueOf<int>("width");
						int height 	= command.valueOf<int>("height");
						Device::PixelFormat pixFmt = command.valueOf<Device::PixelFormat>("pixel", &exist);
						
						if(exist && width > 0 && height > 0) {
							device0.setFormat(width, height, pixFmt);
						}					
					}
					if(message.code() == Message::DEVICE_0_PROPERTIES) {
						double saturation	= command.valueOf<double>("saturation", &exist);
						if(exist)
							device0.set(Device::Saturation, saturation);
						
						double exposure = command.valueOf<double>("exposure", &exist);
						if(exist)
							device0.set(Device::Exposure, exposure);
						
						double autoExposure = command.valueOf<double>("auto_exposure", &exist);
						if(exist)
							device0.set(Device::AutoExposure, autoExposure);
						
					}
				}
			}
		}
	});
	server.onData([&](const Server::ClientInfo& client, const Message& message) {
		std::cout << "Data received from client_" << client.id() << ": [Code:" << message.code() << "] " << message.str() << std::endl;
	});
	
	
	Timer t;
	std::deque<double> freq(100, 0.0);
	
	// -- Open devices --	
	if(device0.open(PATH_CAMERA_0)) {
		// Params
		device0.setFormat(1280, 720, Device::MJPG);	
		// device0.setFormat(320, 240, Device::MJPG);	
		
		for(auto& client: server.getClients()) {
			if(client.connected) {
				server.sendInfo(client, Message(Message::DEVICE_0, "Started."));
			}
		}
		
		// Events		
		device0.onFrame([&](const Gb::Frame& frame) {
			// Show frame and fps
			t.end();
			freq.push_back(1000000.0/t.mus());
			freq.pop_front();
			t.beg();
			
#ifdef _WIN32				
			cv::Mat f = cv::imdecode(cv::Mat(1, frame.length(), CV_8UC1, (void*)(frame.start())), cv::IMREAD_COLOR);
			if(!f.empty()) {
				cv::line(f, cv::Point(0, 100),  cv::Point(100, 100), cv::Scalar(0,0,255), 1, 16);
				cv::line(f, cv::Point(0, 70),  cv::Point(100, 70), cv::Scalar(255,0,0), 1, 16);
				for(int i = 1; i < 100; i++) {
					int y0 = freq[i-1] > 100 ? 100 : (int)(freq[i-1]);
					int y1 = freq[i] > 100 ? 100 : (int)(freq[i]);
	
					cv::line(f, cv::Point(i, 100-y0),  cv::Point(i+1, 100-y1), cv::Scalar(0,255,0), 1, 16);
				}
				
				cv::imshow("frame0", f);
				cv::waitKey(1);
			}
#endif
		
			// Send
			for(auto& client: server.getClients()) {
				if(client.connected && mapRequests[client.id()].play) {
					server.sendData(client, Message(Message::DEVICE_0, reinterpret_cast<const char*>(frame.start()), frame.length()));
				}
			}
		});
	}
	
	if(device1.open(PATH_CAMERA_1)) {
		// device1.setFormat(320, 240, Device::MJPG);	
		device1.setFormat(1280, 720, Device::MJPG);	
		
		// Events
		device1.onFrame([&](const Gb::Frame& frame){
			for(auto& client: server.getClients()) {
				if(client.connected  && mapRequests[client.id()].play) {
					server.sendData(client, Message(Message::DEVICE_1, reinterpret_cast<const char*>(frame.start()), frame.length()));
				}
			}
		});
	}
	
	
	// -------- Main loop --------  
	for(Timer timer; Globals::signalStatus != SIGINT; timer.wait(100)) {
		// ... Do other stuff ...
	}
		
	// -- End
	device0.release();
	device1.release();
	server.disconnect();
	
	std::cout << "Clean exit" << std::endl;
	std::cout << "Press a key to continue..." << std::endl;
	return std::cin.get();
}
