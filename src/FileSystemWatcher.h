#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

class FileSystemWatcher
{
public:
    enum class Event
    {
        fileCreated,
        fileDeleted,
        fileModified,
        fileRenamed
    };

    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void folderChanged (const std::string& folder) {}
        virtual void fileChanged (const std::string& file, Event event) {}
    };

    FileSystemWatcher();
    ~FileSystemWatcher();

    void addListener (Listener* listener);
    void removeListener (Listener* listener);

    void addFolder (const std::string& folder);
    void removeFolder (const std::string& folder);
    void removeAllFolders();

    std::vector<std::string> getWatchedFolders() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};
