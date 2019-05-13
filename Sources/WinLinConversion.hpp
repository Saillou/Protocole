#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <algorithm>
#include <functional>

#ifdef __linux__ 
	#include <sys/select.h>
	#include <sys/socket.h>
	#include <sys/types.h>
	#include <netinet/in.h>	
	#include <arpa/inet.h>
	#include <fcntl.h>
	
#elif _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
	
#endif

#ifdef __linux__
	#ifndef SOCKET
		#define SOCKET int
	#endif
#endif

namespace wlc {
	
void closeSocket(SOCKET idSocket) {
#ifdef _WIN32 
	closesocket(idSocket);
#elif __linux__
	close(idSocket);
#endif
}



}