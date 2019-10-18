#include <iostream>

#include "Network/Client.hpp"
#include "Tool/Timer.hpp"
#include "Tool/Buffer.hpp"
#include "Tool/Mp4/Recorder.hpp"
#include "Tool/Keyboard.hpp"

// ------------------------------------- Entry point -------------------------------------
int main() {
	// Constantes
	const std::string IP_SERVER = "192.168.11.6"; // Barnacle

	// Network
	Client client;
	bool readyAudio = false;
	bool readyVideo = false;

	// Buffers
	MsgBuffer bufferAudio;
	MsgBuffer bufferVideo;

	// Recorder
	Recorder recorder("file.mp4");

	// Events
	client.onConnect([&]() {
		// Ask for info
		std::cout << "Client ask format\n";
		client.sendInfo(Message((Message::SOUND | Message::FORMAT), "?"));
		client.sendInfo(Message((Message::VIDEO | Message::FORMAT), "?"));
	});

	client.onInfo([&](const Message& message) {
		// Ignore config if already launched
		if (recorder.isOpened())
			return;

		// Audio config
		if (message.code() == (Message::SOUND | Message::FORMAT)) {
			MessageFormat cmd(message.content());
			int numChannels		= cmd.valueOf<int>("numChannels");
			int sampleRate		= cmd.valueOf<int>("sampleRate");
			int bitsPerSample	= cmd.valueOf<int>("bitsPerSample");
			int bufferSize		= cmd.valueOf<int>("bufferSize");

			// Non null ?
			if (numChannels > 0 && sampleRate > 0 && bitsPerSample > 0 && bufferSize > 0) {
				// Setup
				readyAudio = true;
				recorder.setAudioConfig(numChannels, sampleRate, bufferSize);

				std::cout << "Answer: " << numChannels << "|" << sampleRate << "|" << bitsPerSample << "|" << bufferSize << std::endl;
			}
			else {
				std::cout << "Format exchange failed\n";
				std::cout << "Client ask format sound [again]\n";

				client.sendInfo(Message((Message::SOUND | Message::FORMAT), "?"));
			}
		}

		// Video config
		if (message.code() == (Message::VIDEO | Message::FORMAT)) {
			MessageFormat cmd(message.content());
			int width	= cmd.valueOf<int>("width");
			int height	= cmd.valueOf<int>("height");
			int pixel	= cmd.valueOf<int>("pixel");
			int fps		= cmd.valueOf<int>("fps");

			// Non null ?
			if (width > 0 && height > 0 && fps > 0) {
				// Setup
				readyVideo = true;
				recorder.setVideoConfig(width, height, fps);

				std::cout << "Answer: " << width << "x" << height << " [" << fps << "] " << pixel << std::endl;
			}
			else {
				std::cout << "Format exchange failed\n";
				std::cout << "Client ask format video [again]\n";

				client.sendInfo(Message((Message::VIDEO | Message::FORMAT), "?"));
			}
		}
		
		// Everything ok
		if (!recorder.isOpened() && readyVideo && readyAudio) {
			recorder.open();
			std::cout << "Open mp4 file \n";
		}
	});

	client.onData([&](const Message& message) {
		// Bufferize sample before recording chunks.
		if (message.code() == Message::SOUND) {
			// Store message
			bufferAudio.lock();
			bufferAudio.push(message, message.timestamp());
			bufferAudio.unlock();
		}

		if (message.code() == Message::VIDEO) {
			// Store message
			bufferVideo.lock();
			bufferVideo.push(message, message.timestamp());
			bufferVideo.unlock();
		}
	});

	// Finally, connect.
	if (!client.connectTo(IP_SERVER, 8888)) {
		std::cout << "Couldn't connect \n";
	}

	// ----------------------------------------------------------------------------------------------------------------

	// Continue until escape key is pressed
	changeKeybordDirection(1);
	for(bool continueLoop = true; continueLoop; Timer::wait(1000)) {
		// Inputs
		switch (keyHit()) {
		case 27:// Escape key
			continueLoop = false;
			break;
		}

		// Recorder has to be opened after this point
		if (!recorder.isOpened())
			continue;

		// Create chunks and clear buffers
		Message msg;
		Recorder::Chunk 
			chunkA(Recorder::SampleType::Audio), 
			chunkV(Recorder::SampleType::Video);

		// Audio
		bufferAudio.lock();
		while (bufferAudio.update(msg)) {
			chunkA.addData(recorder.encode(Recorder::Wav, msg.content(), msg.size()), msg.timestamp());
		}
		bufferAudio.unlock();

		// Video
		bufferVideo.lock();
		while (bufferVideo.update(msg)) {
			chunkV.addData(recorder.encode(Recorder::Jpg, msg.content(), msg.size()), msg.timestamp());
		}
		bufferVideo.unlock();

		// Finally, record chunks
		recorder.addChunk(chunkA);
		recorder.addChunk(chunkV);
	}
	changeKeybordDirection(0);
	
	// ----------------------------------------------------------------------------------------------------------------
	recorder.close();
	client.disconnect();
	bufferVideo.clear();
	bufferAudio.clear();

	std::cout << "Exit.\n";
	return system("pause");
}
