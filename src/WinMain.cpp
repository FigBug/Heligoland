#if defined(_WIN32)

#include <windows.h>

// Forward declaration - defined in main.cpp
int runGame();

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    (void) hInstance;
    (void) hPrevInstance;
    (void) lpCmdLine;
    (void) nCmdShow;

    return runGame();
}

#endif
