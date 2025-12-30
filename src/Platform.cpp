#include "Platform.h"

#include <sys/stat.h>

#if defined(_WIN32)
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <pwd.h>
#endif

namespace Platform
{

namespace
{
    const char* appName = "Heligoland";

    bool directoryExists (const std::string& path)
    {
        struct stat info;
        if (stat (path.c_str(), &info) != 0)
            return false;
        return (info.st_mode & S_IFDIR) != 0;
    }

    bool createDirectory (const std::string& path)
    {
       #if defined(_WIN32)
        return CreateDirectoryA (path.c_str(), nullptr) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
       #else
        return mkdir (path.c_str(), 0755) == 0 || errno == EEXIST;
       #endif
    }

    bool createDirectoryRecursive (const std::string& path)
    {
        if (directoryExists (path))
            return true;

        // Find parent directory
        size_t pos = path.find_last_of ("/\\");
        if (pos != std::string::npos && pos > 0)
        {
            std::string parent = path.substr (0, pos);
            if (! createDirectoryRecursive (parent))
                return false;
        }

        return createDirectory (path);
    }

   #if defined(_WIN32)
    std::string getHomeDirectory()
    {
        char path[MAX_PATH];
        if (SUCCEEDED (SHGetFolderPathA (nullptr, CSIDL_APPDATA, nullptr, 0, path)))
            return std::string (path);

        // Fallback to environment variable
        const char* appdata = getenv ("APPDATA");
        if (appdata)
            return std::string (appdata);

        return "";
    }
   #else
    std::string getHomeDirectory()
    {
        const char* home = getenv ("HOME");
        if (home)
            return std::string (home);

        struct passwd* pw = getpwuid (getuid());
        if (pw)
            return std::string (pw->pw_dir);

        return "";
    }
   #endif
}

std::string getUserDataDirectory()
{
    std::string basePath;

   #if defined(__APPLE__)
    basePath = getHomeDirectory() + "/Library/Application Support";
   #elif defined(_WIN32)
    basePath = getHomeDirectory();
   #elif defined(__linux__)
    // Follow XDG Base Directory spec
    const char* xdgData = getenv ("XDG_DATA_HOME");
    if (xdgData && xdgData[0] != '\0')
        basePath = std::string (xdgData);
    else
        basePath = getHomeDirectory() + "/.local/share";
   #endif

    if (basePath.empty())
        return "";

    std::string fullPath = basePath + "/" + appName;

    if (! createDirectoryRecursive (fullPath))
        return "";

    return fullPath;
}

}
