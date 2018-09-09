#include "Groundfish.h"
#include "GroundfishChat.h"
#include <time.h>

#define CHAT_SERVER_IP	"127.0.0.1"

clock_t Timer;
clock_t Ticks;

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
		{
			if (!CHAT.MainProcess(Ticks)) break;
			Ticks = clock() - Timer;
			Timer = clock();
		}
	}

	//	Step 3: SHUTDOWN
	CHAT.Shutdown();
	winsockWrapper.WinsockShutdown();
	system("cls");
}