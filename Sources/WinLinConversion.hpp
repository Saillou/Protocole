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
	
#elif _WIN32
	/* Os */
	#define USED_OS OPERATING_SYSTEM_WINDOWS
	
	/* Includes */
	#include <winsock2.h>
	#include <ws2tcpip.h>
	
	/* Names */
	#ifndef socklen_t
		#define socklen_t int
	#endif
	
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
		return errno;
#endif

		return 0;
	}
	
	// --- Error Code Translation ---
	enum ErrorCode {
		WOULD_BLOCK, 
		INVALID_ARG, 
		NOT_CONNECT, 
		REFUSED_CONNECT, 
		MSG_SIZE
	};
	
	bool errorIs(const ErrorCode& eCode, const int error) {
		switch(eCode) {
		case WOULD_BLOCK:
#ifdef _WIN32 	
			return (error == WSAEWOULDBLOCK);
#elif __linux__	
			return (error == EWOULDBLOCK);
#endif
		break;
		
		case INVALID_ARG:
#ifdef _WIN32 	
			return (error == WSAEINVAL);
#elif __linux__		
			return (error == EINVAL);
#endif
		break;
		
		case NOT_CONNECT:
#ifdef _WIN32 	
			return (error == WSAENOTCONN);
#elif __linux__		
			return (error == ENOTCONN);
#endif
		break;
		
		case REFUSED_CONNECT:
#ifdef _WIN32 	
			return (error == WSAECONNRESET);
#elif __linux__		
			return (error == ECONNREFUSED);
#endif
		break;
		
		case MSG_SIZE:
#ifdef _WIN32 	
			return (error == WSAEMSGSIZE);
#elif __linux__		
			return (error == EMSGSIZE);
#endif
		break;
		
		}
		return false;
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



