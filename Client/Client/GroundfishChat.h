#pragma once

#include "WinsockWrapper.h"

#include <iostream>
using namespace std;
#include <unordered_map>

#define KEY_DOWN(vk_code) ((GetAsyncKeyState(vk_code) & 0x8000) ? 1 : 0)

#define CHAT_SERVER_IP	"98.181.188.165"
#define SERVER_PORT	2347

#define DIALOGUE_LINE_COUNT	21
#define MAX_LINE_LENGTH		75

#define SERVER_IP_LENGTH	128
#define USER_NAME_LENGTH	32

#define CHARACTER_HOLD_TIMER		200
#define CHARACTER_SHORT_HOLD_TIMER	75

enum MessageIDs
{
	MESSAGE_ID_PING_REQUEST					= 1,
	MESSAGE_ID_ENCRYPTED_CHAT_STRING		= 2,
	MESSAGE_ID_FILE_SEND_INITIALIZER		= 3,
	MESSAGE_ID_FILE_PORTION					= 4,
	MESSAGE_ID_FILE_PORTION_COMPLETE		= 5,
};


void SendMessage_PlayerName(std::string playerName, int socket, char* ip)
{
	winsockWrapper.ClearBuffer(0);
	winsockWrapper.WriteChar(3, 0);
	winsockWrapper.WriteString(playerName.c_str(), 0);
	winsockWrapper.SendMessagePacket(socket, ip, SERVER_PORT, 0);
}

void SendMessage_FileRequest(std::string fileName, int socket, const char* ip)
{
	//  Send a "File Request" message
	winsockWrapper.ClearBuffer(0);
	winsockWrapper.WriteChar(4, 0);
	winsockWrapper.WriteString(fileName.c_str(), 0);
	winsockWrapper.SendMessagePacket(socket, ip, SERVER_PORT, 0);
}

void SendMessage_FileReceiveReady(std::string fileName, int socket, const char* ip)
{
	//  Send a "File Receive Ready" message
	winsockWrapper.ClearBuffer(0);
	winsockWrapper.WriteChar(5, 0);
	//  TODO: Encrypt the file name
	winsockWrapper.WriteString(fileName.c_str(), 0);
	winsockWrapper.SendMessagePacket(socket, ip, SERVER_PORT, 0);
}

void SendMessage_FilePortionCompleteConfirmation(int portionIndex, int socket, const char* ip)
{
	//  Send a "File Portion Complete" message
	winsockWrapper.ClearBuffer(0);
	winsockWrapper.WriteChar(6, 0);
	winsockWrapper.WriteInt(portionIndex, 0);
	winsockWrapper.SendMessagePacket(socket, ip, SERVER_PORT, 0);
}

void SendMessage_FileChunksRemaining(std::unordered_map<int, bool>& chunksRemaining, int socket, const char* ip)
{
	//  Send a "File Portion Complete" message
	winsockWrapper.ClearBuffer(0);
	winsockWrapper.WriteChar(7, 0);
	winsockWrapper.WriteInt(chunksRemaining.size(), 0);
	for (auto i = chunksRemaining.begin(); i != chunksRemaining.end(); ++i) winsockWrapper.WriteShort((short)((*i).first), 0);
	winsockWrapper.SendMessagePacket(socket, ip, SERVER_PORT, 0);
}

class Chat
{
private:
	struct FileReceiveTask
	{
		FileReceiveTask(const char* fileName, int fileSize, int fileChunkSize, int fileChunkBufferSize, int socketID, std::string ipAddress) :
			FileName(fileName),
			FileSize(fileSize),
			FileChunkSize(fileChunkSize),
			FileChunkBufferSize(fileChunkBufferSize),
			SocketID(socketID),
			IPAddress(ipAddress),
			FilePortionIndex(0),
			FileDownloadComplete(false)
		{
			FileChunkCount = FileSize / FileChunkSize;
			if ((FileSize % FileChunkSize) != 0) FileChunkCount += 1;
			auto nextChunkCount = (FileChunkCount > (FileChunkBufferSize)) ? FileChunkBufferSize : FileChunkCount;
			ResetChunksToReceiveMap(nextChunkCount);
			FilePortionCount = FileChunkCount / FileChunkBufferSize;
			if ((FileChunkCount % FileChunkBufferSize) != 0) FilePortionCount += 1;

			//  Create a file of the proper size based on the server's description
			std::ofstream outputFile(fileName, std::ios::binary | std::ios::trunc | std::ios_base::beg);
			outputFile.seekp(fileSize - 1);
			outputFile.write("", 1);
			outputFile.close();

			//  Open the file again, this time keeping the file handle open for later writing
			FileStream.open(FileName.c_str(), std::ios_base::binary | std::ios_base::out | std::ios_base::in);
			assert(FileStream.good() && !FileStream.bad());

			FileReceiveBuffer = new char[FileChunkSize];
			SendMessage_FileReceiveReady(FileName, SocketID, IPAddress.c_str());
		}

		~FileReceiveTask()
		{
			delete[] FileReceiveBuffer;
		}

		inline void ResetChunksToReceiveMap(int chunkCount) { FilePortionsToReceive.clear(); for (auto i = 0; i < chunkCount; ++i)  FilePortionsToReceive[i] = true; }
		

		bool ReceiveFile(std::string& progress)
		{
			auto filePortionIndex = winsockWrapper.ReadInt(0);
			auto chunkIndex = winsockWrapper.ReadInt(0);
			auto chunkSize = winsockWrapper.ReadInt(0);
			unsigned char* chunkData = winsockWrapper.ReadChars(0, chunkSize);

			if (filePortionIndex != FilePortionIndex)
			{
				return false;
			}

			if (FileDownloadComplete) return true;

			auto iter = FilePortionsToReceive.find(chunkIndex);
			if (iter == FilePortionsToReceive.end()) return false;

			auto chunkPosition = (filePortionIndex * FileChunkSize * FileChunkBufferSize) + (chunkIndex * FileChunkSize) + 0;
			FileStream.seekp(chunkPosition);
			FileStream.write((char*)chunkData, chunkSize);

			progress = "Downloaded Portion [" + std::to_string(chunkPosition) + " to " + std::to_string(chunkPosition + chunkSize - 1) + "]";
			assert(iter != FilePortionsToReceive.end());
			FilePortionsToReceive.erase(iter);

			return false;
		}

		bool CheckFilePortionComplete(int portionIndex)
		{
			if (portionIndex != FilePortionIndex) return false;

			if (FilePortionsToReceive.size() != 0)
			{
				SendMessage_FileChunksRemaining(FilePortionsToReceive, SocketID, IPAddress.c_str());
				return false;
			}
			else
			{
				SendMessage_FilePortionCompleteConfirmation(FilePortionIndex, SocketID, IPAddress.c_str());
				if (++FilePortionIndex == FilePortionCount)
				{
					FileDownloadComplete = true;
					FileStream.close();
				}

				//  Reset the chunk list to ensure we're waiting on the right number of chunks for the next portion
				auto chunksProcessed = FilePortionIndex * FileChunkBufferSize;
				auto nextChunkCount = (FileChunkCount > (chunksProcessed + FileChunkBufferSize)) ? FileChunkBufferSize : (FileChunkCount - chunksProcessed);
				ResetChunksToReceiveMap(nextChunkCount);

				return true;
			}
		}

		inline bool GetFileDownloadComplete() const { return FileDownloadComplete; }
		inline float GetPercentageComplete() const { return float(FilePortionIndex) / float(FilePortionCount) * 100.0f; }

		std::string FileName;
		int FileSize;
		int SocketID;
		std::string IPAddress;
		int FilePortionCount;
		int FileChunkCount;
		const int FileChunkSize;
		const int FileChunkBufferSize;
		std::ofstream FileStream;

		int FilePortionIndex;
		char* FileReceiveBuffer;
		std::unordered_map<int, bool> FilePortionsToReceive;
		bool FileDownloadComplete;
	};

	int			ServerSocket;
	char		ServerIP[SERVER_IP_LENGTH];

	char		Name[USER_NAME_LENGTH];
	char		Dialogue[DIALOGUE_LINE_COUNT][MAX_LINE_LENGTH + 1];
	bool		NoDrawNeeded[2];
	char		InputLine[MAX_LINE_LENGTH];

	std::unordered_map<int, long> NoRepeat;
	FileReceiveTask* fileReceiveTask = NULL;

public:
	Chat(void) {}
	~Chat(void) {}

	bool Initialize();
	bool MainProcess(long ticks);
	void Shutdown(void);

	bool ReadMessages(void);
	void NewLine(const char* NewString);
	void DisplayLines(void);
	char GetVKStateCharacter(void);
	void CheckInput(long ticks);
	void SendChatString(char* String);

	void DrawOutline(void);
};

bool Chat::Initialize()
{
	strcpy_s(ServerIP, SERVER_IP_LENGTH, CHAT_SERVER_IP);

	// Connect to the given server
	ServerSocket = winsockWrapper.TCPConnect(ServerIP, SERVER_PORT, 1);
	if (ServerSocket < 0) return false;

	system("cls");
	cout << " Please enter a name for yourself (<16 characters): ";
	cin.get(Name, USER_NAME_LENGTH);
	cin.get();

	if (strcmp(Name, "") == 0) strcpy_s(Name, strlen("Guest") + 1, "Guest");
	system("cls");

	// Send name to the server
	SendMessage_PlayerName(std::string(Name), ServerSocket, ServerIP);

	//  Send file request to the server
	SendMessage_FileRequest("fileToTransfer.jpeg", ServerSocket, ServerIP);

	// Clear the dialogue
	for (int i = 0; i < DIALOGUE_LINE_COUNT; i += 1) Dialogue[i][0] = 0;
	for (int i = 0; i < MAX_LINE_LENGTH; i += 1) InputLine[i] = 0;
	NoDrawNeeded[0] = false;

	DrawOutline();

	return true;
}

bool Chat::MainProcess(long ticks)
{
	ReadMessages();
	DisplayLines();
	CheckInput(ticks);
	return true;
}

void Chat::Shutdown(void)
{
	closesocket(ServerSocket);
}

bool Chat::ReadMessages(void)
{
	int MessageBuffer = winsockWrapper.ReceiveMessagePacket(ServerSocket, 0, 0);
	if (MessageBuffer == 0) return false;
	if (MessageBuffer < 0) return true;

	char MessageID = winsockWrapper.ReadChar(0);

	switch (MessageID)
	{
		case MESSAGE_ID_PING_REQUEST:
			{
				winsockWrapper.ClearBuffer(0);
				winsockWrapper.WriteChar(1, 0);
				winsockWrapper.SendMessagePacket(ServerSocket, ServerIP, SERVER_PORT, 0);
			}
			break;

		case MESSAGE_ID_ENCRYPTED_CHAT_STRING:
			{
				int messageSize = winsockWrapper.ReadInt(0);

				//  Decrypt using Groundfish and post to the chat
				unsigned char encrypted[256];
				memcpy(encrypted, winsockWrapper.ReadChars(0, messageSize), messageSize);
				char decrypted[256];
				Groundfish::Decrypt(encrypted, decrypted);
				NewLine(decrypted);
			}
			break;


		case MESSAGE_ID_FILE_SEND_INITIALIZER:
			{
				int fileNameSize = winsockWrapper.ReadInt(0);

				//  Decrypt using Groundfish and save as the filename
				unsigned char encryptedFileName[256];
				memcpy(encryptedFileName, winsockWrapper.ReadChars(0, fileNameSize), fileNameSize);
				char decryptedFileName[256];
				Groundfish::Decrypt(encryptedFileName, decryptedFileName);

				//  Grab the file size, file chunk size, and buffer count
				auto fileSize = winsockWrapper.ReadInt(0);
				auto fileChunkSize = winsockWrapper.ReadInt(0);
				auto FileChunkBufferSize = winsockWrapper.ReadInt(0);

				//  Create a new file receive task
				fileReceiveTask = new FileReceiveTask(decryptedFileName, fileSize, fileChunkSize, FileChunkBufferSize, 0, CHAT_SERVER_IP);

				//  Output the file name
				std::string newString = "Downloading file: " + std::string(decryptedFileName) + " (size: " + std::to_string(fileSize) + ")";
				NewLine(newString.c_str());
			}
			break;

		case MESSAGE_ID_FILE_PORTION:
			{
				if (fileReceiveTask == NULL) break;

				std::string progressString = "ERROR";
				if (fileReceiveTask->ReceiveFile(progressString))
				{
					//  If ReceiveFile returns true, the transfer is complete
					delete fileReceiveTask;
					fileReceiveTask = NULL;
				}
				//NewLine(progressString.c_str());
			}
			break;

		case MESSAGE_ID_FILE_PORTION_COMPLETE:
			{
				auto portionIndex = winsockWrapper.ReadInt(0);
				if (fileReceiveTask == nullptr) break;
				if (fileReceiveTask->CheckFilePortionComplete(portionIndex))
				{
					auto percentComplete = "File Portion Complete: (" + std::to_string(fileReceiveTask->GetPercentageComplete()) + "%)";
					NewLine(percentComplete.c_str());

					if (fileReceiveTask->GetFileDownloadComplete())
					{
						delete fileReceiveTask;
						fileReceiveTask = nullptr;
						break;
					}
				}
			}
			break;
	}

	return true;
}

void Chat::NewLine(const char* NewString)
{
	int stringLength = (int)strlen(NewString);
	int linesAdded = stringLength / MAX_LINE_LENGTH + 1;
	if (((linesAdded - 1) * MAX_LINE_LENGTH) == stringLength) linesAdded -= 1;

	// Move all existing lines upward the number of line being added
	for (int i = linesAdded; i < DIALOGUE_LINE_COUNT; i += 1)
	{
		memmove(Dialogue[i - linesAdded], Dialogue[i], MAX_LINE_LENGTH);
		Dialogue[i - linesAdded][MAX_LINE_LENGTH] = 0;
	}

	// Add the new lines
	for (int i = 0; i < linesAdded; i += 1)
	{
		memset(Dialogue[DIALOGUE_LINE_COUNT - 1 - i], 0, MAX_LINE_LENGTH);
		memmove(Dialogue[DIALOGUE_LINE_COUNT - 1 - i], &NewString[(linesAdded - 1 - i) * MAX_LINE_LENGTH], MAX_LINE_LENGTH);
		Dialogue[DIALOGUE_LINE_COUNT - 1 - i][MAX_LINE_LENGTH] = 0;
	}

	NoDrawNeeded[0] = false;
}

void Chat::DisplayLines(void)
{
	if (!NoDrawNeeded[0])
	{
		system("cls");
		DrawOutline();

		COORD C;
		for (int i = 0; i < DIALOGUE_LINE_COUNT; i += 1)
		{
			C.X = 2;	C.Y = 1 + i;	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), C);
			for (int j = 0; Dialogue[i][j] != 0; j += 1)
			{
				cout << Dialogue[i][j];
			}
		}

		NoDrawNeeded[0] = true;
	}
	if (!NoDrawNeeded[1])
	{
		COORD C;
		int i;

		// Draw the client's input string
		C.X = 2;	C.Y = 23;	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), C);
		for (i = 0; i < MAX_LINE_LENGTH; i += 1) cout << " ";

		C.X = 2;	C.Y = 23;	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), C);
		for (i = 0; InputLine[i] != 0; i += 1) cout << InputLine[i];


		C.X = 2 + i;	C.Y = 23;	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), C);

		NoDrawNeeded[1] = true;
	}
}

char Chat::GetVKStateCharacter()
{
	//  Check for RETURN and BACK
	if (KEY_DOWN(VK_RETURN))	return VK_RETURN;
	if (KEY_DOWN(VK_BACK))		return VK_BACK;

	//  Check for (uppercase) letters
	for (int i = 'A'; i <= 'Z'; ++i) if (KEY_DOWN(i)) return i;

	//  Check for numbers
	for (int i = '0'; i <= '9'; ++i) if (KEY_DOWN(i)) return i;

	//  Check for other characters
	if (KEY_DOWN(VK_SPACE)) return ' ';
	if (KEY_DOWN(VK_OEM_PERIOD)) return '.';
	if (KEY_DOWN(VK_OEM_COMMA)) return ',';
	if (KEY_DOWN(VK_OEM_MINUS)) return '-';
	if (KEY_DOWN(VK_OEM_PLUS)) return '+';

	return 0;
}

void Chat::CheckInput(long ticks)
{
	if (GetConsoleWindow() != GetForegroundWindow()) return;

	for (std::unordered_map<int, long>::iterator iter = NoRepeat.begin(); iter != NoRepeat.end(); ++iter)
		(*iter).second = ((*iter).second < ticks) ? 0 : ((*iter).second - ticks);

	// If the client detects a character, save it off to use in the string or as a command
	int PressedKey = GetVKStateCharacter();
	int PressedKeyUpper = PressedKey;
	if (PressedKey == 0) return;
	if ((NoRepeat.find(PressedKey) != NoRepeat.end()) && (NoRepeat[PressedKey] != 0)) return;

	// If the client detects the ENTER/RETURN key, send the current line
	if (PressedKey == VK_RETURN)
	{
		SendChatString(InputLine);
		InputLine[0] = 0;
		NoRepeat[PressedKey] = CHARACTER_SHORT_HOLD_TIMER;
		NoDrawNeeded[1] = false;
		return;
	}

	// If the client detects the BACKSPACE key, go back one character
	if (PressedKey == VK_BACK)
	{
		int i = 0;
		for (i = 0; InputLine[i] != 0; i += 1) {}
		if (i != 0) InputLine[i - 1] = 0;
		NoDrawNeeded[1] = false;
		NoRepeat[PressedKey] = CHARACTER_SHORT_HOLD_TIMER;
		return;
	}

	//  If we're checking a letter, check for shift and take uppercase situations into mind
	if ((PressedKey >= 'A') && (PressedKey <= 'Z'))
	{
		PressedKeyUpper = PressedKey;
		PressedKey += ((KEY_DOWN(VK_SHIFT)) ? 0 : ('a' - 'A'));
		if ((NoRepeat.find(PressedKey) != NoRepeat.end()) && (NoRepeat[PressedKey] != 0)) return;
	}

	for (std::unordered_map<int, long>::iterator iter = NoRepeat.begin(); iter != NoRepeat.end(); ++iter)
	{
		if ((*iter).first == PressedKey) continue;
		if ((*iter).first == PressedKeyUpper) continue;
		(*iter).second = 0;
	}

	unsigned int i;
	for (i = 0; ((InputLine[i] != 0) && (i < MAX_LINE_LENGTH - strlen(Name) - 2)); ++i) {}
	if (i < (MAX_LINE_LENGTH - strlen(Name) - 2))
	{
		InputLine[i] = PressedKey;
		InputLine[i + 1] = 0;
		NoRepeat[PressedKey] = CHARACTER_HOLD_TIMER;
		NoDrawNeeded[1] = false;
	}
}

void Chat::DrawOutline(void)
{
	COORD C;
	C.X = 0;	C.Y = 0;	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), C);
	for (int i = 0; i < 40; i += 1) cout << "-" << std::endl;
	for (int i = 1; i < DIALOGUE_LINE_COUNT + 1; i += 1)
	{
		C.X = 0;	C.Y = i;	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), C);
		cout << "|";
		C.X = 79;	C.Y = i;	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), C);
		cout << "|";
	}
	for (int i = 0; i < 40; i += 1) cout << "-";
	C.X = 0;	C.Y = 0;	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), C);
}

void Chat::SendChatString(char* String)
{
	if (strlen(String) == 0) return;

	winsockWrapper.ClearBuffer(0);
	winsockWrapper.WriteChar(2, 0);

	//  Form the full string with the users name
	std::string sendString;
	sendString += Name;
	sendString += ": ";
	sendString += String;

	//  Encrypt the string using Groundfish
	unsigned char encrypted[256];
	int messageSize = Groundfish::Encrypt(sendString.c_str(), encrypted, int(sendString.length()) + 1, 0, rand() % 256);

	winsockWrapper.WriteInt(messageSize, 0);
	winsockWrapper.WriteChars(encrypted, messageSize, 0);
	winsockWrapper.SendMessagePacket(ServerSocket, ServerIP, SERVER_PORT, 0);
}