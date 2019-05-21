#pragma once

#include "WinLinConversion.hpp"
#include <string>
#include <sstream>
#include <vector>

// ---------------- Different types ----------------
enum IpType {
	Ip_error,
	Ip_v4,
	Ip_v6,
};
enum ProtoType {
	Proto_error,
	Proto_Tcp,
	Proto_Udp
};



// ---------------------------- Tools ----------------------------
struct SocketTool {
// Public:
	static IpType determineType(const std::string& ip) {
		// Find type based on . or : occurence
		for (char c : ip) {
			if (c == '.')
				return Ip_v4;
			
			if (c == ':')
				return Ip_v6;
		}
		return Ip_error;
	};
	
	static bool determineValide(const std::string& ip, IpType type = Ip_error) {
		// Check type
		if(type == Ip_error)
			type = determineType(ip);
		
		if(determineType(ip) == Ip_error)
			return false;
		
		// Check number of elements
		std::vector<int> target;
		char delimit 	= type == Ip_v4 ? '.' : ':';
		int base 		= type == Ip_v4 ? 10 : 16;

		std::istringstream flow(ip);
		std::string s;
		int iCompress = -1;
		int i = 0;

		while (getline(flow, s, delimit)) {
			if (type == Ip_v6 && s.empty()) {
				if (iCompress <= 0) { // found ::
					if(iCompress == -1)
						iCompress = i;
					continue;
				}
				else { // multiple ::, bad
					return false;
				}
			}

			try {
				target.push_back(stoi(s, nullptr, base));
			}
			catch (...) {
				return false;
			}
			i++;
		}
		
		// Size needed
		const int size_target = type == Ip_v4 ? 4 : 8;
		const int max_target = (type == Ip_v4 ? (1 << 8) : (1 << 16)) -1;
		
		// Was compressed
		if (type == Ip_v6 && iCompress > -1) {
			int nToAdd = size_target - (int)target.size();
			if (nToAdd < 0)
				return false;

			std::vector<int> pad((size_t)nToAdd, (int)0);
			target.insert(target.begin() + iCompress, pad.begin(), pad.end());
		}
		
		// Valide ?
		if((int)target.size() != size_target)
			return false;
		
		for (int t : target) {
			if (t < 0 || t > max_target) {
				return false;
			}
		}
		
		return true;
	}
};




// ---------------------------- Address ----------------------------
struct SocketAddress {
// Public:	
	SocketAddress() : 
		_ip(""),
		_port(0),
		_type(Ip_error), 
		_ipDefined(false),
		_sizeSockaddr(0)
	{

	}
	SocketAddress(const IpType type, const sockaddr& addr, const socklen_t size) :
		_ip(""),
		_port(0),
		_type(type), 
		_ipDefined(false),
		_sizeSockaddr(size),
		_sockaddr(addr)
	{

	}

	// Methods
	bool create(const std::string& ip, int port) {
		_ip 			= ip;
		_port 		= port;
		_type 		= SocketTool::determineType(ip);
		_ipDefined 	= SocketTool::determineValide(ip, _type);
		
		return _create();
	}
	bool create(const IpType ipTypeHint, int port) {
		_port = port;
		_type = ipTypeHint;
		
		return _create();
	}
	
	void memset(int c) {
		::memset(&_sockaddr, c, size());
	}
	
	// Statics
	static bool compareHost(const SocketAddress& addressA, const SocketAddress& addressB) {
		if(addressA._type != addressB._type || addressA.size() != addressB.size() || addressA._type == Ip_error) {
			return false;
		}
		else if(addressA._type == Ip_v4) {			
			sockaddr_in* pA = (sockaddr_in*)(&addressA._sockaddr);
			sockaddr_in* pB = (sockaddr_in*)(&addressB._sockaddr);
			
			 return (pA->sin_addr.s_addr == pB->sin_addr.s_addr);
		}
		else if(addressA._type == Ip_v6) {
			sockaddr_in6* pA = (sockaddr_in6*)(&addressA._sockaddr);
			sockaddr_in6* pB = (sockaddr_in6*)(&addressB._sockaddr);
			
			unsigned char* pAs = pA->sin6_addr.s6_addr;
			unsigned char* pBs = pB->sin6_addr.s6_addr;
			
			for(int i = 0; i < 16; i++) {
				if(pAs[i] != pBs[i])
					return false;
			}
			
			return true;
		}
		
		return false;
	}
	

	// Getters
	IpType type() const {
		return _type;
	}
	bool ipDefined() const {
		return _ipDefined;
	}
	bool created() const {
		return _sizeSockaddr > 0;
	}
	socklen_t size() const {
		return _sizeSockaddr;
	}
	int port() const {
		return _port;
	}
	const sockaddr * get() const {
		return &_sockaddr;
	}

private:
	// Methods
	bool _create() {
		// Already created
		if(_sizeSockaddr > 0)
			return false;
		// Ip not valide
		if(_type == Ip_error)
			return false;
		// Port not valide
		if(_port < (1 << 1) || _port > (1 << 16))
			return false;
		
		if (_type == Ip_v4) {
			sockaddr_in* pSock4 = (sockaddr_in*)(&_sockaddr);
			::memset(pSock4, 0, sizeof(_sockaddr));
			
			pSock4->sin_family 	= AF_INET;
			pSock4->sin_port 		= htons(_port);
			
			if(!_ipDefined) {
				pSock4->sin_addr.s_addr = htonl(INADDR_ANY);
			}
			else {
				if(inet_pton(AF_INET, _ip.c_str(), &pSock4->sin_addr) != 1)
					return false;
			}
			
			// Socket Address
			_sizeSockaddr	= (socklen_t)sizeof(sockaddr_in);
		}	
		else if (_type == Ip_v6) {
			sockaddr_in6* pSock6 = (sockaddr_in6*)(&_sockaddr);
			::memset(pSock6, 0, sizeof(_sockaddr));
			
			pSock6->sin6_family	= AF_INET6;
			pSock6->sin6_port 	= htons(_port);
			
			if(!_ipDefined) {
				pSock6->sin6_addr = in6addr_any;
			}
			else {
				if(inet_pton(AF_INET6, _ip.c_str(), &pSock6->sin6_addr) != 1)
					return false;
			}
			
			// Socket Address
			_sizeSockaddr	= sizeof(sockaddr_in6);
		}
		
		return _sizeSockaddr>0;
	}
		
	// Members
	std::string _ip;
	int _port;
	IpType _type;
	bool _ipDefined;
	
	socklen_t _sizeSockaddr;
	sockaddr _sockaddr;
};


// ------------------------------ Socket ----------------------------
struct Socket {
// Public:
	Socket(SOCKET id = INVALID_SOCKET, ProtoType proto = Proto_error, SocketAddress add = SocketAddress()) :
		_socket(id),
		_protoType(proto),
		_address(add)
	{

	}
	
	// Methods
	bool connect(const SocketAddress& address, const ProtoType proto, const bool blocking = false) {
		if(initialized())
			return true;
		
		// -- Creation --
		if(!_createSocket(address, proto))
			return false;
		
		// -- Connection --
		if(::connect(_socket, _address.get(), _address.size()) != 0) {
			close();
			return false;
		}
		
		// -- Options --
		if(wlc::setNonBlocking(_socket, !blocking) < 0) {
			close();
			return false;
		}
		
		return true;
	}
	bool bind(const SocketAddress& address, const ProtoType proto, const bool blocking = false) {
		if(initialized())
			return true;
		
		// -- Creation --
		if(!_createSocket(address, proto))
			return false;
		
		// -- Bounding --
		std::cout << "Bind " << (proto == Proto_Tcp ? "Tcp" : "Udp") << std::endl;
		if(::bind(_socket, _address.get(), _address.size()) == SOCKET_ERROR) {
			std::cout << wlc::getError() << std::endl;
			close();
			return false;
		}
		
		// -- Options --
		if(wlc::setNonBlocking(_socket, !blocking) < 0) {
			close();
			return false;
		}
		
		return true;
	}
	bool accept(Socket& socketAccepted) {
		if(_socket == INVALID_SOCKET || _protoType == Proto_error)
			return false;
		
		sockaddr address;
		socklen_t slen = sizeof(address);
		SOCKET socketId = ::accept(_socket, &address, &slen);
		
		if(socketId == SOCKET_ERROR)
			return false;
		
		// Create socket | address
		socketAccepted = Socket(socketId, _protoType, SocketAddress(_address.type(), address, slen));

		return socketAccepted.initialized();
	}
	
	void close() {
		if(_socket == INVALID_SOCKET)
			return;
		
		wlc::closeSocket(_socket);
		
		_socket = INVALID_SOCKET;
	}
	
	// Getters
	bool initialized() const {
		return _socket != INVALID_SOCKET && _address.size() > 0 && _protoType != Proto_error;
	}
	
	const SocketAddress& address() const {
		return _address;
	}
	IpType type() const {
		return _address.type();
	}
	ProtoType proto() const {
		return _protoType;
	}
	SOCKET get() const {
		return _socket;
	}
	
private:
	// Methods
	bool _createSocket(const SocketAddress& address, const ProtoType proto) {
		_address 	= address; 
		_protoType 	= proto;
		
		// -- Family --
		int family = 0;
		if (_address.type() == Ip_error) 
			return false;
		else if (_address.type() == Ip_v4)
			family = PF_INET;
		else if (_address.type() == Ip_v6)
			family = PF_INET6;
		
		// -- Proto --
		if(proto == Proto_error)
			return false;
		else if(proto == Proto_Tcp)
			_socket = socket(family, SOCK_STREAM, IPPROTO_TCP);
		else if(proto == Proto_Udp)
			_socket = socket(family, SOCK_DGRAM , IPPROTO_UDP);		
		
		return true;
	}
	
	// Members
	SOCKET 			_socket;
	ProtoType 		_protoType;
	SocketAddress 	_address;
};

