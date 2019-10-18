#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>

#include <thread>
#include <mutex>
#include <atomic>

#include "Network/Server.hpp"
#include "Tool/Keyboard.hpp"
#include "Gui/core.hpp"
#include "Gui/TopGui.hpp"
#include "Device/Device.hpp"

// --- Globals ---
std::mutex mutServer;
std::atomic<bool> stop;

// -------------------------------- SOUND --------------------------------
snd_pcm_t* init_sound(unsigned int* pRate, int* pDir, int channelsNumber) {
	snd_pcm_t* capture_handle = nullptr;
	snd_pcm_hw_params_t* hw_params_cap = nullptr;
	
	// -- Init sound
	snd_pcm_open (&capture_handle, "default", SND_PCM_STREAM_CAPTURE, 0);
	
	snd_pcm_hw_params_malloc (&hw_params_cap);
	snd_pcm_hw_params_any (capture_handle, hw_params_cap);
	snd_pcm_hw_params_set_access (capture_handle, hw_params_cap, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format (capture_handle, hw_params_cap, SND_PCM_FORMAT_U8);
	snd_pcm_hw_params_set_rate_near (capture_handle, hw_params_cap, pRate, pDir);
	snd_pcm_hw_params_set_channels (capture_handle, hw_params_cap, channelsNumber);
	snd_pcm_hw_params (capture_handle, hw_params_cap);
	snd_pcm_hw_params_free (hw_params_cap);
	
	// -- Launch
	if(snd_pcm_prepare (capture_handle) < 0) {
		printf ("Sound init failed. \n");
		return nullptr;
	}	
	
	return capture_handle;
}

void loopSoundSend(Server* pServer, snd_pcm_t** pCapture, const int BUF_SIZE) {	
	char buf_8[BUF_SIZE] = {0};
	double relTime = 0.0;
	const double TIME_STEP = 1000.0*1024/44100; //ms
	
	while(!stop) {
		// Capture buffer 8bits
		if (snd_pcm_readi (*pCapture, buf_8, BUF_SIZE) != BUF_SIZE) {
			printf ("Sound read failed. \n");
			break;
		}
		
		relTime += TIME_STEP;
		uint64_t timestamp = (uint64_t)relTime; // Round
				
		// Send buffer
		mutServer.lock();
		for(auto& client : pServer->getClients()) {
			pServer->sendData(client, Message(Message::SOUND, &buf_8[0], BUF_SIZE, timestamp));
		}
		mutServer.unlock();
	}
}

// -------------------------------- VIDEO --------------------------------
void loopVideoSend(Server* pServer, Device* pDevice) {
	Gb::Frame frame(Gb::FrameType::Jpg422);
	double relTime = 0.0;
	const double TIME_STEP = 1000.0*1/30; //ms
	
	while(!stop) {
		if(!pDevice->read(frame))
			Timer::waitMus(5000);
			
		relTime += TIME_STEP;
		uint64_t timestamp = (uint64_t)relTime; // Round
			
		mutServer.lock();
		for(auto& client : pServer->getClients()) {
			pServer->sendData(client, Message(Message::VIDEO, (char*)frame.start(), frame.length(), timestamp));
		}	
		mutServer.unlock();
	}
}

// -------------------------------- MAIN --------------------------------
int main (int argc, char *argv[]) {
	printf("Init..\n");
	
	// -- Sound 
	const size_t BUF_SIZE 	= 1024;
	unsigned int rate 		= 44100;
	int dir = 0;	
	int channelsNumber = 1;
	 
	snd_pcm_t* capture_handle = init_sound(&rate, &dir, channelsNumber);
	if(!capture_handle) 
		return 1;
		
		
	// -- Video
	Device device("/dev/video0");
	if(!device.open())
		return 2;
	
	
	// -- Network
	Server server;
	
	server.onInfo([&](const Server::ClientInfo& client, const Message& message) {
		if(message.code() == (Message::FORMAT | Message::SOUND)) {
			MessageFormat cmd;
			cmd.add("numChannels", channelsNumber);
			cmd.add("sampleRate", rate);
			cmd.add("bitsPerSample", 8);
			cmd.add("bufferSize", BUF_SIZE);
			
			server.sendInfo(client, Message((Message::FORMAT | Message::SOUND), cmd.str()));
			printf("Wav info sent to client. \n");
		}
		
		if(message.code() == (Message::FORMAT | Message::VIDEO)) {
			MessageFormat cmd;
			cmd.add("width", 640);
			cmd.add("height", 480);
			cmd.add("fps", 30);
			cmd.add("pixel", (int)Gb::Jpg422);
			
			server.sendInfo(client, Message((Message::FORMAT | Message::VIDEO), cmd.str()));
			printf("Video info sent to client. \n");
		}
	});	
	
	if(!server.connectAt(8888)) {
		printf("Can't connect to server. \n");
		return 3;
	}
	
	// -- Loop
	printf("Start streaming. [Press escape to stop].\n");
	std::thread thSound(loopSoundSend, &server, &capture_handle, BUF_SIZE);
	std::thread thVideo(loopVideoSend, &server, &device);
	
	changeKeybordDirection(1);
	for(char key = 0; key != 27; key = keyHit()) {
		Timer::waitMus(5000);
	}
	changeKeybordDirection(0);
	
	
	
	// -- End
	stop = true;
	if(thSound.joinable())
		thSound.join();
		
	if(thVideo.joinable())
		thVideo.join();
	
	snd_pcm_close(capture_handle);
	device.close();	

	return 0;
}
