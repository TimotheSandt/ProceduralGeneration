#include "Game.h"
#include "Logger.h"

int main() {
	SET_LOG_FILE("logs/log.log");

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