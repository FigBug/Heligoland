#include "FileSystemWatcher.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>
#include <map>

#if defined(__APPLE__)
#include <CoreServices/CoreServices.h>
#include <dispatch/dispatch.h>
#elif defined(__linux__)
#include <sys/inotify.h>
#include <unistd.h>
#include <limits.h>
#include <poll.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

//==============================================================================
#if defined(__APPLE__)

class FileSystemWatcher::Impl
{
public:
    Impl (FileSystemWatcher& o) : owner (o)
    {
        queue = dispatch_queue_create ("FileSystemWatcher", DISPATCH_QUEUE_SERIAL);
    }

    ~Impl()
    {
        removeAllFolders();
        dispatch_release (queue);
    }

    void addListener (Listener* l)
    {
        std::lock_guard<std::mutex> lock (listenerMutex);
        listeners.push_back (l);
    }

    void removeListener (Listener* l)
    {
        std::lock_guard<std::mutex> lock (listenerMutex);
        listeners.erase (std::remove (listeners.begin(), listeners.end(), l), listeners.end());
    }

    void addFolder (const std::string& folder)
    {
        std::lock_guard<std::mutex> lock (folderMutex);

        if (streams.find (folder) != streams.end())
            return;

        CFStringRef path = CFStringCreateWithCString (nullptr, folder.c_str(), kCFStringEncodingUTF8);
        CFArrayRef paths = CFArrayCreate (nullptr, (const void**) &path, 1, &kCFTypeArrayCallBacks);

        FSEventStreamContext context = {};
        context.info = this;

        FSEventStreamRef stream = FSEventStreamCreate (
            nullptr,
            &Impl::callback,
            &context,
            paths,
            kFSEventStreamEventIdSinceNow,
            0.1,
            kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagUseCFTypes
        );

        CFRelease (paths);
        CFRelease (path);

        if (stream)
        {
            FSEventStreamSetDispatchQueue (stream, queue);
            FSEventStreamStart (stream);
            streams[folder] = stream;
            folders.push_back (folder);
        }
    }

    void removeFolder (const std::string& folder)
    {
        std::lock_guard<std::mutex> lock (folderMutex);

        auto it = streams.find (folder);
        if (it != streams.end())
        {
            FSEventStreamStop (it->second);
            FSEventStreamInvalidate (it->second);
            FSEventStreamRelease (it->second);
            streams.erase (it);
        }

        folders.erase (std::remove (folders.begin(), folders.end(), folder), folders.end());
    }

    void removeAllFolders()
    {
        std::lock_guard<std::mutex> lock (folderMutex);

        for (auto& pair : streams)
        {
            FSEventStreamStop (pair.second);
            FSEventStreamInvalidate (pair.second);
            FSEventStreamRelease (pair.second);
        }
        streams.clear();
        folders.clear();
    }

    std::vector<std::string> getWatchedFolders() const
    {
        std::lock_guard<std::mutex> lock (folderMutex);
        return folders;
    }

private:
    void notifyFolderChanged (const std::string& folder)
    {
        std::lock_guard<std::mutex> lock (listenerMutex);
        for (auto* l : listeners)
            l->folderChanged (folder);
    }

    void notifyFileChanged (const std::string& file, Event event)
    {
        std::lock_guard<std::mutex> lock (listenerMutex);
        for (auto* l : listeners)
            l->fileChanged (file, event);
    }

    static void callback (ConstFSEventStreamRef, void* info, size_t numEvents,
                          void* eventPaths, const FSEventStreamEventFlags* flags,
                          const FSEventStreamEventId*)
    {
        auto* self = static_cast<Impl*> (info);
        CFArrayRef paths = static_cast<CFArrayRef> (eventPaths);

        for (size_t i = 0; i < numEvents; ++i)
        {
            CFStringRef cfPath = static_cast<CFStringRef> (CFArrayGetValueAtIndex (paths, (CFIndex) i));
            char path[PATH_MAX];
            CFStringGetCString (cfPath, path, PATH_MAX, kCFStringEncodingUTF8);

            FileSystemWatcher::Event event = FileSystemWatcher::Event::fileModified;
            if (flags[i] & kFSEventStreamEventFlagItemCreated)
                event = FileSystemWatcher::Event::fileCreated;
            else if (flags[i] & kFSEventStreamEventFlagItemRemoved)
                event = FileSystemWatcher::Event::fileDeleted;
            else if (flags[i] & kFSEventStreamEventFlagItemRenamed)
                event = FileSystemWatcher::Event::fileRenamed;

            self->notifyFileChanged (path, event);

            std::string pathStr (path);
            auto lastSlash = pathStr.rfind ('/');
            if (lastSlash != std::string::npos)
                self->notifyFolderChanged (pathStr.substr (0, lastSlash));
        }
    }

    FileSystemWatcher& owner;
    std::vector<Listener*> listeners;
    std::mutex listenerMutex;
    dispatch_queue_t queue = nullptr;
    std::map<std::string, FSEventStreamRef> streams;
    std::vector<std::string> folders;
    mutable std::mutex folderMutex;
};

//==============================================================================
#elif defined(__linux__)

class FileSystemWatcher::Impl
{
public:
    Impl (FileSystemWatcher& o) : owner (o)
    {
        inotifyFd = inotify_init1 (IN_NONBLOCK);
        if (inotifyFd >= 0)
        {
            running = true;
            watchThread = std::thread ([this] { threadFunc(); });
        }
    }

    ~Impl()
    {
        running = false;
        if (watchThread.joinable())
            watchThread.join();

        removeAllFolders();

        if (inotifyFd >= 0)
            close (inotifyFd);
    }

    void addListener (Listener* l)
    {
        std::lock_guard<std::mutex> lock (listenerMutex);
        listeners.push_back (l);
    }

    void removeListener (Listener* l)
    {
        std::lock_guard<std::mutex> lock (listenerMutex);
        listeners.erase (std::remove (listeners.begin(), listeners.end(), l), listeners.end());
    }

    void addFolder (const std::string& folder)
    {
        std::lock_guard<std::mutex> lock (folderMutex);

        if (inotifyFd < 0)
            return;

        int wd = inotify_add_watch (inotifyFd, folder.c_str(),
            IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO);

        if (wd >= 0)
        {
            watches[wd] = folder;
            folders.push_back (folder);
        }
    }

    void removeFolder (const std::string& folder)
    {
        std::lock_guard<std::mutex> lock (folderMutex);

        for (auto it = watches.begin(); it != watches.end(); ++it)
        {
            if (it->second == folder)
            {
                inotify_rm_watch (inotifyFd, it->first);
                watches.erase (it);
                break;
            }
        }

        folders.erase (std::remove (folders.begin(), folders.end(), folder), folders.end());
    }

    void removeAllFolders()
    {
        std::lock_guard<std::mutex> lock (folderMutex);

        for (auto& pair : watches)
            inotify_rm_watch (inotifyFd, pair.first);

        watches.clear();
        folders.clear();
    }

    std::vector<std::string> getWatchedFolders() const
    {
        std::lock_guard<std::mutex> lock (folderMutex);
        return folders;
    }

private:
    void notifyFolderChanged (const std::string& folder)
    {
        std::lock_guard<std::mutex> lock (listenerMutex);
        for (auto* l : listeners)
            l->folderChanged (folder);
    }

    void notifyFileChanged (const std::string& file, Event event)
    {
        std::lock_guard<std::mutex> lock (listenerMutex);
        for (auto* l : listeners)
            l->fileChanged (file, event);
    }

    void threadFunc()
    {
        char buffer[4096];
        pollfd pfd = { inotifyFd, POLLIN, 0 };

        while (running)
        {
            int ret = poll (&pfd, 1, 100);
            if (ret <= 0)
                continue;

            ssize_t len = read (inotifyFd, buffer, sizeof (buffer));
            if (len <= 0)
                continue;

            for (char* ptr = buffer; ptr < buffer + len;)
            {
                auto* event = reinterpret_cast<inotify_event*> (ptr);
                processEvent (event);
                ptr += sizeof (inotify_event) + event->len;
            }
        }
    }

    void processEvent (inotify_event* event)
    {
        std::string folder;
        {
            std::lock_guard<std::mutex> lock (folderMutex);
            auto it = watches.find (event->wd);
            if (it == watches.end())
                return;
            folder = it->second;
        }

        std::string file = folder + "/" + event->name;

        FileSystemWatcher::Event eventType = FileSystemWatcher::Event::fileModified;
        if (event->mask & IN_CREATE)
            eventType = FileSystemWatcher::Event::fileCreated;
        else if (event->mask & IN_DELETE)
            eventType = FileSystemWatcher::Event::fileDeleted;
        else if (event->mask & (IN_MOVED_FROM | IN_MOVED_TO))
            eventType = FileSystemWatcher::Event::fileRenamed;

        notifyFileChanged (file, eventType);
        notifyFolderChanged (folder);
    }

    FileSystemWatcher& owner;
    std::vector<Listener*> listeners;
    std::mutex listenerMutex;
    int inotifyFd = -1;
    std::atomic<bool> running { false };
    std::thread watchThread;
    std::map<int, std::string> watches;
    std::vector<std::string> folders;
    mutable std::mutex folderMutex;
};

//==============================================================================
#elif defined(_WIN32)

class FileSystemWatcher::Impl
{
public:
    Impl (FileSystemWatcher& o) : owner (o) {}

    ~Impl()
    {
        removeAllFolders();
    }

    void addListener (Listener* l)
    {
        std::lock_guard<std::mutex> lock (listenerMutex);
        listeners.push_back (l);
    }

    void removeListener (Listener* l)
    {
        std::lock_guard<std::mutex> lock (listenerMutex);
        listeners.erase (std::remove (listeners.begin(), listeners.end(), l), listeners.end());
    }

    void addFolder (const std::string& folder)
    {
        std::lock_guard<std::mutex> lock (folderMutex);

        if (watchData.find (folder) != watchData.end())
            return;

        HANDLE handle = CreateFileA (
            folder.c_str(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            nullptr
        );

        if (handle == INVALID_HANDLE_VALUE)
            return;

        auto watch = std::make_shared<WatchData>();
        watch->folder = folder;
        watch->handle = handle;
        watch->owner = this;
        watch->overlapped.hEvent = CreateEvent (nullptr, TRUE, FALSE, nullptr);
        watch->running = true;

        if (startWatch (watch.get()))
        {
            watchData[folder] = watch;
            folders.push_back (folder);

            watch->thread = std::thread ([watch] {
                while (watch->running)
                {
                    DWORD result = WaitForSingleObject (watch->overlapped.hEvent, 100);
                    if (result == WAIT_OBJECT_0 && watch->running)
                    {
                        DWORD bytesTransferred;
                        if (GetOverlappedResult (watch->handle, &watch->overlapped, &bytesTransferred, FALSE))
                        {
                            watch->owner->processEvents (watch.get(), bytesTransferred);
                            ResetEvent (watch->overlapped.hEvent);
                            if (watch->running)
                                watch->owner->startWatch (watch.get());
                        }
                    }
                }
            });
        }
        else
        {
            CloseHandle (watch->overlapped.hEvent);
            CloseHandle (handle);
        }
    }

    void removeFolder (const std::string& folder)
    {
        std::shared_ptr<WatchData> watch;
        {
            std::lock_guard<std::mutex> lock (folderMutex);
            auto it = watchData.find (folder);
            if (it != watchData.end())
            {
                watch = it->second;
                watchData.erase (it);
            }
            folders.erase (std::remove (folders.begin(), folders.end(), folder), folders.end());
        }

        if (watch)
        {
            watch->running = false;
            CancelIo (watch->handle);
            SetEvent (watch->overlapped.hEvent);
            if (watch->thread.joinable())
                watch->thread.join();
            CloseHandle (watch->overlapped.hEvent);
            CloseHandle (watch->handle);
        }
    }

    void removeAllFolders()
    {
        std::vector<std::string> foldersToRemove;
        {
            std::lock_guard<std::mutex> lock (folderMutex);
            foldersToRemove = folders;
        }

        for (const auto& folder : foldersToRemove)
            removeFolder (folder);
    }

    std::vector<std::string> getWatchedFolders() const
    {
        std::lock_guard<std::mutex> lock (folderMutex);
        return folders;
    }

private:
    void notifyFolderChanged (const std::string& folder)
    {
        std::lock_guard<std::mutex> lock (listenerMutex);
        for (auto* l : listeners)
            l->folderChanged (folder);
    }

    void notifyFileChanged (const std::string& file, Event event)
    {
        std::lock_guard<std::mutex> lock (listenerMutex);
        for (auto* l : listeners)
            l->fileChanged (file, event);
    }

    struct WatchData
    {
        std::string folder;
        HANDLE handle = INVALID_HANDLE_VALUE;
        OVERLAPPED overlapped = {};
        alignas(DWORD) char buffer[32768];
        std::atomic<bool> running { false };
        Impl* owner = nullptr;
        std::thread thread;
    };

    bool startWatch (WatchData* watch)
    {
        return ReadDirectoryChangesW (
            watch->handle,
            watch->buffer,
            sizeof (watch->buffer),
            TRUE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION,
            nullptr,
            &watch->overlapped,
            nullptr
        ) != 0;
    }

    void processEvents (WatchData* watch, DWORD bytesTransferred)
    {
        if (bytesTransferred == 0)
            return;

        char* ptr = watch->buffer;
        while (true)
        {
            auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*> (ptr);

            int len = WideCharToMultiByte (CP_UTF8, 0, info->FileName,
                info->FileNameLength / sizeof (WCHAR), nullptr, 0, nullptr, nullptr);
            std::string filename (len, '\0');
            WideCharToMultiByte (CP_UTF8, 0, info->FileName,
                info->FileNameLength / sizeof (WCHAR), &filename[0], len, nullptr, nullptr);

            std::string fullPath = watch->folder + "\\" + filename;

            FileSystemWatcher::Event event = FileSystemWatcher::Event::fileModified;
            switch (info->Action)
            {
                case FILE_ACTION_ADDED:            event = FileSystemWatcher::Event::fileCreated; break;
                case FILE_ACTION_REMOVED:          event = FileSystemWatcher::Event::fileDeleted; break;
                case FILE_ACTION_RENAMED_OLD_NAME:
                case FILE_ACTION_RENAMED_NEW_NAME: event = FileSystemWatcher::Event::fileRenamed; break;
            }

            notifyFileChanged (fullPath, event);
            notifyFolderChanged (watch->folder);

            if (info->NextEntryOffset == 0)
                break;
            ptr += info->NextEntryOffset;
        }
    }

    FileSystemWatcher& owner;
    std::vector<Listener*> listeners;
    std::mutex listenerMutex;
    std::map<std::string, std::shared_ptr<WatchData>> watchData;
    std::vector<std::string> folders;
    mutable std::mutex folderMutex;
};

#endif

//==============================================================================
FileSystemWatcher::FileSystemWatcher()
    : impl (std::make_unique<Impl> (*this))
{
}

FileSystemWatcher::~FileSystemWatcher() = default;

void FileSystemWatcher::addListener (Listener* listener)
{
    impl->addListener (listener);
}

void FileSystemWatcher::removeListener (Listener* listener)
{
    impl->removeListener (listener);
}

void FileSystemWatcher::addFolder (const std::string& folder)
{
    impl->addFolder (folder);
}

void FileSystemWatcher::removeFolder (const std::string& folder)
{
    impl->removeFolder (folder);
}

void FileSystemWatcher::removeAllFolders()
{
    impl->removeAllFolders();
}

std::vector<std::string> FileSystemWatcher::getWatchedFolders() const
{
    return impl->getWatchedFolders();
}
