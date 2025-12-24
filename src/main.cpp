#include "Game.h"

#if defined(_WIN32)
#include <windows.h>

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    (void) hInstance;
    (void) hPrevInstance;
    (void) lpCmdLine;
    (void) nCmdShow;

    Game game;

    if (! game.init())
    {
        return 1;
    }

    game.run();
    game.shutdown();

    return 0;
}

#else

int main (int argc, char* argv[])
{
    (void) argc;
    (void) argv;

    Game game;

    if (! game.init())
    {
        return 1;
    }

    game.run();
    game.shutdown();

    return 0;
}

#endif
