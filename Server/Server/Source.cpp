#include "GroundfishServer.h"
#include <windows.h>
#include <iostream>

#define KEY_DOWN(vk_code) ((GetAsyncKeyState(vk_code) & 0x8000) ? 1 : 0)

bool TestGroundfish(unsigned char wordListIndex)
{
	const char* testMessage = "This is a test of the Groundfish encryption system.";
	unsigned char EncryptedMessage[128];
	char DecryptedMessage[128];
	unsigned int messageLength = strlen(testMessage) + 1;
	Groundfish::Encrypt(testMessage, (unsigned char*)EncryptedMessage, messageLength, 0, wordListIndex);
	Groundfish::Decrypt((unsigned char*)EncryptedMessage, (char*)DecryptedMessage);
	return (strcmp(testMessage, DecryptedMessage) == 0);
}

void Display_DEBUG(Server* SERVER)
{
	COORD C;

	C.X = 3;	C.Y = 5;	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), C);
	std::cout << "DEBUG DISPLAY: FIRST TEN CLIENTS";
	C.X = 3;	C.Y = 6;	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), C);
	std::cout << "================================";

	int DebugCount = 10;
	for (int i = 0; i < DebugCount; i += 1)
	{
		if (i < SERVER->Client_Count)
		{
			C.X = 3;	C.Y = 7 + i;	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), C);
			std::cout << "#" << i + 1 << ":  " << SERVER->GetClientIP(i) << "                    ";
		}
		else
		{
			C.X = 3;	C.Y = 7 + i;	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), C);
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
		if (chatServer.MainProcess() == 0)
		{
			system("cls");
			std::cout << "Server has shut down" << std::endl;
			break;
		}

		// Debug data
		Display_DEBUG(&chatServer);
	}
}