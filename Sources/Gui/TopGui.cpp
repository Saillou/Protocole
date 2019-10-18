#include "TopGui.hpp"
#ifdef __linux__

void TopGui::show(const std::string& name, const Frame& frame) {
	// Gui memory
	static std::map<std::string, std::unique_ptr<Gui>> _mapGui;
	
	auto it = _mapGui.find(name);
	if(it != _mapGui.end()) {
		auto& pGui = it->second;
		if(pGui->isRunning()) { 
			// Just update
			pGui->update(frame);
		}
		else {
			// Recreate
			it->second = std::make_unique<Gui>(name, frame);
		}
			
	}
	else {
		// Doesn't exist
		_mapGui[name] = std::make_unique<Gui>(name, frame);
	}
}

#endif