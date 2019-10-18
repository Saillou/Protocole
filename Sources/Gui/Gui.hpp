#pragma once
#ifdef __linux__

#include <cairo.h>
#include <gtk/gtk.h>

#include <iostream>
#include <cmath>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <string>

#include "core.hpp"

class Gui {
public:
	Gui(const std::string& name = "Gui", const Frame& frame = Frame(640, 480, 3), int refreshTime = -1);
	~Gui();
	
	void update(const Frame& frame, int refreshTime = -1);
	void close();
	
	bool isRunning() const;
	
private:
	// - Methods
	void _init();
	
	// Creation window
	static void _onActivate(GtkApplication* app, Gui* _this);
	void _activate();
	
	// Draw area
	static gboolean _onDraw(GtkWidget* drawArea, cairo_t *cr, Gui* _this);
	gboolean _draw(cairo_t *cr);

	// Timer interrupt
	static gboolean _onTimeout(Gui* _this);
	gboolean _timeout();

	// Destruction
	static void _onDestroy(void);
	
	// Members
	bool _refresh;
	int _refreshTime;
	std::atomic<bool> _running;
	std::string _name;
	Size _size;
	
	GtkApplication* _app;
	GtkWidget* _mainWindow;
	GtkWidget* _drawArea;
	guint _idTimer;
	
	Frame _frame;
	std::mutex _mutGui;
	std::thread _threadGui;
};

#endif