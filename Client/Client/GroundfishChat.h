#pragma once

#include "WinsockWrapper.h"

#include <iostream>
using namespace std;

#define KEY_DOWN(vk_code) ((GetAsyncKeyState(vk_code) & 0x8000) ? 1 : 0)

#define SERVER_PORT	2347

#define DIALOGUE_LINES		21
#define MAX_LINE_LENGTH		75

#define SERVER_IP_LENGTH	128
#define USER_NAME_LENGTH	32

#define CHARACTER_HOLD_TIMER		150
#define CHARACTER_SHORT_HOLD_TIMER	75

class Chat
{
private:
	int			ServerSocket;
	char		ServerIP[SERVER_IP_LENGTH];

	char		Name[USER_NAME_LENGTH];
	char		Dialogue[DIALOGUE_LINES][MAX_LINE_LENGTH + 1];
	bool		NoDrawNeeded[2];
	char		InputLine[MAX_LINE_LENGTH];

	std::unordered_map<int, long> NoRepeat;

public:
	Chat(void) {}
	~Chat(void) {}

	bool Initialize(const char* IP);
	bool MainProcess(long ticks);
	void Shutdown(void);

	bool ReadMessages(void);
	void NewLine(char* NewString);
	void DisplayLines(void);
	char GetVKStateCharacter(void);
	void CheckInput(long ticks);
	void SendChatString(char* String);

	void DrawOutline(void);
};

bool Chat::Initialize(const char* IP)
{
	strcpy_s(ServerIP, SERVER_IP_LENGTH, IP);

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
	winsockWrapper.ClearBuffer(0);
	winsockWrapper.WriteChar(3, 0);
	winsockWrapper.WriteString(Name, 0);
	winsockWrapper.SendMessagePacket(ServerSocket, ServerIP, SERVER_PORT, 0);

	// Clear the dialogue
	for (int i = 0; i < DIALOGUE_LINES; i += 1) Dialogue[i][0] = 0;
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
	case 1:
		winsockWrapper.ClearBuffer(0);
		winsockWrapper.WriteChar(1, 0);
		winsockWrapper.SendMessagePacket(ServerSocket, ServerIP, SERVER_PORT, 0);
		break;
	case 2:
	{
		int messageSize = winsockWrapper.ReadInt(0);

		//  Decrypt using Groundfish
		unsigned char encrypted[256];
		memcpy(encrypted, winsockWrapper.ReadChars(0, messageSize), messageSize);
		char decrypted[256];
		Groundfish::Decrypt(encrypted, decrypted);

		NewLine(decrypted);
	}
	break;
	}

	return true;
}

void Chat::NewLine(char* NewString)
{
	int stringLength = (int)strlen(NewString);
	int Lines = stringLength / MAX_LINE_LENGTH + 1;
	if (((Lines - 1) * MAX_LINE_LENGTH) == stringLength) Lines -= 1;

	// Move all existing lines upward the number of line being added
	for (int i = Lines; i < DIALOGUE_LINES; i += 1)
	{
		for (int j = 0; j < MAX_LINE_LENGTH; j += 1) Dialogue[i - Lines][j] = Dialogue[i][j];
		Dialogue[i - Lines][MAX_LINE_LENGTH] = 0;
	}

	// Add the new lines
	for (int i = 0; i < Lines; i += 1)
	{
		for (int j = 0; j < MAX_LINE_LENGTH; j += 1) Dialogue[DIALOGUE_LINES - 1 - i][j] = 0;
		for (int j = 0; j < MAX_LINE_LENGTH; j += 1) Dialogue[DIALOGUE_LINES - 1 - i][j] = NewString[(Lines - 1 - i) * MAX_LINE_LENGTH + j];
		Dialogue[DIALOGUE_LINES - 1 - i][MAX_LINE_LENGTH] = 0;
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
		for (int i = 0; i < DIALOGUE_LINES; i += 1)
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
	for (int i = 0; i < 80; i += 1) cout << "-";
	for (int i = 1; i < DIALOGUE_LINES + 1; i += 1)
	{
		C.X = 0;	C.Y = i;	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), C);
		cout << "|";
		C.X = 79;	C.Y = i;	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), C);
		cout << "|";
	}
	for (int i = 0; i < 80; i += 1) cout << "-";
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