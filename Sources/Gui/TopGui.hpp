#pragma once
#ifdef __linux__

#include "Gui.hpp"

#include <algorithm>
#include <memory>
#include <map>

class TopGui {
public:
	static void show(const std::string& name, const Frame& frame);
};

#endif