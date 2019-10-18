#include "Gui.hpp"
#ifdef __linux__

Gui::Gui(const std::string& name, const Frame& frame, int refreshTime) : 
	_refresh(false), 
	_refreshTime(refreshTime > 0 ? refreshTime : 30), 
	_running{false}, 
	_name(name), 
	_size(frame.width, frame.height),
	_app(NULL),
	_mainWindow(NULL),
	_drawArea(NULL),
	_idTimer(0),
	_frame(frame),
	_threadGui(&Gui::_init, this)
	
{
	// Nothing
}

Gui::~Gui() {
	close();
}

void Gui::update(const Frame& frame, int refreshTime) {
	_mutGui.lock();
	
	// Change echantillonnage
	if(refreshTime > 1)
		_refreshTime = refreshTime;
	
	// Change frame	
	_frame = frame;
	_refresh = true;
	
	_mutGui.unlock();
}

void Gui::close() {
	_running = false;
	
	if(_threadGui.joinable())
		_threadGui.join();
}

bool Gui::isRunning() const {
	return _running;
}

// - Private methods
void Gui::_init() {
	_running = true;
	_app = gtk_application_new("Test.app", G_APPLICATION_FLAGS_NONE);

	// Events
	g_signal_connect(_app, "activate", G_CALLBACK(this->_onActivate), this);
	
	// Loop
	_refresh = true;
	g_application_run(G_APPLICATION(_app), 0, NULL);
	
	// End
	g_object_unref(_app);	
	_running = false;		
}

// Creation window
void Gui::_onActivate(GtkApplication* app, Gui* _this) {
	return _this->_activate();
}
void Gui::_activate() {
	// Create window
	_mainWindow = gtk_application_window_new(_app);
	
	gtk_window_set_title(GTK_WINDOW(_mainWindow), _name.c_str());
	gtk_window_set_default_size(GTK_WINDOW(_mainWindow), _size.width, _size.height);
	
	// Create drawing area
	_drawArea = gtk_drawing_area_new();
	
	gtk_widget_set_size_request(_drawArea, _size.width, _size.height);
	gtk_container_add(GTK_CONTAINER(_mainWindow), _drawArea);
	
	// Events
	g_signal_connect(_mainWindow, "destroy", G_CALLBACK(_onDestroy), NULL);
	g_signal_connect(_drawArea, "draw", G_CALLBACK(_onDraw), this);
	_idTimer = g_timeout_add(_refreshTime, (GSourceFunc)_onTimeout, this);
	
	// Show
	gtk_widget_show_all(_mainWindow);
}

// Draw area
gboolean Gui::_onDraw(GtkWidget* drawArea, cairo_t *cr, Gui* _this) {
	return _this->_draw(cr);
}
gboolean Gui::_draw(cairo_t *cr) {
	if(!GTK_IS_WIDGET (_drawArea))
		return FALSE;
		
	// Environnement
	int width, height;
	gtk_window_get_size(GTK_WINDOW(gtk_widget_get_toplevel(_drawArea)), &width, &height);
	
	if(_frame.width != width || _frame.height != height) {
		_size.width = _frame.width;
		_size.height = _frame.height;
		
		gtk_window_set_default_size(GTK_WINDOW(_mainWindow), _size.width, _size.height);
		gtk_widget_set_size_request(_mainWindow, _size.width, _size.height);
		gtk_widget_set_size_request(_drawArea, _size.width, _size.height);
	}
	
	// Create pixels
	_mutGui.lock();	
	GdkPixbuf* pixbuf = gdk_pixbuf_new_from_data(
		_frame.data(),
		GDK_COLORSPACE_RGB, FALSE, 
		_frame.bitsPerSample(),
		_frame.width, _frame.height,
		_frame.bytesPerRow(),
		NULL, NULL
	);
	
	// Draw pixels
	gdk_cairo_set_source_pixbuf(cr, pixbuf, 0, 0);
	cairo_paint(cr);
	_mutGui.unlock();
	
	_refresh = false;
	return TRUE;		
}

// Timer interrupt
gboolean Gui::_onTimeout(Gui* _this) {
	return _this->_timeout();
}
gboolean Gui::_timeout() {
	if(!GTK_IS_WIDGET(_drawArea))
		return FALSE;
	
	if(!_running) {
		gtk_widget_destroy(_mainWindow);
		return FALSE;
	}
	
	if(_refresh)
		gtk_widget_queue_draw(_drawArea);
		
	// Timer time change
	g_source_remove(_idTimer);
	
	_mutGui.lock();
	_idTimer = g_timeout_add(_refreshTime, (GSourceFunc)_onTimeout, this);
	_mutGui.unlock();
	
	return TRUE;
}

// Destruction
void Gui::_onDestroy(void) {
	std::cout << "Destroy window \n";
}


#endif