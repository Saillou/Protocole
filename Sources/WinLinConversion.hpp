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

/* Constantes OS name */
#define OPERATING_SYSTEM_MAC 0
#define OPERATING_SYSTEM_LINUX 1
#define OPERATING_SYSTEM_WINDOWS 2

/* Platform specifics */
#ifdef __linux__
	/* Os */
	#define USED_OS OPERATING_SYSTEM_LINUX
	
	/* Includes */
	#include <sys/select.h>
	#include <sys/socket.h>
	#include <sys/types.h>
	#include <netinet/in.h>	
	#include <arpa/inet.h>
	#include <fcntl.h>
	#include <errno.h>
	
	/* Types */
	#ifndef SOCKET
		#define SOCKET int
	#endif
	
#elif _WIN32
	/* Os */
	#define USED_OS OPERATING_SYSTEM_WINDOWS
	
	/* Includes */
	#include <winsock2.h>
	#include <ws2tcpip.h>
	
	/* Types */
	
#endif

namespace wlc {
	// --- Initialization ---
	bool initSockets() {
#ifdef _WIN32 	
		WSADATA wsa;
		if (WSAStartup(MAKEWORD(2,2), &wsa) != 0)
			return false;
#elif __linux__
		/* Nothing to do*/
#endif	

		return true;
	}
	
	// --- Un-initialization ---
	void uninitSockets() {
#ifdef _WIN32 	
		WSACleanup();
#elif __linux__
		/* Nothing to do*/
#endif
	}
	
	// --- Getting errors ---
	int getError() {
#ifdef _WIN32 	
		return WSAGetLastError();
#elif __linux__
		/* Nothing to do*/
#endif

		return 0;
	}
	
	// --- Changing sockets mode ---
	int setNonBlocking(SOCKET idSocket, bool nonBlocking) {
#ifdef __linux__ 
		// Use the standard POSIX 
		int oldFlags = fcntl(idSocket, F_GETFL, 0);
		int flags = nonBlocking ? oldFlags | O_NONBLOCK : oldFlags & ~O_NONBLOCK;
		return fcntl(idSocket, F_SETFL, flags);
	
#elif _WIN32
		// Use the WSA 
		unsigned long ul = nonBlocking ? 1 : 0; // Parameter for FIONBIO
		return ioctlsocket(idSocket, FIONBIO, &ul);
#endif

		// No implementation
		return -1;
	}
	
	// --- Closing sockets ---
	void closeSocket(SOCKET idSocket) {
#ifdef _WIN32 
		closesocket(idSocket);
#elif __linux__
		close(idSocket);
#endif
	}


}



