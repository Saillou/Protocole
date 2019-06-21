#include "ManagerGpio.hpp"

// Constructors
ManagerGpio::ManagerGpio() : _pThread(nullptr) {

}

ManagerGpio::~ManagerGpio() {
	stopListening();
	
	_fdList.clear();
	_pCallbacks.clear();
	_fdGpio.clear();
}

// Methods
void ManagerGpio::addEventListener(Gpio& gpio, const Gpio::Event event, Callback cbk) {
	if(!_isRunning) { // Only when not working we are safe
		if(!gpio.setEvent(event))
			return;
	
		pollfd pfd;
		pfd.fd = gpio.fd();
		pfd.events = POLLPRI;
		pfd.revents = 0;
		
		_clearEvents(pfd.fd);
		
		bool already = false;
		for(auto it = _fdList.begin(); it != _fdList.end(); it++) {
			if(it->fd == pfd.fd) {
				already = true;
				break;
			}
		}
		if(!already)
			_fdList.push_back(pfd); 
			
		_pCallbacks[pfd.fd] = cbk;
		_fdGpio[pfd.fd] = gpio.id();
	}
}
void ManagerGpio::removeEventListener(Gpio& gpio, const Gpio::Event event) {
	if(!_isRunning) { // Only when not working we are safe
		int fdRem = gpio.fd();
		
		_fdList.erase(
			std::remove_if(_fdList.begin(), _fdList.end(), [&](const pollfd& pfd)->bool {
				return pfd.fd == fdRem;
			}), _fdList.end()
		);
		_pCallbacks.erase(fdRem);
		_fdGpio.erase(fdRem);
	}
}

void ManagerGpio::startListening() {
	if(!_isRunning) {
		_isRunning = true;
		_pThread = new std::thread(&ManagerGpio::_pollin, this);
	}
}
void ManagerGpio::stopListening() {
	if(_isRunning) {
		_isRunning = false;
		
		if(_pThread != nullptr) {
			if(_pThread->joinable())
				_pThread->join();
			delete _pThread;
		}
		_pThread = nullptr;
	}
}


// Private 
void ManagerGpio::_pollin() { // Running inside thread
	while(_isRunning) {
		int ret = poll(_fdList.data(), _fdList.size(), 50);
		if(ret < 0) // error
			break;
		if(ret == 0) // timeout
			continue;
			
		// Search in the list which ones has a revents non null
		for(auto it = _fdList.begin(); it != _fdList.end(); it++) {
			if(it->revents == 0)
				continue;
				
			std::string result = _clearEvents(it->fd);
			Gpio::Level level = Gpio::StringToLevel(result);
			
			// Check callbacks
			auto callback = _pCallbacks.find(it->fd);
			if(callback != _pCallbacks.end())
				callback->second(_fdGpio[it->fd], level, level == Gpio::High ? Gpio::Rising : Gpio::Falling);	
		}
	}
}

// Static
const std::string ManagerGpio::_clearEvents(int fd) {
	if(fd < 0)
		return "";
		
	char buf[8];
	lseek(fd, 0, SEEK_SET);
	int nRead = read(fd, buf, sizeof(buf));
	
	return nRead > 0 ? std::string(buf, nRead-1) : "";
}








