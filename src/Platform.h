#pragma once

#include <string>

namespace Platform
{
    // Returns the user data directory for this application, creating it if it doesn't exist.
    // macOS: ~/Library/Application Support/Heligoland/
    // Windows: %APPDATA%/Heligoland/
    // Linux: ~/.local/share/Heligoland/
    std::string getUserDataDirectory();
}
