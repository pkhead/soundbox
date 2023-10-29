#include <filesystem>
#include <cstring>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

#include "file_browser.h"

enum class UserDirectoryType
{
    Home,
    Desktop,
    Documents,
    Music,
    Pictures,
    Videos,
    Downloads
};

static std::filesystem::path get_user_directory(UserDirectoryType type)
{
    std::filesystem::path out;

#ifdef _WIN32
    const char* path;

    switch (type)
    {
        case UserDirectoryType::Home:
            path = ".";
            break;
        case UserDirectoryType::Desktop:
            path = "Desktop";
            break;
        case UserDirectoryType::Downloads:
            path = "Downloads";
            break;
        case UserDirectoryType::Music:
            path = "Music";
            break;
        case UserDirectoryType::Pictures:
            path = "Pictures";
            break;
        case UserDirectoryType::Videos:
            path = "Videos";
            break;
        default:
            throw std::runtime_error("Invalid UserDirectoryType");
    }

    out = std::filesystem::path(std::getenv("USERPROFILE")) / path;

#else
    const char* type_str;

    switch (type)
    {
        case UserDirectoryType::Home:
            type_str = "HOME";
            break;
        case UserDirectoryType::Desktop:
            type_str = "DESKTOP";
            break;
        case UserDirectoryType::Downloads:
            type_str = "DOWNLOAD";
            break;
        case UserDirectoryType::Music:
            type_str = "MUSIC";
            break;
        case UserDirectoryType::Pictures:
            type_str = "PICTURES";
            break;
        case UserDirectoryType::Videos:
            type_str = "VIDEOS";
            break;
        default:
            throw std::runtime_error("Invalid UserDirectoryType");
    }

    bool s = true;
    char buf[256];
    int err;

    std::string cmd = std::string("xdg-user-dir ") + type_str;

    FILE* pipe = popen(cmd.c_str(), "r");

    if (pipe && fgets(buf, 256, pipe) != nullptr && pclose(pipe) == 0) {
        buf[strlen(buf) - 1] = 0; // remove ending newline
        out = buf;
    }
#endif

    return out;
}

FileBrowser::FileBrowser()
{
    // get system locations
    struct type_t { UserDirectoryType type; std::string name; };

    std::array<type_t, 7> directories({
        UserDirectoryType::Home,        "Home",
        UserDirectoryType::Desktop,     "Desktop",
        UserDirectoryType::Documents,   "Documents",
        UserDirectoryType::Music,       "Music",
        UserDirectoryType::Pictures,    "Pictures",
        UserDirectoryType::Videos,      "Videos",
        UserDirectoryType::Downloads,   "Downloads"
    });

    for (type_t type : directories)
    {
        std::filesystem::path p = get_user_directory(type.type);
        if (!p.empty())
            bookmarks.push_back({ p, type.name });
    }

    // WINDOWS: get drive letters
#ifdef _WIN32
    {
        const char* DRIVE_LETTERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

        unsigned long drives = GetLogicalDrives();
        size_t mask = 1;

        for (int i = 0; i < 32; i++)
        {
            if ((drives & mask) != 0)
            {
                bookmarks.push_back({
                    std::string(DRIVE_LETTERS[i]) + ":",
                    std::string(DRIVE_LETTERS[i]) + ":",
                });
            }

            mask <<= 1;
        }
    }
#else
    bookmarks.push_back({
        std::filesystem::path("/"),
        "File System"
    });
#endif
}

void FileBrowser::reset()
{
    _is_open = false;

    file_filters.clear();
    filter_strings.clear();
    selected_filter = 0;
    memset(path_buf, 0, sizeof(path_buf));
    memset(name_buf, 0, sizeof(name_buf));

    back_stack.clear();
    forward_stack.clear();
    
    open_error_popup = false;
    enter_path = false;
    show_path_input = false;
    scroll_to_selected = false;
    multi = false;
    write = false;
}

void FileBrowser::open()
{
    if (file_filters.empty())
    {
        file_filters.push_back(FileFilter("Any", "*.*"));
    }

    // construct filter labels
    filter_strings.clear();
    for (auto& filter : file_filters)
    {
        std::stringstream str;
        str << filter.filter_name << " (" << filter.filers_to_string() << ")";
        filter_strings.push_back(str.str());
    }

    // check if last opened directory still exists
    if (last_directory.empty() || !std::filesystem::exists(last_directory))
    {
        last_directory = default_path;
    }

    set_path(last_directory);
}

void FileBrowser::open_read(std::vector<FileFilter> _file_filters, bool _multi, Callback _callback)
{
    reset();
    callback = _callback;
    file_filters = _file_filters;
    multi = _multi;

    open();
}

void FileBrowser::set_path(std::filesystem::path new_path)
{
    
}
