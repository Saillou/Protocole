#pragma once

#include <cstring>
#include <string>
#include <vector>
#include <iostream>

#include "../Timer.hpp"

class Message {
public:
	enum ActionCode {
		TEXT 			= (1<<1),
		HANDSHAKE	= (1<<2),
		DEVICE_0		= (1<<3),
		DEVICE_1		= (1<<4),
	};
	
public:
	// - Constructors
	// Serializator
	Message(const std::string& message) {
		_serialize(TEXT, message.size(), message.data());
	}
	Message(const ActionCode code, const std::string& message) {
		_serialize(code, message.size(), message.data());
	}
	Message(const ActionCode code, const char* buffer, const size_t len) {
		_serialize(code, len, buffer);
	}
	
	// Unserializator
	Message(const char* buffer, const size_t len) {
		_unserialize(buffer, len);
	}
	
	// - Getters
	const unsigned int code() const {
		return _code;
	}
	const unsigned int size() const {
		return _size;
	}
	const uint64_t timestamp() const {
		return _time;
	}
	const unsigned int length() const {
		return _size+14;
	}
	const char* content() const {
		if(isValide())
			return &_dataSerialized[14];
		else
			return nullptr;
	}
	const char* data() const {
		if(isValide())
			return &_dataSerialized[0];
		else
			return nullptr;
	}
	const std::string str() const {
		if(!isValide())
			return "";
		
		return std::string(&_dataSerialized[14], _size);
	}
	
	bool isValide() const {
		// Message should be at least 14 to be valid
		return (_dataSerialized.size() > 14);
	}
	
private:
	// - Methods 
	// Create a message [[CODE] [SIZE_MSG] [MSG]]
	void _serialize(const ActionCode code, const size_t size, const char* pMessage) {
		_time = Timer::timestampMs();
		_code = static_cast<unsigned int>(code);
		_size = static_cast<unsigned int>(size);
		
		// Transform these to 4 bytes
		unsigned char byteCode[4] = {
			static_cast<unsigned char>((_code & 0x000000FF) >> 0),
			static_cast<unsigned char>((_code & 0x0000FF00) >> 8), 
			static_cast<unsigned char>((_code & 0x00FF0000) >> 16), 
			static_cast<unsigned char>((_code & 0xFF000000) >> 24)
		};
		unsigned char byteSize[4] = {
			static_cast<unsigned char>((_size & 0x000000FF) >> 0),
			static_cast<unsigned char>((_size & 0x0000FF00) >> 8), 
			static_cast<unsigned char>((_size & 0x00FF0000) >> 16), 
			static_cast<unsigned char>((_size & 0xFF000000) >> 24)
		};
		unsigned char byteTime[6] = {
			static_cast<unsigned char>((_time & 0x0000000000FF) >> 0),
			static_cast<unsigned char>((_time & 0x00000000FF00) >> 8), 
			static_cast<unsigned char>((_time & 0x000000FF0000) >> 16), 
			static_cast<unsigned char>((_time & 0x0000FF000000) >> 24),
			static_cast<unsigned char>((_time & 0x00FF00000000) >> 32),
			static_cast<unsigned char>((_time & 0xFF0000000000) >> 40),
		};
		
		// Create string
		_dataSerialized.resize(static_cast<size_t>(14+_size), '\0');
		
		// Copy code 
		memcpy(&_dataSerialized[0], byteCode, 4);
		memcpy(&_dataSerialized[4], byteSize, 4);
		memcpy(&_dataSerialized[8], byteTime, 6);
		memcpy(&_dataSerialized[14], pMessage, static_cast<size_t>(_size));
	}
	
	void _unserialize(const char* buffer, const size_t len) {
		if(len < 14)
			return;
		
		_code = 
			(static_cast<unsigned int>(static_cast<unsigned char>(buffer[0])) << 0)  +
			(static_cast<unsigned int>(static_cast<unsigned char>(buffer[1])) << 8)  +
			(static_cast<unsigned int>(static_cast<unsigned char>(buffer[2])) << 16)	+
			(static_cast<unsigned int>(static_cast<unsigned char>(buffer[3])) << 24);
			
		_size = 
			(static_cast<unsigned int>(static_cast<unsigned char>(buffer[4])) << 0)  +
			(static_cast<unsigned int>(static_cast<unsigned char>(buffer[5])) << 8)  +
			(static_cast<unsigned int>(static_cast<unsigned char>(buffer[6])) << 16)	+
			(static_cast<unsigned int>(static_cast<unsigned char>(buffer[7])) << 24);
			
		_time = 
			(static_cast<uint64_t>(static_cast<unsigned char>(buffer[8])) 	<< 0)  +
			(static_cast<uint64_t>(static_cast<unsigned char>(buffer[9])) 	<< 8)  +
			(static_cast<uint64_t>(static_cast<unsigned char>(buffer[10])) << 16) +
			(static_cast<uint64_t>(static_cast<unsigned char>(buffer[11])) << 24) +
			(static_cast<uint64_t>(static_cast<unsigned char>(buffer[12])) << 32) +
			(static_cast<uint64_t>(static_cast<unsigned char>(buffer[13])) << 40);
		
		_dataSerialized = std::vector<char>(buffer, buffer+len);
	}
	
	// Members
	unsigned int _code;
	unsigned int _size;
	uint64_t _time;
	std::vector<char> _dataSerialized;
};

// --------- Error ------------
class Error {
public:
	enum ErrorCode {
		NO_CODE 				= 0x0,
		BAD_CONNECTION 	= 0x1,
		NOT_CONNECTED 		= 0x2
	};
	
public:
	Error(const unsigned int errorCode, const std::string& message) :
		_code(errorCode), _msg(message), _time(Timer::timestampMs())
	{
		// nothing else
	}
	
	const unsigned int code() const {
		return _code;
	}
	const uint64_t timestamp() const {
		return _time;
	}
	const std::string& msg() const {
		return _msg;
	}
	
private:
	unsigned int _code;
	uint64_t _time;
	std::string _msg;
};

// --------- Manager ------------
class MessageManager {
public:
	static std::vector<Message> readMessages(const char* buffer, const size_t len) {
		std::vector<Message> messages;
		
		unsigned int offset = 0;
		unsigned int limit = (unsigned int)len;
		
		while(offset + 14 <= limit) {
			// Read size
			unsigned int size = 
				(static_cast<unsigned int>(static_cast<unsigned char>(buffer[4])) << 0)  +
				(static_cast<unsigned int>(static_cast<unsigned char>(buffer[5])) << 8)  +
				(static_cast<unsigned int>(static_cast<unsigned char>(buffer[6])) << 16)	+
				(static_cast<unsigned int>(static_cast<unsigned char>(buffer[7])) << 24);
				
			// Create message
			if(offset + size + 14 <= limit)
				messages.push_back(Message(buffer + offset, size + 14));
			
			// Increase offset
			offset += (14 + size);
		}
		
		return messages;
	}
};


