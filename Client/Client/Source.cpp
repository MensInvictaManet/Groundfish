#include "Groundfish.h"
#include "Chat.h"

int main(void)
{
	//  Load the current Groundfish word list
	Groundfish::LoadCurrentWordList();

	//  Initialize the Winsock wrapper
	winsockWrapper.WinsockInitialize();

	Chat CHAT;
	if (CHAT.Initialize("127.0.0.1"))
	{
		//	Step 2: EXECUTION
		while (!KEY_DOWN(VK_F3))
			if (!CHAT.MainProcess()) break;
	}

	//	Step 3: SHUTDOWN
	CHAT.Shutdown();
	winsockWrapper.WinsockShutdown();
	system("cls");
}