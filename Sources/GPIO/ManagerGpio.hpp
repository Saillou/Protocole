#ifndef MANAGER_GPIO_HPP
#define MANAGER_GPIO_HPP

#include <map>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <algorithm>
#include <functional>

#include <poll.h>
#include <fcntl.h>
#include <unistd.h>

#include "Gpio.hpp"


class ManagerGpio {
public:
	// Constructors
	ManagerGpio();
	~ManagerGpio();
	
	// Type
	typedef std::function<void(const int idGpio, const Gpio::Level level, const Gpio::Event event)> Callback;
	
	// Methods
	void startListening();
	void stopListening();
	
	void addEventListener(Gpio& gpio, const Gpio::Event event, Callback cbk);
	void removeEventListener(Gpio& gpio, const Gpio::Event event);
	
private:
	// Methods
	void _pollin(); // function for thread
	static const std::string _clearEvents(int fd);
	
	// Members
	std::vector<pollfd> _fdList;
	std::map<int, Callback> _pCallbacks;
	std::map<int, int> _fdGpio;
	
	std::thread* _pThread;
	std::atomic<bool> _isRunning{false};
	
};

#endif
