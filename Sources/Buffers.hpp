#pragma once

#include <opencv2/core.hpp>	

#include <iostream>
#include <csignal>
#include <deque>

#include "Network/Message.hpp"

// Buffer
template <typename T>
class VirtualBuffer {
public:
	void lock() const {
		mut.lock();
	}
	void unlock() const {
		mut.unlock();
	}
	size_t size() const {
		return buffer.size();
	}	
	
	virtual bool update(T& t) = 0;
	
	void pop() {
		buffer.pop_front();
		times.pop_front();	
	}
	
	void push(const T& t, const uint64_t& time) {
		buffer.push_back(t);
		times.push_back(time);
	}
	
protected:
	mutable std::mutex mut;
	std::deque<T> buffer;
	std::deque<uint64_t> times;
	
	Timer timer;
	int64_t timeToWait = 0;
};

// --- For frame ---
class FrameBuffer : public VirtualBuffer<cv::Mat> {
public:	
	bool update(cv::Mat& frame) override {
		if(size() > 1) {			
			if(timer.elapsed_mus() >= timeToWait ) {
				timer.beg();
				
				// Change frame disp
				buffer.front().copyTo(frame);
				timeToWait = 1000*((int64_t)times[1] - (int64_t)times[0])/2;
				
				// Change buffer
				pop();
				
				return !frame.empty();
			}
		}	
		return false;
	}
};


// --- For datas ---
class DataBuffer : public VirtualBuffer<MessageFormat> {	
public:
	bool update(MessageFormat& message) {
		if(size() > 1) {			
			if(timer.elapsed_mus() >= timeToWait ) {
				timer.beg();
				
				// Change messag disp
				message = buffer.front();
				timeToWait = 1000*((int64_t)times[1] - (int64_t)times[0])/2;
				
				// Change buffer
				pop();
				
				return !message.str().empty();
			}
		}	
		return false;
	}
};


