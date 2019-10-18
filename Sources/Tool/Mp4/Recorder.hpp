#pragma once

#include <string>
#include <vector>

#include <lame.h>

#include "../Decoder.hpp"
#include "../Encoder.hpp"

#include "mp4Creator.hpp"

class Recorder {
public:
	// Shortened names
	typedef mp4Creator::SampleType	SampleType;
	typedef mp4Creator::Chunk		Chunk;

	enum DataType {
		Raw = 0,
		Wav  = (1 << 2) | mp4Creator::Audio,
		Mp3  = (1 << 3) | mp4Creator::Audio,
		Bgr  = (1 << 4) | mp4Creator::Video,
		Jpg  = (1 << 5) | mp4Creator::Video,
		H264 = (1 << 6) | mp4Creator::Video
	};

	// Constructors
	explicit Recorder(const std::string& path = "untitled.mp4") :
		_isOpened(false),
		_path(path),
		_numChannels(1),
		_sampleRate(44100),
		_bufferSize(1024),
		_width(640),
		_height(480),
		_fps(30),
		_lame(nullptr)
	{
		_configAudio();
		_encoder.setup(_width, _height);
	}

	// Methods
	bool open(const std::string& path = "") {
		if (!path.empty())
			_path = path;

		_isOpened = _mp4Creator.open(_path, _numChannels, _sampleRate, _bufferSize, _fps);

		return isOpened();
	}
	bool close() {
		if (isOpened())
			return _mp4Creator.close();

		if(_lame)
			lame_close(_lame);
		_lame = nullptr;

		_encoder.cleanup();
		_decoder.cleanup();

		return false;
	}
	bool addChunk(const Chunk& chunk) {
		if(isOpened())
			return _mp4Creator.addChunk(chunk);

		return false;
	}
	Sample encode(DataType type, const char* pBuffer, const uint32_t size) {
		puint8_t in((unsigned char*)pBuffer, (unsigned char*)pBuffer + size);

		// Nothing special to do
		if (type == Raw || type == Mp3 || type == H264) {
			return Sample(in, type == Mp3 ? Sample::MP3_SAMPLE : Sample::NALU_SAMPLE);
		}

		// Audio encode (wav -> mp3)
		if (type == Wav) {
			//return Sample::fromRawMp3(_encodeAudio(in));
			return Sample(_encodeAudio(in), Sample::MP3_SAMPLE);
		}

		// Image decode (jpg -> rgb)
		puint8_t out;
		if (type == Jpg) {
			if (_decoder.decode2bgr24(in, out, _width, _height)) {

				in.swap(out);
				type = Bgr; // send result to video encoding
			}
			// else failed..
		}

		// Video encode (bgr {-> yuv420} -> h264)
		if (type == Bgr) {
			if (_encoder.encodeBgr(&in[0], out)) {
				return Sample::fromRawNalu(out);
			}
			// else failed..
		}

		// Failed
		return Sample();
	}

	// Setters
	bool setAudioConfig(int numChannels, int sampleRate, int bufferSize) {
		_numChannels = numChannels;
		_sampleRate	 = sampleRate;
		_bufferSize = bufferSize;

		// Re-config audio decoder
		return _configAudio();
	}
	bool setVideoConfig(int width, int height, int fps) {
		if (_width != width || _height != height) {
			// Re-config video encoder
			_encoder.cleanup();
			_encoder.setup(width, height);
		}

		_width	= width;
		_height = height;
		_fps	= fps;

		return _width > 0 && _height > 0 && _fps > 0;
	}

	// Getters
	bool isOpened() const {
		return _isOpened;
	}

private:
	// Methods
	bool _configAudio() {
		// Re-Config lame
		if (!_lame) // Previously closed
			_lame = lame_init();

		lame_set_in_samplerate(_lame, _sampleRate);
		lame_set_num_channels(_lame, _numChannels);
		lame_set_mode(_lame, _numChannels == 1 ? MPEG_mode::MONO : MPEG_mode::STEREO);

		return lame_init_params(_lame) > 0;
	}

	puint8_t _encodeAudio(const puint8_t& in) {
		const char* pcm		= (char*)&in[0];
		const uint32_t npcm = (uint32_t)in.size();

		// Can encode ?
		if (!_lame || npcm <= 0)
			return _mp3Buffer;

		// Internal buffer size
		size_t worstSize = (size_t)(7200 + 1.25*npcm);
		if (_mp3Buffer.size() < worstSize)
			_mp3Buffer.resize(worstSize);

		if (_pcmS.size() != npcm)
			_pcmS.resize(npcm);

		// To 16 signed bits (easier for lame)
		for (size_t i = 0; i < npcm; i++)
			_pcmS[i] = (short)(pcm[i] - 0x80) << 8;

		// Encode
		int nMp3 = lame_encode_buffer(_lame, &_pcmS[0], nullptr, (int)npcm, &_mp3Buffer[0], (int)worstSize);

		// Truncate
		return puint8_t(_mp3Buffer.begin(), _mp3Buffer.begin() + nMp3);
	}

	// Members
	mp4Creator _mp4Creator;
	std::string _path;
	bool _isOpened;

	int _numChannels;
	int _sampleRate;
	int _bufferSize;

	int _width;
	int _height;
	int _fps;

	lame_t _lame;
	std::vector<short> _pcmS;
	std::vector<unsigned char> _mp3Buffer;

	DecoderJpg _decoder;
	EncoderH264 _encoder;
};