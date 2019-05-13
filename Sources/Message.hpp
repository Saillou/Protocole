#pragma once

#include <cstring>
#include <string>
#include <vector>
#include <iostream>

class Message {
public:
	enum ActionCode {
		TEXT,
		HANDSHAKE
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
	const unsigned int length() const {
		return _size+8;
	}
	const char* content() const {
		if(isValide())
			return &_dataSerialized[8];
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
		
		return std::string(&_dataSerialized[8], _size);
	}
	
	bool isValide() const {
		// Message should be at least 8 to be valid
		return (_dataSerialized.size() > 8);
	}
	
private:
	// - Methods 
	// Create a message [[CODE] [SIZE_MSG] [MSG]]
	void _serialize(const ActionCode code, const size_t size, const char* pMessage) {
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
		
		// Create string
		_dataSerialized.resize(static_cast<size_t>(8+_size), '\0');
		
		// Copy code 
		memcpy(&_dataSerialized[0], byteCode, 4);
		memcpy(&_dataSerialized[4], byteSize, 4);
		memcpy(&_dataSerialized[8], pMessage, static_cast<size_t>(_size));
	}
	
	void _unserialize(const char* buffer, const size_t len) {
		if(len < 8)
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
		
		_dataSerialized = std::vector<char>(buffer, buffer+len);
	}
	
	// Members
	unsigned int _code;
	unsigned int _size;
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
	Error(const int errorCode, const std::string& message) :
		_code(errorCode), _msg(message)
	{
		// nothing else
	}
	
	const int code() const {
		return _code;
	}
	const std::string& msg() const {
		return _msg;
	}
	
private:
	int _code;
	std::string _msg;
};

// --------- Manager ------------
class MessageManager {
public:
	static std::vector<Message> readMessages(const char* buffer, const size_t len) {
		std::vector<Message> messages;
		
		unsigned int offset = 0;
		unsigned int limit = (unsigned int)len;
		
		while(offset + 8 <= limit) {
			// Read size
			unsigned int size = 
				(static_cast<unsigned int>(static_cast<unsigned char>(buffer[4])) << 0)  +
				(static_cast<unsigned int>(static_cast<unsigned char>(buffer[5])) << 8)  +
				(static_cast<unsigned int>(static_cast<unsigned char>(buffer[6])) << 16)	+
				(static_cast<unsigned int>(static_cast<unsigned char>(buffer[7])) << 24);
				
			// Create message
			if(offset + size + 8 <= limit)
				messages.push_back(Message(buffer + offset, size + 8));
			
			// Increase offset
			offset += (8+size);
		}
		
		return messages;
	}
};


