#pragma once

#include "SingleLinkList.h"
#include "WinsockWrapper.h"
#include "Groundfish.h"

#define SERVER_PORT				2347
#define FILE_PORTION_SIZE		1024
#define MAXIMUM_PACKET_COUNT	10

class Server
{
private:
	struct FileSendTask
	{
		FileSendTask(std::string& fileName, int fileSize, int socketID, std::string ipAddress) :
			FileName(fileName),
			FileSize(fileSize),
			SocketID(socketID),
			IPAddress(ipAddress)
		{
			FilePortionCount = FileSize / FILE_PORTION_SIZE;
			if ((FileSize % FILE_PORTION_SIZE) != 0) FilePortionCount += 1;

			for (int i = 0; i < FilePortionCount; ++i) FilePortionsSent.push_back(i);
			FileStream.open(FileName.c_str(), std::ios_base::binary);
			assert(FileStream.good() && !FileStream.bad());
		}

		bool SendFile()
		{
			if (FilePortionsSent.begin() == FilePortionsSent.end())
			{
				FileStream.close();
				return true;
			}

			winsockWrapper.ClearBuffer(0);
			winsockWrapper.WriteChar(4, 0);

			/*
			//  Encrypt the file name string using Groundfish
			unsigned char encryptedFileName[256];
			int fileNameMessageSize = Groundfish::Encrypt(FileName.c_str(), encryptedFileName, int(FileName.length()) + 1, 0, rand() % 256);

			winsockWrapper.WriteInt(fileNameMessageSize, 0);
			winsockWrapper.WriteChars(encryptedFileName, fileNameMessageSize, 0);
			*/

			//  Choose a portion of the file and commit it to a byte array
			std::list<int>::iterator portionIter = FilePortionsSent.begin();
			FileStream.seekg(FILE_PORTION_SIZE * (*portionIter));
			int portionSize = (((FILE_PORTION_SIZE * (*portionIter)) + FILE_PORTION_SIZE) > FileSize) ? (FileSize - (FILE_PORTION_SIZE * (*portionIter))) : FILE_PORTION_SIZE;
			FileStream.read(PortionArray, portionSize);

			winsockWrapper.WriteInt(FILE_PORTION_SIZE * (*portionIter), 0);
			winsockWrapper.WriteInt(portionSize, 0);
			FilePortionsSent.erase(portionIter);

			/*
			//  Encrypt the file name string using Groundfish
			unsigned char* encryptedPortion = new unsigned char[portionSize + 9];
			int portionMessageSize = Groundfish::Encrypt(PortionArray, encryptedPortion, portionSize, 0, rand() % 256);
			winsockWrapper.WriteInt(portionMessageSize, 0);
			winsockWrapper.WriteChars(encryptedPortion, portionMessageSize, 0);
			delete [] encryptedPortion;
			*/

			winsockWrapper.WriteChars((unsigned char*)PortionArray, portionSize, 0);

			winsockWrapper.WriteInt(int(this), 0);

			winsockWrapper.SendMessagePacket(SocketID, IPAddress.c_str(), SERVER_PORT, 0);

			return false;
		}

		std::string FileName;
		int FileSize;
		int SocketID;
		std::string IPAddress;

		int FilePortionCount;
		std::list<int> FilePortionsSent;
		std::ifstream FileStream;
		char PortionArray[FILE_PORTION_SIZE];
	};

	int						ServerSocket;

	SLList<int>				C_Socket;
	SLList<std::string>		C_IPAddr;
	SLList<char>			C_PingCount;
	std::unordered_map<FileSendTask*, int> FileSendTaskList;

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

	void SendFile(const char* filename, int clientID);
	void SendChatString(const char* String);

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

	//  Send files
	for (std::unordered_map<FileSendTask*, int>::iterator iter = FileSendTaskList.begin(); iter != FileSendTaskList.end(); ++iter)
	{
		if ((*iter).second >= MAXIMUM_PACKET_COUNT) continue;
		if (!(*iter).first->SendFile())
		{
			(*iter).second += 1;
			continue;
		}

		FileSendTaskList.erase(iter);
		break;
	}

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
		case 1: //  Ping return
			C_PingCount[i] = 0;
			break;

		case 2: // Chat message: Decrypt it, then send it out to all users (auto-encrypt)
			{
				int messageSize = winsockWrapper.ReadInt(0);

				//  Decrypt using Groundfish
				unsigned char encrypted[256];
				memcpy(encrypted, winsockWrapper.ReadChars(0, messageSize), messageSize);
				char decrypted[256];
				Groundfish::Decrypt(encrypted, decrypted);

				std::string decryptedString(decrypted);
				if (decryptedString.find("download") != std::string::npos) SendFile("fileToTransfer.jpeg", i);
				else SendChatString(decrypted);
			}
			break;

		case 3: //  Player enters the server, sending their name to be broadcast
			{
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
			break;

		case 4: //  Player confirms receipt of file portion
			{
				int filePointer = winsockWrapper.ReadInt(0);
				if (FileSendTaskList.find((FileSendTask*)filePointer) == FileSendTaskList.end()) return;
				FileSendTaskList[(FileSendTask*)filePointer] -= 1;
			}
			break;
		}

	}
}

////////////////////////////////////////
//	Program Functionality
////////////////////////////////////////

void Server::SendFile(const char* filename, int clientID)
{
	std::ifstream inputFile(filename, std::ios_base::binary);
	assert(inputFile.good() && !inputFile.bad());

	winsockWrapper.ClearBuffer(0);
	winsockWrapper.WriteChar(3, 0);

	//  Encrypt the file name string using Groundfish
	unsigned char encrypted[256];
	int messageSize = Groundfish::Encrypt(filename, encrypted, int(strlen(filename)) + 1, 0, rand() % 256);

	winsockWrapper.WriteInt(messageSize, 0);
	winsockWrapper.WriteChars(encrypted, messageSize, 0);

	int fileSize = int(inputFile.tellg());
	inputFile.seekg(0, std::ios::end);
	fileSize = int(inputFile.tellg()) - fileSize;
	winsockWrapper.WriteInt(fileSize, 0);

	winsockWrapper.SendMessagePacket(C_Socket[clientID], C_IPAddr[clientID].c_str(), SERVER_PORT, 0);

	std::string fileNameString(filename);
	FileSendTask* newTask = new FileSendTask(fileNameString, fileSize, C_Socket[clientID], C_IPAddr[clientID]);
	FileSendTaskList[newTask] = true;
}

void Server::SendChatString(const char* String)
{
	for (int i = 0; i < Client_Count; i += 1)
	{
		winsockWrapper.ClearBuffer(0);
		winsockWrapper.WriteChar(2, 0);

		//  Encrypt the string using Groundfish
		unsigned char encrypted[256];
		int messageSize = Groundfish::Encrypt(String, encrypted, int(strlen(String)) + 1, 0, rand() % 256);

		winsockWrapper.WriteInt(messageSize, 0);
		winsockWrapper.WriteChars(encrypted, messageSize, 0);
		winsockWrapper.SendMessagePacket(C_Socket[i], C_IPAddr[i].c_str(), SERVER_PORT, 0);
	}
}