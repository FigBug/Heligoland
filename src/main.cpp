#include "Config.h"
#include "Game.h"

int runGame()
{
    config.load();
    config.startWatching();

    Game game;

    if (! game.init())
    {
        return 1;
    }

    game.run();
    game.shutdown();

    return 0;
}

#if !defined(_WIN32)

int main (int argc, char* argv[])
{
    (void) argc;
    (void) argv;

    return runGame();
}

#endif
