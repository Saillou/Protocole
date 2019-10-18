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

/* ---- Platform specifics ---- */
// Linux
#ifdef __linux__	
	/* Includes */
	#include <sys/select.h>
	#include <sys/socket.h>
	#include <sys/types.h>
	#include <sys/poll.h>
	#include <netinet/in.h>	
	#include <arpa/inet.h>
	#include <fcntl.h>
	#include <errno.h>
	#include <unistd.h>
	
	/* Names */
	#ifndef SOCKET
		#define SOCKET unsigned int
	#endif
	
	#ifndef INVALID_SOCKET
		#define INVALID_SOCKET -1
	#endif
	
	#ifndef SOCKET_ERROR
		#define SOCKET_ERROR -1
	#endif
	
// Windows	
#elif _WIN32	
	/* Includes */
	#include <winsock2.h>
	#include <ws2tcpip.h>
	
	/* Names */
	typedef int socklen_t;
	typedef int ssize_t;
	typedef WSAPOLLFD pollfd;

#endif

namespace wlc {
	// --- Initialization ---
	bool initSockets();
	
	// --- Un-initialization ---
	void uninitSockets();
	
	// --- Getting errors ---
	int getError();
	
	// --- Error Code Translation ---
	enum ErrorCode {
		WOULD_BLOCK, 
		INVALID_ARG, 
		NOT_CONNECT, 
		REFUSED_CONNECT, 
		MSG_SIZE
	};
	
	bool errorIs(const ErrorCode& eCode, const int error);
	
	// --- Changing sockets mode ---
	int setNonBlocking(SOCKET idSocket, bool nonBlocking);
	
	int setReusable(SOCKET idSocket, bool reusable);
	
	// --- Non blocking ---
	int polling(pollfd* pfds, unsigned long nfds, int timeout);
	
	// --- Closing sockets ---
	void closeSocket(SOCKET idSocket);
}



