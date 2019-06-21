#ifndef GPIO_GPIO_HPP
#define GPIO_GPIO_HPP

#include <iostream>
#include <fstream>
#include <string>

#include <poll.h>
#include <fcntl.h>
#include <unistd.h>

class Gpio {
public:
	// Enums
	enum Type {
		Output, Input
	};
	enum Level {
		Low, High
	};
	enum Event {
		Rising, Falling, Both, None
	};
	
	// Constructors
	Gpio(int nPin, Type t, bool enablePullUp = false);
	~Gpio();
	
	// Statics
	static const std::string TypeToString(const Type t);
	static const std::string LevelToString(const Level l);
	static const std::string EventToString(const Event e);
	
	static const Level StringToLevel(const std::string& sl);
	
	// Methods
	bool setValue(const Level l);
	bool setEvent(const Event e);
	const Level readValue();
	
	// Accessors
	const int id() const;
	const int fd() const;
	const bool isValide() const;
	const std::string getPath() const;
	
private:
	// Methods
	static bool _echo(const std::string& pathFile, const std::string& command);
	
	// Members
	int _nPin;
	Type _type;
	std::string _path;
	Level _level;
	bool _isValide;
	int _fd;
	Event _event;
};

#endif
