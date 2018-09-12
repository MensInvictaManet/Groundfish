#include "Groundfish.h"
#include "GroundfishChat.h"
#include <time.h>

clock_t Timer;
clock_t Ticks;

int main(void)
{
	//  Load the current Groundfish word list
	Groundfish::LoadCurrentWordList();

	//  Initialize the Winsock wrapper
	winsockWrapper.WinsockInitialize();

	Chat CHAT;
	if (CHAT.Initialize())
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