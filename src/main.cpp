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
		return EXIT_FAILURE;

	LOG_INFO("Starting game");

	Game game;
	LOG_TRACE("Game created");
	game.init();
	LOG_TRACE("Game initialized");
	game.run();
	game.stop();

	LOG_INFO("Game stopped");
	Window::TerminateOpenGL();
	FLUSH_LOG_TO_FILE;
	return EXIT_SUCCESS;
}
