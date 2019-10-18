#include "Gpio.hpp"
#ifdef __linux__	

	// Constructors
Gpio::Gpio(int nPin, Type type, bool enablePullUp) :
	_nPin(nPin),
	_type(type),
	_path("/sys/class/gpio/gpio"+std::to_string(nPin)),
	_level(Low),
	_isValide(false),
	_fd(-1),
	_event(None)
{
	// Export GPIO
	_isValide = _echo("/sys/class/gpio/export", std::to_string(_nPin));
	if(!_isValide)
		return;
		
	// Direction
	_isValide = _echo(_path+"/direction", _type == Input ? "in" : "out");
	if(!_isValide)
		return;
		
	// ------ Nothing else for output ------
	if(_type == Output)
		return;
		
	// Pull up/down
	_isValide = _echo(_path+"/active_low", enablePullUp ? "1" : "0");
	if(!_isValide)
		return;
			
	// Event, file descriptor
	_isValide = _echo(_path+"/edge", "none");
	_fd = open(std::string(_path+"/value").c_str(), O_RDONLY);		
}
Gpio::~Gpio() {
	if(_type == Output)
		setValue(Low);
		
	_echo("/sys/class/gpio/unexport", std::to_string(_nPin));
}
	
// Statics
const std::string Gpio::TypeToString(const Type t) {
	switch(t) {
		case Input:  return "input";
		case Output: return "output";
	}
	return "";
}
const std::string Gpio::LevelToString(const Level l) {
	switch(l) {
		case High: 	return "high";
		case Low: 	return "low";
	}
	return "";
}
const std::string Gpio::EventToString(const Event e) {
	switch(e) {
		case Rising: 	return "rising";
		case Falling: 	return "falling";
		case Both: 		return "both";
	}
	return "none";
}

const Gpio::Level Gpio::StringToLevel(const std::string& sl) {
	if(sl == "0")
		return Low;
	return High;
}
	
// Methods
bool Gpio::setValue(const Level l) {
	if(!_isValide)
		return false;
		
	if(_type == Output) { // Only outputs should be modified
		_level = l;
		return _echo(_path+"/value", _level == Low ? "0" : "1");	
	}
	
	return false;
}
bool Gpio::setEvent(const Event e) {
	if(!_isValide || _fd < 0)
		return false;
		
	_event = e;
		
	return _echo(_path+"/edge", EventToString(e));
}
const Gpio::Level Gpio::readValue() {
	if(!_isValide)
		return Low;
		
	if(_type == Input) { // Only inputs need to be re-read
		const std::string pathFile = _path + "/value";
		std::ifstream file;

		file.open(pathFile);
		if(!file.is_open()) {
			std::cout << "Could not open " << pathFile << std::endl;
			return Low;
		}
	
		std::string value;
		file >> value;
		file.close();	
		
		_level = value == "0" ? Low : High; 
	}
	
	return _level;
}

// Accessors
const int Gpio::id() const {
	return _nPin;
}
const int Gpio::fd() const {
	return _fd;
}
const bool Gpio::isValide() const {
	return _isValide;
}
const std::string Gpio::getPath() const {
	return _path;
}

// -- Private
// Statics
bool Gpio::_echo(const std::string& pathFile, const std::string& command) {
	std::ofstream file;
	
	file.open(pathFile, std::ios::trunc);
	if(!file.is_open()) {
		std::cout << "Could not open " << pathFile << std::endl;
		return false;
	}
	file << command;
	file.close();	
	
	return true;
}

#endif