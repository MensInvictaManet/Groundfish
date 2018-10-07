#include "GroundfishServer.h"
#include <windows.h>
#include <iostream>

#define KEY_DOWN(vk_code) ((GetAsyncKeyState(vk_code) & 0x8000) ? 1 : 0)

bool TestGroundfish(unsigned char wordListIndex)
{
	const char* testMessage = "This is a test of the Groundfish encryption system.";
	unsigned char EncryptedMessage[128];
	char DecryptedMessage[128];
	unsigned int messageLength = int(strlen(testMessage)) + 1;
	Groundfish::Encrypt(testMessage, (unsigned char*)EncryptedMessage, messageLength, 0, wordListIndex);
	Groundfish::Decrypt((unsigned char*)EncryptedMessage, (char*)DecryptedMessage);
	return (strcmp(testMessage, DecryptedMessage) == 0);
}

void Display_DEBUG(Server* SERVER)
{
	COORD C;

	C.X = 0;	C.Y = 0;	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), C);
	std::cout << "DEBUG DISPLAY: FIRST TEN CLIENTS";
	C.X = 0;	C.Y = 1;	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), C);
	std::cout << "================================";

	auto userDisplayIndex = 0;
	auto userConnectionsList = SERVER->GetUserList();
	for (auto userIter = userConnectionsList.begin(); userIter != userConnectionsList.end() && (++userDisplayIndex < 10); ++userIter)
	{
		auto user = (*userIter).first;

		if (userDisplayIndex < int(SERVER->GetUserList().size()))
		{
			C.X = 0;	C.Y = 2 + userDisplayIndex;	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), C);
			std::cout << "#" << userDisplayIndex + 1 << ":  " << user->IPAddress << "                    ";
		}
		else
		{
			C.X = 0;	C.Y = 2 + userDisplayIndex;	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), C);
			std::cout << "                                        ";
		}
	}
}

int main(void)
{
	srand((unsigned int)(time(NULL)));

	//  Load the current Groundfish word list
	Groundfish::LoadCurrentWordList();

	//  Initialize the Winsock wrapper
	winsockWrapper.WinsockInitialize();
	Server chatServer;
	assert(chatServer.Initialize());

	//	Step 2: EXECUTION
	while (!KEY_DOWN(VK_F2))
	{
		chatServer.MainProcess();

		// Debug data
		Display_DEBUG(&chatServer);
	}
}