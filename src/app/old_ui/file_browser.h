#pragma once
#include <filesystem>
#include <functional>
#include <vector>

struct FileFilter
{
    std::string filter_name;
    std::vector<std::pair<std::string, std::string>> filters;
    
    FileFilter(std::string filter_name, std::string str);
    bool matches(std::filesystem::path path) const;
    std::string filers_to_string() const;
};

class FileBrowser
{
public:
    typedef std::function<void()> Callback;

private:
    struct location_t
    {
        std::filesystem::path path;
        std::string name;
    };

    std::vector<location_t> bookmarks;
    std::filesystem::path last_directory;

    bool _is_open;

    std::function<void()> callback;

    std::vector<FileFilter> file_filters;
    std::vector<std::string> filter_strings;
    size_t selected_filter;
    char* path_buf[256];
    char* name_buf[256];

    std::vector<std::filesystem::path> back_stack;
    std::vector<std::filesystem::path> forward_stack;
    // selected
    bool open_error_popup;
    bool enter_path;
    bool show_path_input;
    bool scroll_to_selected;
    bool multi, write;

    void reset();
    void open();
    void refresh_locations();
    void set_path(std::filesystem::path new_path);
public:
    enum OpenMode
    {
        Save, Open, Multiopen
    };

    FileBrowser();

    std::string default_path;

    void open_read(std::vector<FileFilter> file_filters, bool multi, Callback callback);
    void open_write(std::vector<FileFilter> file_filters, std::string default_name, Callback callback);

    void close();
};