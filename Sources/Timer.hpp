#pragma once

#include <chrono>
#include <time.h>
#include <sstream>
#include <thread>

class Timer {	
public:
	Timer():
		_tRef(_now()),
		_tBeg(_tRef),
		_tEnd(_tRef)
	{
	}

	// Methods	
	void beg() {
		_tBeg = _now();
	}
	void end() {
		_tEnd = _now();
	}
	void reset() {
		_tRef = _now();
		_tBeg = _tRef;
		_tEnd = _tRef;
	}
	
	int64_t clock_mus() const {
		return  _diffMus(_tRef, _now());
	}
	int64_t mus() const {
		return _diffMus(_tBeg, _tEnd);
	}
	int64_t elapsed_mus() const {
		return _diffMus(_tBeg, _now());
	}
	
	// Static methods
	static void wait(int ms) {
		if(ms > 0)
			std::this_thread::sleep_for(std::chrono::milliseconds(ms));
	}
	
	static std::string date() {
		time_t rawtime = time(NULL);
		struct tm timeInfo;
		
#ifdef _WIN32
		localtime_s(&timeInfo, &rawtime);
#else
		struct tm *ptrTimeInfo;
		ptrTimeInfo = localtime(&rawtime);
		timeInfo = *ptrTimeInfo;
#endif	
		
		std::stringstream ss;
		ss << _int2paddedStr(timeInfo.tm_year + 1900, 	4);
		ss << _int2paddedStr(timeInfo.tm_mon  + 1, 		2);
		ss << _int2paddedStr(timeInfo.tm_mday,			2);
		ss << "_";
		ss << _int2paddedStr(timeInfo.tm_hour, 2);
		ss << _int2paddedStr(timeInfo.tm_min,	 2);
		ss << _int2paddedStr(timeInfo.tm_sec,  2);
		
		return ss.str();
	}
	
	static std::string timeStr(int64_t mus) {
		int totalSec 	= (int)(mus / 1000000);
		
		int hours 		= totalSec / 3600;
		totalSec -= 3600*hours;
		
		int minutes 	= totalSec / 60;
		totalSec -= 	60*minutes;
		
		int secondes 	= totalSec;
		
		std::stringstream ss;
		
		if(hours > 0)
			ss << _int2paddedStr(hours, 2) << "h ";
		
		if(hours > 0 || minutes > 0)
			ss << _int2paddedStr(minutes, 2) << "mn ";
		
		ss << _int2paddedStr(secondes,  2) << "s";
		
		return ss.str();
	}
	
private:
	typedef std::chrono::high_resolution_clock::time_point _timePoint;
	
	// Static methods
	static _timePoint _now() {
		return std::chrono::high_resolution_clock::now();
	}
	static int64_t _diffMus(const _timePoint& tA, const _timePoint& tB) {
		return std::chrono::duration_cast<std::chrono::duration<int64_t, std::micro>>(tB - tA).count();
	}
	
	static std::string _int2paddedStr(const int _int, const size_t pad) {
		std::string intstr = std::to_string(_int);
		if(intstr.size() >= pad)
			return intstr;
		else
			return std::string(pad - intstr.size() , '0') + intstr;
	}
	
	// Members
	_timePoint _tRef; // Memorise time of the chrono's birth / last reset
	_timePoint _tBeg;
	_timePoint _tEnd;
};
