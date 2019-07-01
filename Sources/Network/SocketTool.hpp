#pragma once

#include "WinLinConversion.hpp"
#include "Message.hpp"

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
		_sizeSockaddr(size)
	{
		 if(_type != Ip_error)
			 memcpy(_get(), &addr, size);
	}
	SocketAddress(const sockaddr_in& addr4) :
		_ip(""),
		_port(0),
		_type(Ip_v4), 
		_ipDefined(false),
		_sizeSockaddr(sizeof(addr4)),
		_sockaddr4(addr4)
	{

	}
	SocketAddress(const sockaddr_in6& addr6) :
		_ip(""),
		_port(0),
		_type(Ip_v6), 
		_ipDefined(false),
		_sizeSockaddr(sizeof(addr6)),
		_sockaddr6(addr6)
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
	
	void memset(int c = 0) {
		if(_sizeSockaddr > 0) {
			::memset(_get(), c, _sizeSockaddr);
		}
	}
	
	// Statics
	static bool compareHost(const SocketAddress& addressA, const SocketAddress& addressB) {
		if(addressA._type != addressB._type || addressA.size() != addressB.size() || addressA._type == Ip_error) {
			return false;
		}
		else if(addressA._type == Ip_v4) {			
			 return (addressA._sockaddr4.sin_addr.s_addr == addressB._sockaddr4.sin_addr.s_addr);
		}
		else if(addressA._type == Ip_v6) {
			for(int i = 0; i < 16; i++) {
				if(addressA._sockaddr6.sin6_addr.s6_addr[i] != addressB._sockaddr6.sin6_addr.s6_addr[i])
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
		 if(_type == Ip_error)
			 return nullptr;
		else if(_type == Ip_v4)
			return (sockaddr*)&_sockaddr4;
		else if(_type == Ip_v6)
			return (sockaddr*)&_sockaddr6;
		
		return nullptr;
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
			_sizeSockaddr = sizeof(_sockaddr4);
			
			memset();
			_sockaddr4.sin_family 	= AF_INET;
			_sockaddr4.sin_port 	= htons(_port);
			
			if(!_ipDefined) {
				_sockaddr4.sin_addr.s_addr	= htonl(INADDR_ANY);
			}
			else {
				if(inet_pton(AF_INET, _ip.c_str(), &_sockaddr4.sin_addr.s_addr) != 1)
					return false;
			}
		}	
		else if (_type == Ip_v6) {
			_sizeSockaddr = sizeof(_sockaddr6);
			
			memset();
			_sockaddr6.sin6_family = AF_INET6;
			_sockaddr6.sin6_port 	= htons(_port);

			if(!_ipDefined) {
				_sockaddr6.sin6_addr = in6addr_any;
			}
			else {
				if(inet_pton(AF_INET6, _ip.c_str(), &_sockaddr6.sin6_addr.s6_addr) != 1)
					return false;
			}
		}
		
		return _sizeSockaddr>0;
	}
		
	sockaddr* _get() {
		 if(_type == Ip_error)
			 return nullptr;
		else if(_type == Ip_v4)
			return (sockaddr*)&_sockaddr4;
		else if(_type == Ip_v6)
			return (sockaddr*)&_sockaddr6;
		
		return nullptr;
	}
		
	// Members
	std::string _ip;
	int _port;
	IpType _type;
	bool _ipDefined;
	
	socklen_t _sizeSockaddr;
	sockaddr_in _sockaddr4;
	sockaddr_in6 _sockaddr6;
	
};

struct IAddress {
	// Ctor
	IAddress(const std::string& ipAddress, const int p) : 
		ip(ipAddress), 
		port(p) 
	{
		
	}
	
	// Methods
	const bool isValide() const {
		return SocketTool::determineValide(ip) && (port > (1 << 1) && port < (1 << 16));
	}
	
	// Members
	std::string ip;
	int port;
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
		
		// -- Options --
		if(wlc::setNonBlocking(_socket, !blocking) < 0 || wlc::setReusable(_socket, true) < 0) {
			close();
			return false;
		}
		
		// -- Connection --
		if(::connect(_socket, _address.get(), _address.size()) != 0) {
			if (!wlc::errorIs(wlc::WOULD_BLOCK, wlc::getError())) {
				close();
				return false;
			}

			// Connection pending
			const int TIMEOUT = 500; // 0.5 sec
			pollfd fdRead = { 0 };
			fdRead.fd = _socket;
			fdRead.events = POLLIN;

			int pollResult = wlc::polling(&fdRead, 1, TIMEOUT);
			if (pollResult < 0 || pollResult == 0) { 	// failed || timeout
				close();
				return false;
			}
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
		if(::bind(_socket, _address.get(), _address.size()) == SOCKET_ERROR) {
			std::cout << wlc::getError() << std::endl;
			close();
			return false;
		}
		
		// -- Options --
		if(wlc::setNonBlocking(_socket, !blocking) < 0 || wlc::setReusable(_socket, true) < 0) {
			close();
			return false;
		}
		
		return true;
	}
	bool accept(Socket& socketAccepted) {
		std::cout << "accept: " << _socket << " " << _protoType << std::endl;
		if(_socket == INVALID_SOCKET || _protoType == Proto_error)
			return false;
		
		sockaddr address;
		socklen_t slen =  _address.size();
		SOCKET socketId = ::accept(_socket, &address, &slen);
		
		std::cout << "Id: " << socketId << std::endl;
		if(socketId == SOCKET_ERROR)
			return false;
		
		// Create socket | address
		socketAccepted = Socket(socketId, _protoType, SocketAddress(_address.type(), address, slen));
		wlc::setReusable(socketId, true);
		wlc::setNonBlocking(socketId, true);

		return socketAccepted.initialized();
	}
	
	bool receiveFrom(ssize_t& recvLen, char* buffer, const int bufferSize, SocketAddress& senderAddress) const {
		if(type() == Ip_v4) {
			sockaddr_in clientAddress;
			socklen_t slen = sizeof(clientAddress);
			
			if ((recvLen = recvfrom(_socket, buffer, bufferSize, 0, (sockaddr*)&clientAddress, &slen)) == SOCKET_ERROR)
				return false;
			
			senderAddress = SocketAddress(clientAddress);
			return senderAddress.size() > 0;
		}
		else if(type() == Ip_v6) {
			sockaddr_in6 clientAddress;
			socklen_t slen = sizeof(clientAddress);
			
			if ((recvLen = recvfrom(_socket, buffer, bufferSize, 0, (sockaddr*)&clientAddress, &slen)) == SOCKET_ERROR)
				return false;
			
			senderAddress = SocketAddress(clientAddress);
			return senderAddress.size() > 0;
		}
		
		return false;
	}
	bool sendTo(const Message& msg, const SocketAddress& receiverAddress) const {
		bool error = false;
		const int bufferSize = (int)msg.length();
		const char* buffer = msg.data();
		
		if(bufferSize < 64000) { // 64k is almost the limit (exactly it should be [65 535 - socketAddressSize] ~ 65 500 bytes)
			// Send header + content
			if(sendto(_socket, msg.data(), bufferSize, 0, receiverAddress.get(), receiverAddress.size()) != bufferSize)
				return false;
		}
		else {		
			unsigned int codeFrag	= msg.code() | Message::FRAGMENT;
			uint64_t timestampMsg	= msg.timestamp();
			
			// - Create header
			Message msgHeader(codeFrag | Message::HEADER, nullptr, msg.size(), timestampMsg);
			if(sendto(_socket, msgHeader.data(), 14, 0, receiverAddress.get(), receiverAddress.size()) != 14)
				return false;
			
			// - Cut in messages fragment
			int offset 					= 0;
			int limitFragmentSize 	= 60000;
			int totalLengthSend 		= (int)msg.size();
			
			do {
				int sizeToSend = totalLengthSend > limitFragmentSize ? limitFragmentSize : totalLengthSend;
				
				Message msgFrag(codeFrag, buffer+14+offset, (size_t)sizeToSend, timestampMsg);
				
				if(sendto(_socket, msgFrag.data(), 14+sizeToSend, 0, receiverAddress.get(), receiverAddress.size()) != 14+sizeToSend)
					return false;
				
				offset += sizeToSend;
				totalLengthSend -= sizeToSend;
				limitFragmentSize--; // Avoid to get packets of the same size
				
			} while(totalLengthSend > 0);	
		}
		
		return true;
	}
	bool send(const Message& msg) const {
		return (::send(_socket, msg.data(), (int)msg.length(), 0) == (int)msg.length());
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

