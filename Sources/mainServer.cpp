#include <iostream>
#include <csignal>

#include "StreamDevice/ServerDevice.hpp"
#include "Tool/Timer.hpp"

#include "GPIO/ManagerGpio.hpp"
#include "GPIO/Gpio.hpp"



namespace Globals {
	// Constantes
	const std::string PATH_0 = "/dev/video0";
	const std::string PATH_1 = "/dev/video1";
	
	// Variables
	volatile std::sig_atomic_t signalStatus = 0;
	
	std::atomic<bool> G_ready(false);
	std::atomic<bool> G_stop(false);
	
	std::atomic<bool> G_shutdown(false);
	std::atomic<bool> G_countLong(false);
	std::atomic<int> G_count(0);
}


// --- Signals ---
static void sigintHandler(int signal) {
	Globals::signalStatus = signal;
}

static void onBtnShut(const int idGpio, const Gpio::Level level, const Gpio::Event event) {
	if(!Globals::G_ready)
		return;
		
	// Bounce
	Timer::wait(10);
	
	// Switch mode
	if(event == Gpio::Falling) {
		Globals::G_countLong = true;
		Globals::G_count = 0;
	}
	else if(event == Gpio::Rising) {
		Globals::G_countLong = false;
		Globals::G_count = 0;
		if(Globals::G_stop)
			Globals::G_shutdown = true;
	}
}




// --- Entry point ---
int main(int argc, char* argv[]) {	
	// - Install signal handler
	std::signal(SIGINT, sigintHandler);
	
	// - Gpio
	Gpio gLedRec(100, Gpio::Output); 	// Rock64 = 15 (GPIO3_A4)
	Gpio gBtnRec(101, Gpio::Input);		// Rock64 = 16 (GPIO3_A5)
	Gpio gLedShut(102, Gpio::Output);  	// Rock64 = 18 (GPIO3_A6)
	Gpio gBtnShut(103, Gpio::Input);   	// Rock64 = 22 (GPIO3_A7)
	Gpio gNpn(81, Gpio::Output);  		// Rock64 = 3  (GPIO2_C1)
	
	gLedShut.setValue(Gpio::High);
	gNpn.setValue(Gpio::High);			// Activate relay (shutdown button)
	
	ManagerGpio manager;
	manager.addEventListener(gBtnShut, Gpio::Both, onBtnShut);
	manager.startListening();

	
	// - Devices
	ServerDevice device0(Globals::PATH_0, 8000);
	ServerDevice device1(Globals::PATH_1, 8002);
	
	// -------- Main loop --------  
	bool error = false;
	if(!device0.open(1000)) {
		std::cout << "Can't open device" << std::endl;
		error = true;
	}
	if(!device1.open(1000)) {
		std::cout << "Can't open device" << std::endl;
		error = true;
	}
	
	Globals::G_ready = true;
	for(Timer timer; !Globals::G_stop && Globals::signalStatus != SIGINT; timer.wait(100)) {
		// ... Do other stuff ...
		if(error) {
			static int iLed = 1;
			if(iLed % 3 == 0) {
				gLedRec.setValue(gLedRec.readValue() == Gpio::High ? Gpio::Low : Gpio::High);
			}
			if(iLed % 10 == 0) {
				gLedShut.setValue(gLedShut.readValue() == Gpio::High ? Gpio::Low : Gpio::High);
				iLed = 0;
			}
			iLed++;
		}
		
		// Shutting down
		if(Globals::G_countLong) {
			Globals::G_count++;
			
			if(Globals::G_count > 20) // 20 * 100ms => 2 secondes
				Globals::G_stop = true;
		}
	}
	
	// -- End
	device0.close();
	device1.close();
	
	std::cout << "Clean exit" << std::endl;
	
	if(Globals::signalStatus != SIGINT) {
		// Waiting for shutting down
		while(!Globals::G_shutdown) {
			Timer::wait(100);
			gLedShut.setValue(gLedShut.readValue() == Gpio::High ? Gpio::Low : Gpio::High);
		}
		gNpn.setValue(Gpio::Low); 				// Disable relay
	
		std::cout << "Shutting down" << std::endl;
		system("shutdown -h now");
	}
	
	gLedShut.setValue(Gpio::Low);
	gLedRec.setValue(Gpio::Low);
	return 0;
}