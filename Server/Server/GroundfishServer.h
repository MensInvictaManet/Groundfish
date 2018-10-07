#pragma once

#include "WinsockWrapper.h"
#include "Groundfish.h"

#include <ctime>

#define SERVER_PORT						2347
#define FILE_CHUNK_SIZE					1024
#define FILE_CHUNK_BUFFER_COUNT			500
#define FILE_SEND_BUFFER_SIZE			(FILE_CHUNK_SIZE * FILE_CHUNK_BUFFER_COUNT)
#define MAXIMUM_PACKET_COUNT			10
#define PORTION_COMPLETE_REMIND_TIME	0.5

enum MessageIDs
{
	MESSAGE_ID_PING_RETURN						= 1,
	MESSAGE_ID_ENCRYPTED_CHAT_STRING			= 2,
	MESSAGE_ID_UNENCRYPTED_USER_NAME			= 3,
	MESSAGE_ID_FILE_REQUEST						= 4,
	MESSAGE_ID_FILE_RECEIVE_READY				= 5,
	MESSAGE_ID_FILE_PORTION_COMPLETE_CONFIRM	= 6,
	MESSAGE_ID_FILE_CHUNKS_REMAINING			= 7,
};

void SendMessage_FileSendInitializer(std::string fileName, int fileSize, int socket, const char* ip)
{
	//  Encrypt the file name string using Groundfish
	unsigned char encrypted[256];
	int messageSize = Groundfish::Encrypt(fileName.c_str(), encrypted, int(fileName.length()) + 1, 0, rand() % 256);

	winsockWrapper.ClearBuffer(0);
	winsockWrapper.WriteChar(3, 0);
	winsockWrapper.WriteInt(messageSize, 0);
	winsockWrapper.WriteChars(encrypted, messageSize, 0);
	winsockWrapper.WriteInt(fileSize, 0);
	winsockWrapper.WriteInt(FILE_CHUNK_SIZE, 0);
	winsockWrapper.WriteInt(FILE_CHUNK_BUFFER_COUNT, 0);

	winsockWrapper.SendMessagePacket(socket, ip, SERVER_PORT, 0);
}

void SendMessage_FileSendChunk(int chunkBufferIndex, int chunkIndex, int chunkSize, unsigned char* buffer, int socket, const char* ip)
{
	winsockWrapper.ClearBuffer(0);
	winsockWrapper.WriteChar(4, 0);
	winsockWrapper.WriteInt(chunkBufferIndex, 0);
	winsockWrapper.WriteInt(chunkIndex, 0);
	winsockWrapper.WriteInt(chunkSize, 0);
	winsockWrapper.WriteChars(buffer, chunkSize, 0);
	winsockWrapper.SendMessagePacket(socket, ip, SERVER_PORT, 0);
}

void SendMessage_FileTransferPortionComplete(int portionIndex, int socket, const char* ip)
{
	winsockWrapper.ClearBuffer(0);
	winsockWrapper.WriteChar(5, 0);
	winsockWrapper.WriteInt(portionIndex, 0);
	winsockWrapper.SendMessagePacket(socket, ip, SERVER_PORT, 0);
}


struct FileSendTask
{
	FileSendTask(std::string fileName, int socketID, std::string ipAddress) :
		FileName(fileName),
		SocketID(socketID),
		IPAddress(ipAddress),
		FileTransferReady(false),
		FileChunkTransferState(CHUNK_STATE_INITIALIZING),
		FilePortionIndex(0),
		LastMessageTime(clock())
	{
		//  Open the file, determine that the file handler is good or not, then save off the chunk send indicator map
		FileStream.open(FileName.c_str(), std::ios_base::binary);
		assert(FileStream.good() && !FileStream.bad());

		//  Get the file size by reading the beginning and end memory positions
		FileSize = int(FileStream.tellg());
		FileStream.seekg(0, std::ios::end);
		FileSize = int(FileStream.tellg()) - FileSize;

		//  Determine the file portion count
		FileChunkCount = FileSize / FILE_CHUNK_SIZE;
		if ((FileSize % FILE_CHUNK_SIZE) != 0) FileChunkCount += 1;

		FilePortionCount = FileChunkCount / FILE_CHUNK_BUFFER_COUNT;
		if ((FileChunkCount % FILE_CHUNK_BUFFER_COUNT) != 0) FilePortionCount += 1;

		BufferFilePortions(FilePortionIndex, FILE_CHUNK_BUFFER_COUNT);

		//  Send a "File Send Initializer" message
		SendMessage_FileSendInitializer(FileName, FileSize, SocketID, IPAddress.c_str());
	}

	~FileSendTask()
	{
		FileStream.close();
	}

	void BufferFilePortions(int chunkBufferIndex, int portionCount)
	{
		auto portionPosition = chunkBufferIndex * FILE_SEND_BUFFER_SIZE;
		auto bytesBuffering = ((portionPosition + FILE_SEND_BUFFER_SIZE) > FileSize) ? (FileSize - portionPosition) : FILE_SEND_BUFFER_SIZE;
		auto bufferCount = (((bytesBuffering % FILE_CHUNK_SIZE) == 0) ? (bytesBuffering / FILE_CHUNK_SIZE) : ((bytesBuffering / FILE_CHUNK_SIZE) + 1));

		//  Empty the entire FilePortionBuffer
		memset(FilePortionBuffer, NULL, FILE_SEND_BUFFER_SIZE);

		//  Determine the amount of data we're actually buffering, if the file would end before reading the entire size

		//  Go through each chunk of the file and place it in the file buffer until we either fill the buffer or run out of file
		FileChunksToSend.clear();
		for (auto i = 0; i < bufferCount; ++i)
		{
			auto chunkPosition = portionPosition + (i * FILE_CHUNK_SIZE);
			FileStream.seekg(chunkPosition);

			auto chunkFill = (((chunkPosition + FILE_CHUNK_SIZE) > FileSize) ? (FileSize - chunkPosition) : FILE_CHUNK_SIZE);
			FileStream.read(FilePortionBuffer[i], chunkFill);

			FileChunksToSend[i] = true;
		}
	}

	bool SendFileChunk()
	{
		//  If we're pending completion, wait and send completion confirmation only if we've reached the end
		if (FileChunkTransferState == CHUNK_STATE_PENDING_COMPLETE)
		{
			auto seconds = double(clock() - LastMessageTime) / CLOCKS_PER_SEC;
			if (seconds > PORTION_COMPLETE_REMIND_TIME)
			{
				SendMessage_FileTransferPortionComplete(FilePortionIndex, SocketID, IPAddress.c_str());
				LastMessageTime = clock();
				return false;
			}
			return (FilePortionIndex >= FilePortionCount);
		}

		//  First, check that we have data left unsent in the current chunk buffer. If not, set us to "Pending Complete" on the current chunk buffer
		if (FileChunksToSend.begin() == FileChunksToSend.end())
		{
			FileChunkTransferState = CHUNK_STATE_PENDING_COMPLETE;
			SendMessage_FileTransferPortionComplete(FilePortionIndex, SocketID, IPAddress.c_str());
			LastMessageTime = clock();
			return false;
		}


		//  Choose a portion of the file and commit it to a byte array
		auto chunkIndex = (*FileChunksToSend.begin()).first;
		auto chunkPosition = (FilePortionIndex * FILE_SEND_BUFFER_SIZE) + (FILE_CHUNK_SIZE * chunkIndex);
		int chunkSize = ((chunkPosition + FILE_CHUNK_SIZE) > FileSize) ? (FileSize - chunkPosition) : FILE_CHUNK_SIZE;

		//  Write the chunk buffer index, the index of the chunk, the size of the chunk, and then the chunk data
		SendMessage_FileSendChunk(FilePortionIndex, chunkIndex, chunkSize, (unsigned char*)FilePortionBuffer[chunkIndex], SocketID, IPAddress.c_str());

		//  Delete the portionIter to signal we've completed sending it
		FileChunksToSend.erase(chunkIndex);

		return false;
	}

	void SetChunksRemaining(std::unordered_map<int, bool>& chunksRemaining)
	{
		FileChunksToSend.clear();
		for (auto i = chunksRemaining.begin(); i != chunksRemaining.end(); ++i) FileChunksToSend[(*i).first] = true;
		FileChunkTransferState = CHUNK_STATE_SENDING;
	}

	void FilePortionComplete(int portionIndex)
	{
		//  If this is just a duplicate message we're receiving, ignore it
		if (FilePortionIndex > portionIndex) return;

		//  If we've reached the end of the file, don't attempt to buffer anything, and leave the state PENDING COMPLETE
		if (++FilePortionIndex >= FilePortionCount)  return;

		BufferFilePortions(FilePortionIndex, FILE_CHUNK_BUFFER_COUNT);
	}

	inline bool GetFileTransferReady() const { return FileTransferReady; }
	inline void SetFileTransferReady(bool ready) { FileTransferReady = ready; }
	inline int GetFileTransferState() const { return FileChunkTransferState; }
	inline void SetFileTransferState(int state) { FileChunkTransferState = (FileChunkSendState)state; }

	std::string FileName;
	int FileSize;
	int SocketID;
	std::string IPAddress;
	std::ifstream FileStream;

	enum FileChunkSendState
	{
		CHUNK_STATE_INITIALIZING = 0,
		CHUNK_STATE_SENDING = 1,
		CHUNK_STATE_PENDING_COMPLETE = 2,
		CHUNK_STATE_COMPLETE = 3,
	};

	int FilePortionIndex;
	bool FileTransferReady;
	FileChunkSendState FileChunkTransferState;
	int FilePortionCount;
	int FileChunkCount;
	std::unordered_map<int, bool> FileChunksToSend;
	char FilePortionBuffer[FILE_CHUNK_BUFFER_COUNT][FILE_CHUNK_SIZE];
	clock_t LastMessageTime;
};

class Server
{
private:
	int						ServerSocketHandle;

	struct UserConnection
	{
		int				SocketID;
		std::string		IPAddress;
		int				PingCount;
	};

	std::unordered_map<UserConnection*, bool> UserConnectionsList;
	std::unordered_map<UserConnection*, FileSendTask*> FileSendTaskList; // NOTE: Key is the client ID, so we should limit them to one transfer in the future

public:
	Server(void) {}
	~Server(void) {}

	bool Initialize(void);
	void MainProcess(void);
	void Shutdown(void);

	void AddClient(int socketID, std::string ipAddress);
	void RemoveClient(UserConnection* user);
	void AcceptNewClients(void);
	void ReceiveMessages(void);

	void ContinueFileTransfers(void);
	void BeginFileTransfer(const char* filename, UserConnection* user);
	void SendChatString(const char* String);

	inline const std::unordered_map<UserConnection*, bool> GetUserList(void) const { return UserConnectionsList; }
	inline std::string	GetClientIP(UserConnection* user) { return user->IPAddress; }
};

bool Server::Initialize()
{
	// Initialize the Listening Port for Clients
	ServerSocketHandle = winsockWrapper.TCPListen(SERVER_PORT, 10, 1);
	if (ServerSocketHandle == -1) return false;
	winsockWrapper.SetNagle(ServerSocketHandle, true);

	return true;
}

void Server::MainProcess(void)
{
	// Accept Incoming Connections
	AcceptNewClients();

	// Receive messages
	ReceiveMessages();

	//  Send files
	ContinueFileTransfers();
}

void Server::Shutdown(void)
{
	winsockWrapper.CloseSocket(ServerSocketHandle);
}


////////////////////////////////////////
//	Client Connection Functions
////////////////////////////////////////

void Server::AddClient(int socketID, std::string ipAddress)
{
	auto newConnection = new UserConnection;
	newConnection->SocketID = socketID;
	newConnection->IPAddress = ipAddress;
	newConnection->PingCount = 0;
	UserConnectionsList[newConnection] = true;
}

void Server::RemoveClient(UserConnection* user)
{
	auto userIter = UserConnectionsList.find(user);
	assert(userIter != UserConnectionsList.end());

	delete user;
	UserConnectionsList.erase(userIter);
}

void Server::AcceptNewClients(void)
{
	int NewClient = winsockWrapper.TCPAccept(ServerSocketHandle, 1);
	while (NewClient >= 0)
	{
		AddClient(NewClient, winsockWrapper.GetExteriorIP(NewClient));

		// Check for another client connection
		NewClient = winsockWrapper.TCPAccept(ServerSocketHandle, 1);
	}
}

void Server::ReceiveMessages(void)
{
	for (auto userIter = UserConnectionsList.begin(); userIter != UserConnectionsList.end(); ++userIter)
	{
		auto user = (*userIter).first;
		auto messageBufferSize = winsockWrapper.ReceiveMessagePacket(user->SocketID, 0, 0);

		//  If no message has arrived, move on to the next connection
		if (messageBufferSize < 0) continue;

		//  If we receive a disconnection indicator, remove the user and break out to avoid continuation issues
		if (messageBufferSize == 0)
		{
			RemoveClient(user);
			break;
		}

		char messageID = winsockWrapper.ReadChar(0);
		switch (messageID)
		{
		case MESSAGE_ID_PING_RETURN:
		{
			//  NO DATA

			user->PingCount = 0;
		}
		break;

		case MESSAGE_ID_ENCRYPTED_CHAT_STRING:
		{
			//  (int) Message Size [N]
			//  (N-sized char array) Encrypted String

			int messageSize = winsockWrapper.ReadInt(0);

			//  Decrypt using Groundfish
			unsigned char encrypted[256];
			memcpy(encrypted, winsockWrapper.ReadChars(0, messageSize), messageSize);
			char decrypted[256];
			Groundfish::Decrypt(encrypted, decrypted);

			std::string decryptedString(decrypted);
			if (decryptedString.find("download") != std::string::npos) BeginFileTransfer("fileToTransfer.jpeg", user);
			else SendChatString(decrypted);
		}
		break;

		case MESSAGE_ID_UNENCRYPTED_USER_NAME: // Player enters the server, sending their name to be broadcast
		{
			//  (?-size string) Unencrypted Player Name

			char NewString[100];
			char* Name = winsockWrapper.ReadString(0);
			char* Extra = (char*)" has entered the server.";
			int S = (int)strlen(Name);
			for (auto j = 0; Name[j] != 0; ++j)		NewString[j] = Name[j];
			for (auto j = S; Extra[j - S] != 0; ++j)	NewString[j] = Extra[j - S];
			S += (int)strlen(Extra);
			NewString[S] = 0;
			SendChatString(NewString);
		}
		break;

		case MESSAGE_ID_FILE_REQUEST:
		{
			char* fileName = winsockWrapper.ReadString(0);

			BeginFileTransfer(fileName, user);
		}
		break;

		case MESSAGE_ID_FILE_RECEIVE_READY:
		{
			char* fileName = winsockWrapper.ReadString(0);
			auto taskIter = FileSendTaskList.find(user);
			assert(taskIter != FileSendTaskList.end());

			auto task = (*taskIter).second;
			task->SetFileTransferReady(true);
			task->SetFileTransferState(FileSendTask::CHUNK_STATE_SENDING);
		}
		break;

		case MESSAGE_ID_FILE_PORTION_COMPLETE_CONFIRM:
		{
			auto taskIter = FileSendTaskList.find(user);
			assert(taskIter != FileSendTaskList.end());

			auto task = (*taskIter).second;
			assert(task->GetFileTransferState() == FileSendTask::CHUNK_STATE_PENDING_COMPLETE);

			auto portionIndex = winsockWrapper.ReadInt(0);
			task->FilePortionComplete(portionIndex);
		}
		break;

		case MESSAGE_ID_FILE_CHUNKS_REMAINING:
		{
			auto taskIter = FileSendTaskList.find(user);
			assert(taskIter != FileSendTaskList.end());

			auto task = (*taskIter).second;
			auto chunkCount = winsockWrapper.ReadInt(0);
			std::unordered_map<int, bool> chunksRemaining;
			for (auto i = 0; i < chunkCount; ++i) chunksRemaining[winsockWrapper.ReadShort(0)] = true;
			task->SetChunksRemaining(chunksRemaining);
		}
		break;
		}
	}
}

////////////////////////////////////////
//	Program Functionality
////////////////////////////////////////
void Server::ContinueFileTransfers(void)
{
	//  Find all file transfers and send a file chunk for each one if the file transfer is ready
	for (auto taskIter = FileSendTaskList.begin(); taskIter != FileSendTaskList.end(); ++taskIter)
	{
		auto task = (*taskIter).second;

		//  If we aren't ready to send the file, continue out and wait for a ready signal
		if (task->GetFileTransferReady() == false) continue;

		//  If we have data to send, send it and continue out so we can keep sending it until we're done
		if (!task->SendFileChunk()) continue;

		//  If we've gotten this far, we're ready to delete the file send task, as it completed.
		delete task;
		FileSendTaskList.erase(taskIter);
		break;
	}
}


void Server::BeginFileTransfer(const char* filename, UserConnection* user)
{
	//  Add a new FileSendTask to our list, so it can manage itself
	FileSendTask* newTask = new FileSendTask(std::string(filename), user->SocketID, std::string(user->IPAddress));
	FileSendTaskList[user] = newTask;
}

void Server::SendChatString(const char* String)
{
	for (auto userIter = UserConnectionsList.begin(); userIter != UserConnectionsList.end(); ++userIter)
	{
		auto user = (*userIter).first;

		winsockWrapper.ClearBuffer(0);
		winsockWrapper.WriteChar(2, 0);

		//  Encrypt the string using Groundfish
		unsigned char encrypted[256];
		int messageSize = Groundfish::Encrypt(String, encrypted, int(strlen(String)) + 1, 0, rand() % 256);

		winsockWrapper.WriteInt(messageSize, 0);
		winsockWrapper.WriteChars(encrypted, messageSize, 0);
		winsockWrapper.SendMessagePacket(user->SocketID, user->IPAddress.c_str(), SERVER_PORT, 0);
	}
}