#include "Game.h"
#include "Logger.h"

int main() {
	SET_LOG_FILE("logs/log.log");

	Game game;
	game.init();
	game.run();

	FLUSH_LOG_TO_FILE;
	return 0;
}