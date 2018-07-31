#pragma once

#include "SocketBuffer.h"

#include <string>
#include <WS2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

class Socket
{
private:
	bool m_IsConnectionUDP;
	int m_DataFormat;
	char m_FormatString[30];
	static SOCKADDR_IN SenderAddr;

	int receivetext(char*buf, int max);

public:
	SOCKET m_SocketID;

	explicit Socket(SOCKET sock);
	Socket();
	~Socket();

	bool tcpconnect(const char* address, int port, int mode);
	bool tcplisten(int port, int max, int mode);
	Socket* tcpaccept(int mode) const;
	std::string tcpip() const;
	void setnagle(bool enabled) const;
	bool tcpconnected() const;
	int setsync(int mode) const;
	bool udpconnect(int port, int mode);
	int sendmessage(const char* ip, int port, SocketBuffer* source);
	int receivemessage(int len, SocketBuffer*destination, int length_specific = 0);
	int peekmessage(int size, SocketBuffer*destination) const;
	static int lasterror();
	static std::string GetHostIP(char* address);
	static int SockExit(void);
	static int SockStart(void);
	static char* lastinIP(void);
	static unsigned short lastinPort(void);
	static char* myhost();
	int SetFormat(int mode, char* sep);
};

int SenderAddrSize = sizeof(SOCKADDR_IN);
SOCKADDR_IN Socket::SenderAddr;

inline bool Socket::tcpconnect(const char *address, int port, int mode)
{
	char portString[16];
	sprintf_s(portString, 16, "%d", port);

	if ((m_SocketID = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == SOCKET_ERROR) return false;

	struct addrinfo hints, *servinfo;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	int rv;
	if ((rv = getaddrinfo(address, portString, &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		closesocket(m_SocketID);
		return false;
	}

	if (mode == 2) setsync(1);
	if (connect(m_SocketID, servinfo->ai_addr, (int)(servinfo->ai_addrlen)) == SOCKET_ERROR)
	{
		int WSAerror = WSAGetLastError();
		if (WSAerror != WSAEWOULDBLOCK)
		{
			closesocket(m_SocketID);
			return false;
		}
	}
	
	if (mode == 1) setsync(1);
	return true;
}

inline bool Socket::tcplisten(int port, int max, int mode)
{
	if ((m_SocketID = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) return false;
	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);
	if (mode) setsync(1);
	if (bind(m_SocketID, (LPSOCKADDR)&addr, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
	{
		int errorCode = WSAGetLastError();
		closesocket(m_SocketID);
		return false;
	}
	if (listen(m_SocketID, max) == SOCKET_ERROR)
	{
		int errorCode = WSAGetLastError();
		closesocket(m_SocketID);
		return false;
	}
	return true;
}

inline Socket::Socket(SOCKET sock) : 
	m_SocketID(sock),
	m_IsConnectionUDP(false),
	m_DataFormat(0)
{

}

inline Socket::Socket() :
	m_SocketID(0),
	m_IsConnectionUDP(false),
	m_DataFormat(0)
{

}

inline Socket::~Socket()
{
	if (m_SocketID < 0) return;
	shutdown(m_SocketID, 1);
	closesocket(m_SocketID);
}

inline Socket* Socket::tcpaccept(int mode) const
{
	if (m_SocketID < 0) return nullptr;
	SOCKET sock2;
	if ((sock2 = accept(m_SocketID, (SOCKADDR *)&SenderAddr, &SenderAddrSize)) != INVALID_SOCKET)
	{
		auto sockit = new Socket(sock2);
		if (mode >= 1)sockit->setsync(1);
		return sockit;
	}
	return nullptr;
}

inline std::string Socket::tcpip() const
{
	if (m_SocketID < 0) return nullptr;
	if (getpeername(m_SocketID, (SOCKADDR *)&SenderAddr, &SenderAddrSize) == SOCKET_ERROR) return nullptr;

	char ipAddress[32];
	inet_ntop(AF_INET, &SenderAddr.sin_addr, ipAddress, INET_ADDRSTRLEN);
	return std::string(ipAddress);
}

inline void Socket::setnagle(bool enabled) const
{
	if (m_SocketID < 0) return;
	setsockopt(m_SocketID, IPPROTO_TCP, TCP_NODELAY, (char*)&enabled, sizeof(bool));
}

inline bool Socket::tcpconnected() const 
{
	if (m_SocketID < 0) return false;
	char b;
	if (recv(m_SocketID, &b, 1, MSG_PEEK) == SOCKET_ERROR)
		if (WSAGetLastError() != WSAEWOULDBLOCK) return false;
	return true;
}

inline int Socket::setsync(int mode) const 
{
	if (m_SocketID < 0) return -1;
	u_long i = mode;
	return ioctlsocket(m_SocketID, FIONBIO, &i);
}

inline bool Socket::udpconnect(int port, int mode)
{
	SOCKADDR_IN addr;
	if ((m_SocketID = socket(AF_INET, SOCK_DGRAM, 0)) == SOCKET_ERROR)
		return false;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);
	if (mode)setsync(1);
	if (bind(m_SocketID, (SOCKADDR*)&addr, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
	{
		closesocket(m_SocketID);
		return false;
	}
	m_IsConnectionUDP = true;
	return true;
}

inline int Socket::sendmessage(const char *ip, int port, SocketBuffer *source)
{
	if (m_SocketID <= 0) return -1;
	auto size = 0;
	SOCKADDR_IN addr;
	if (m_IsConnectionUDP)
	{
		struct sockaddr_in sa;
		inet_pton(AF_INET, ip, &(sa.sin_addr)); //  TODO: Is this line even needed?

		size = std::min<int>(source->m_BufferUtilizedCount, 8195);
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = sa.sin_addr.S_un.S_addr;
		size = sendto(m_SocketID, source->m_BufferData, size, 0, (SOCKADDR *)&addr, sizeof(SOCKADDR_IN));
	}
	else
	{
		SocketBuffer sendbuff;
		sendbuff.clear();
		if (m_DataFormat == 0)
		{
			sendbuff.writeushort(source->m_BufferUtilizedCount);
			sendbuff.addBuffer(source);
			size = send(m_SocketID, sendbuff.m_BufferData, sendbuff.m_BufferUtilizedCount, 0);
		}
		else if (m_DataFormat == 1)
		{
			sendbuff.addBuffer(source);
			sendbuff.writechars(m_FormatString);
			size = send(m_SocketID, sendbuff.m_BufferData, sendbuff.m_BufferUtilizedCount, 0);
		}
		else if (m_DataFormat == 2)
			size = send(m_SocketID, source->m_BufferData, source->m_BufferUtilizedCount, 0);
	}
	return ((size == SOCKET_ERROR) ? -WSAGetLastError() : size);
}

inline int Socket::receivetext(char*buf, int max)
{
	auto len = int(strlen(m_FormatString));
	if ((max = recv(m_SocketID, buf, max, MSG_PEEK)) != SOCKET_ERROR)
	{
		int i, ii;
		for (i = 0; i < max; i++)
		{
			for (ii = 0; ii < len; ii++)
				if (buf[i + ii] != m_FormatString[ii]) break;
			if (ii == len)
				return recv(m_SocketID, buf, i + len, 0);
		}
	}
	return -1;
}
inline int Socket::receivemessage(int len, SocketBuffer*destination, int length_specific)
{
	if (m_SocketID < 0) return -1;
	auto size = -1;
	char* buff = nullptr;
	if (m_IsConnectionUDP)
	{
		size = 8195;
		buff = new char[size];
		size = recvfrom(m_SocketID, buff, size, 0, (SOCKADDR *)&SenderAddr, &SenderAddrSize);
	}
	else
	{
		if (m_DataFormat == 0 && !len)
		{
			unsigned short length;
			if (length_specific == 0)
			{
				if ((size = recv(m_SocketID, (char*)&length, 2, 0)) == SOCKET_ERROR) { return -1; }
				if (size == 0) { return 0; }
			}
			auto buffer_size = length_specific != 0 ? length_specific : length;
			buff = new char[buffer_size];
			size = recv(m_SocketID, buff, buffer_size, 0);
		}
		else if (m_DataFormat == 1 && !len)
		{
			size = 65536;
			buff = new char[size];
			size = receivetext(buff, size);
		}
		else if (m_DataFormat == 2 || len > 0)
		{
			buff = new char[len];
			size = recv(m_SocketID, buff, len, 0);
		}
	}
	if (size > 0)
	{
		destination->clear();
		destination->addBuffer(buff, size);
	}
	if (buff != nullptr)
	{
		delete [] buff;
	}
	return size;
}

inline int Socket::peekmessage(int size, SocketBuffer* destination) const
{
	if (m_SocketID < 0 ) return -1;
	if (size == 0) size = 65536;
	auto buff = new char[size];
	size = recvfrom(m_SocketID, buff, size, MSG_PEEK, (SOCKADDR *)&SenderAddr, &SenderAddrSize);
	if (size < 0)
	{
		delete [] buff;
		return -1;
	}
	destination->clear();
	destination->addBuffer(buff, size);
	delete [] buff;
	return size;
}

inline int Socket::lasterror()
{
	return WSAGetLastError();
}

inline std::string Socket::GetHostIP(char* hostname)
{
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_in *h;
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(hostname, "http", &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return "";
	}

	char ipAddress[32];

	// loop through all the results and connect to the first we can
	for (p = servinfo; p != nullptr; p = p->ai_next)
	{
		h = (sockaddr_in *)p->ai_addr;
		inet_ntop(AF_INET, &h->sin_addr, ipAddress, INET_ADDRSTRLEN);
	}

	freeaddrinfo(servinfo); // all done with this structure
	return std::string(ipAddress);
}

inline char* Socket::lastinIP(void)
{
	return (char*)"";
	////return inet_ntoa(SenderAddr.sin_addr);
}

inline unsigned short Socket::lastinPort(void)
{
	return ntohs(SenderAddr.sin_port);
}

inline int Socket::SetFormat(int mode, char* sep)
{
	auto previous = m_DataFormat;
	m_DataFormat = mode;
	if (mode == 1 && strlen(sep)>0) strcpy_s(m_FormatString, 30, sep);
	return previous;
}

inline int Socket::SockExit(void)
{
	WSACleanup();
	return 1;
}

inline int Socket::SockStart(void)
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(1, 1), &wsaData);
	return 1;
}

inline char* Socket::myhost()
{
	static char buf[16];
	gethostname(buf, 16);
	return buf;
}