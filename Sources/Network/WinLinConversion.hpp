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
	bool initSockets() {
#ifdef _WIN32 	
		WSADATA wsa;
		if (WSAStartup(MAKEWORD(2,2), &wsa) != 0)
			return false;
#endif	

		return true;
	}
	
	// --- Un-initialization ---
	void uninitSockets() {
#ifdef _WIN32 	
		WSACleanup();
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
			return (error == WSAEWOULDBLOCK) || (error == EAGAIN);
#elif __linux__	
			return (error == EWOULDBLOCK) || (error == EAGAIN);
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
#ifdef _WIN32
		// Use the WSA 
		unsigned long ul = nonBlocking ? 1 : 0; // Parameter for FIONBIO
		return ioctlsocket(idSocket, FIONBIO, &ul);
#elif __linux__ 
		// Use the standard POSIX 
		int oldFlags = fcntl(idSocket, F_GETFL, 0);
		int flags = nonBlocking ? oldFlags | O_NONBLOCK : oldFlags & ~O_NONBLOCK;
		return fcntl(idSocket, F_SETFL, flags);	
#endif

		return -1;
	}
	
	int setReusable(SOCKET idSocket, bool reusable) {
		int on = reusable ? 1 : 0; // Parameter for SO_REUSEADDR
		return setsockopt(idSocket, SOL_SOCKET,  SO_REUSEADDR, (char *)&on, sizeof(on));
	}
	
	// --- Non blocking ---
	int polling(pollfd* pfds, unsigned long nfds, int timeout) {
#ifdef _WIN32 
		return WSAPoll(pfds, nfds, timeout);
#elif __linux__
		return poll(pfds, nfds, timeout);
#endif		
	}
	
	// --- Closing sockets ---
	void closeSocket(SOCKET idSocket) {
		if(idSocket < 0)
			return;
		
#ifdef _WIN32 
		closesocket(idSocket);
#elif __linux__
		close(idSocket);
#endif
	}


}



