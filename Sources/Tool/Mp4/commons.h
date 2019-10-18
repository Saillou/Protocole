#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <cstdint>
#include <ctime>
#include <time.h>
#include <memory.h>
#include <stdio.h>

// Types
typedef std::vector<uint32_t> puint32_t;
typedef std::vector<uint16_t> puint16_t;
typedef std::vector<uint8_t> puint8_t;

// Operators
// Basic type to std::vector<uint8_t>
puint8_t& operator<<(puint8_t& os, const uint8_t n);
puint8_t& operator<<(puint8_t& os, const uint16_t n);
puint8_t& operator<<(puint8_t& os, const uint32_t n);

// Container type to std::vector<uint8_t>
puint8_t& operator<<(puint8_t& os, const puint32_t& cpOs);
puint8_t& operator<<(puint8_t& os, const puint16_t& cpOs);
puint8_t& operator<<(puint8_t& os, const puint8_t& cpOs);
puint8_t& operator<<(puint8_t& os, const std::string& osu);
puint8_t& operator<<(puint8_t& os, const char& c);

// String
std::string operator+(const uint8_t c, const std::string& s);

// More globally
template <typename T>
std::vector<T>& operator<<(std::vector<T>& os, const T& n) {
	os.push_back(n);
	return os;
}

template <typename T>
std::vector<T>& operator<<(std::vector<T>& os, const std::vector<T>& osCp) {
	for(const T& s : osCp)
		os.push_back(s);
	return os;
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& array) {
	for(const T& a : array)
		os << a << " ";
	return os;
}