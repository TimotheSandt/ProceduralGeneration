#include "Game.h"
#include "Logger.h"

#include <iostream>

int main() {
	SetWorkingDirectoryToExe();

#ifdef DEBUG
	SET_LOG_FILE("logs/log.log");
#else
	SET_LOG_FILE_DEFAULT;
#endif



	if (!Window::InitOpenGL())
		return 1;

	
	Game game;
	game.init();
	game.run();
	game.stop();	


	Window::TerminateOpenGL();
	FLUSH_LOG_TO_FILE;
	return 0;
}