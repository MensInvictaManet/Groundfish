#include "Groundfish.h"
#include "GroundfishChat.h"

#define CHAT_SERVER_IP	"groundfishchat.zapto.org"

int main(void)
{
	//  Load the current Groundfish word list
	Groundfish::LoadCurrentWordList();

	//  Initialize the Winsock wrapper
	winsockWrapper.WinsockInitialize();

	Chat CHAT;
	if (CHAT.Initialize(CHAT_SERVER_IP))
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