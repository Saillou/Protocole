#include "commons.h"

// Operators
// Basic type to std::vector<uint8_t>
puint8_t& operator<<(puint8_t& os, const uint8_t n) {
	os.push_back(n);
	return os;
}
puint8_t& operator<<(puint8_t& os, const uint16_t n) {
	uint8_t na = (uint8_t)(n >> 8);
	uint8_t nb = (uint8_t)(n % 0x100);
	os << na << nb;
	
	return os;
}
puint8_t& operator<<(puint8_t& os, const uint32_t n) {
	uint16_t na = (uint16_t)(n >> 16);
	uint16_t nb = (uint16_t)(n % 0x1'0000);
	os << na << nb;
	
	return os;
}

// Container type to std::vector<uint8_t>
puint8_t& operator<<(puint8_t& os, const puint32_t& cpOs) {
	for(auto t : cpOs)
		os << t;
	
	return os;
}
puint8_t& operator<<(puint8_t& os, const puint16_t& cpOs) {
	for(auto t : cpOs)
		os << t;
	
	return os;
}
puint8_t& operator<<(puint8_t& os, const puint8_t& cpOs) {
	for(auto t : cpOs)
		os << t;
	
	return os;
}
puint8_t& operator<<(puint8_t& os, const std::string& osu) {
	for(char c : osu)
		os << (uint8_t)c;
	
	return os;
}
puint8_t& operator<<(puint8_t& os, const char& c) {
	os << (uint8_t)c;
	return os;
}

// String
std::string operator+(const uint8_t c, const std::string& s) {
	std::string sc;
	sc.resize(1);
	sc[0] = (char)c;
	std::string sp = sc + s;
	return sp;
}