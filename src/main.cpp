#include "Game.h"
#include <SDL3/SDL_main.h>

int main (int argc, char* argv[])
{
    Game game;

    if (! game.init())
    {
        return 1;
    }

    game.run();
    game.shutdown();

    return 0;
}
