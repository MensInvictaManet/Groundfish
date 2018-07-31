#pragma once

#include "SingleLinkList.h"
#include "WinsockWrapper.h"
#include "Groundfish.h"

#define SERVER_PORT		2347

class Server
{
private:
	int						ServerSocket;

	SLList<int>				C_Socket;
	SLList<std::string>		C_IPAddr;
	SLList<char>			C_PingCount;

public:
	int						Client_Count;

	Server(void) {}
	~Server(void) {}

	bool Initialize(void);
	bool MainProcess(void);
	void Shutdown(void);

	void AddClient(int SockID, std::string IP);
	void RemoveClient(int ID);
	void AcceptNewClients(void);
	void Messages_Clients(void);

	void SendChatString(char* String);

	inline std::string	GetClientIP(int i) { return C_IPAddr[i]; }
};

bool Server::Initialize()
{
	// Initialize the Listening Port for Clients
	ServerSocket = winsockWrapper.TCPListen(SERVER_PORT, 10, 1);
	if (ServerSocket == -1) return false;
	winsockWrapper.SetNagle(ServerSocket, true);

	Client_Count = 0;

	return true;
}

bool Server::MainProcess(void)
{
	// Get the current running time and step time
	static DWORD	TICK_Current = GetTickCount();
	static DWORD	TICK_Last = GetTickCount();
	static double	TIME_Step = 0.0;
	TICK_Last = TICK_Current;
	TICK_Current = GetTickCount();
	TIME_Step = (((double)TICK_Current - (double)TICK_Last) / 1000.0);

	// Accept Incoming Connections
	AcceptNewClients();

	// Receive messages
	Messages_Clients();

	return true;
}

void Server::Shutdown(void)
{
	winsockWrapper.CloseSocket(ServerSocket);
}


////////////////////////////////////////
//	Client Connection Functions
////////////////////////////////////////

void Server::AddClient(int SockID, std::string IP)
{
	C_Socket.AddHead(SockID);
	C_IPAddr.AddHead(IP);

	Client_Count += 1;
}

void Server::RemoveClient(int ID)
{
	C_Socket.Remove(ID);
	C_IPAddr.Remove(ID);

	Client_Count -= 1;
}

void Server::AcceptNewClients(void)
{
	int NewClient = winsockWrapper.TCPAccept(ServerSocket, 1);
	while (NewClient >= 0)
	{
		AddClient(NewClient, winsockWrapper.GetExteriorIP(NewClient).c_str());

		// Check for another client connection
		NewClient = winsockWrapper.TCPAccept(ServerSocket, 1);
	}
}

void Server::Messages_Clients(void)
{
	for (int i = 0; i < C_Socket.Size(); i += 1)
	{
		int MessageBuffer = winsockWrapper.ReceiveMessagePacket(C_Socket[i], 0, 0);
		if (MessageBuffer == 0)
		{
			RemoveClient(i);
			i -= 1;
			continue;
		}
		if (MessageBuffer < 0) continue;

		char MessageID = winsockWrapper.ReadChar(0);
		switch (MessageID)
		{
		case 1:
			C_PingCount[i] = 0;
			break;

		case 2:
			SendChatString(winsockWrapper.ReadString(0));
			break;

		case 3:
			char NewString[100];
			char* Name = winsockWrapper.ReadString(0);
			char* Extra = (char*)" has entered the server.";
			int S = (int)strlen(Name);
			for (int i = 0; Name[i] != 0; i += 1)		NewString[i] = Name[i];
			for (int i = S; Extra[i - S] != 0; i += 1)	NewString[i] = Extra[i - S];
			S += (int)strlen(Extra);
			NewString[S] = 0;
			SendChatString(NewString);
		}
	}
}

////////////////////////////////////////
//	Program Functionality
////////////////////////////////////////

void Server::SendChatString(char* String)
{
	for (int i = 0; i < Client_Count; i += 1)
	{
		winsockWrapper.ClearBuffer(0);
		winsockWrapper.WriteChar(2, 0);

		//  Encrypt the string using Groundfish
		unsigned char encrypted[256];
		int messageSize = Groundfish::Encrypt(String, encrypted, strlen(String) + 1, 0, rand() % 256);

		winsockWrapper.WriteInt(messageSize, 0);
		winsockWrapper.WriteChars(encrypted, messageSize, 0);
		winsockWrapper.SendMessagePacket(C_Socket[i], C_IPAddr[i].c_str(), SERVER_PORT, 0);
	}
}