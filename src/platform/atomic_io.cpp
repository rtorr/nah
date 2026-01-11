#include "nah/platform.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <random>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#if defined(__APPLE__)
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#else
extern "C" char** environ;
#endif
#endif

namespace nah {

namespace fs = std::filesystem;

Platform get_current_platform() {
#if defined(__APPLE__)
    return Platform::macOS;
#elif defined(_WIN32)
    return Platform::Windows;
#elif defined(__linux__)
    return Platform::Linux;
#else
    return Platform::Unknown;
#endif
}

namespace {

#ifndef _WIN32
// fsync a file descriptor
bool fsync_fd(int fd) {
#ifdef __APPLE__
    return fcntl(fd, F_FULLFSYNC, 0) == 0;
#else
    return fsync(fd) == 0;
#endif
}

// fsync a directory by path
bool fsync_directory(const std::string& dir_path) {
    int dir_fd = open(dir_path.c_str(), O_RDONLY);
    if (dir_fd < 0) return false;
    
    bool result = fsync_fd(dir_fd);
    close(dir_fd);
    return result;
}

// Generate a temporary filename
std::string make_temp_filename(const std::string& base) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::string hex_chars = "0123456789abcdef";
    std::string suffix;
    for (int i = 0; i < 8; ++i) {
        suffix += hex_chars[static_cast<size_t>(dis(gen))];
    }
    
    return base + ".tmp." + suffix;
}
#endif

} // namespace

AtomicWriteResult atomic_write_file(const std::string& path, const std::string& content) {
    return atomic_write_file(path, std::vector<uint8_t>(content.begin(), content.end()));
}

AtomicWriteResult atomic_write_file(const std::string& path, const std::vector<uint8_t>& content) {
    AtomicWriteResult result;
    
#ifdef _WIN32
    // Windows implementation using ReplaceFile
    std::string temp_path = path + ".tmp";
    
    // Write to temp file
    std::ofstream temp_file(temp_path, std::ios::binary);
    if (!temp_file) {
        result.error = "failed to create temp file";
        return result;
    }
    
    temp_file.write(reinterpret_cast<const char*>(content.data()), 
                    static_cast<std::streamsize>(content.size()));
    temp_file.flush();
    temp_file.close();
    
    // Rename
    if (!MoveFileExA(temp_path.c_str(), path.c_str(), 
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileA(temp_path.c_str());
        result.error = "failed to rename temp file";
        return result;
    }
    
    result.ok = true;
#else
    // POSIX implementation: temp + fsync(file) + rename + fsync(dir)
    std::string dir_path = get_parent_directory(path);
    std::string temp_path = make_temp_filename(path);
    
    // Create and write to temp file
    int fd = open(temp_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        result.error = "failed to create temp file: " + std::string(strerror(errno));
        return result;
    }
    
    ssize_t written = write(fd, content.data(), content.size());
    if (written < 0 || static_cast<size_t>(written) != content.size()) {
        close(fd);
        unlink(temp_path.c_str());
        result.error = "failed to write content";
        return result;
    }
    
    // fsync the file
    if (!fsync_fd(fd)) {
        close(fd);
        unlink(temp_path.c_str());
        result.error = "failed to fsync temp file";
        return result;
    }
    
    close(fd);
    
    // Rename to final path
    if (rename(temp_path.c_str(), path.c_str()) != 0) {
        unlink(temp_path.c_str());
        result.error = "failed to rename temp file: " + std::string(strerror(errno));
        return result;
    }
    
    // fsync the directory
    if (!dir_path.empty()) {
        fsync_directory(dir_path);
    }
    
    result.ok = true;
#endif
    
    return result;
}

AtomicWriteResult atomic_create_directory(const std::string& path) {
    AtomicWriteResult result;
    
    try {
        fs::create_directories(path);
        
#ifndef _WIN32
        // fsync parent directory
        std::string parent = get_parent_directory(path);
        if (!parent.empty()) {
            fsync_directory(parent);
        }
#endif
        
        result.ok = true;
    } catch (const std::exception& e) {
        result.error = e.what();
    }
    
    return result;
}

AtomicWriteResult atomic_update_symlink(const std::string& link_path, const std::string& target) {
    AtomicWriteResult result;
    
#ifdef _WIN32
    // Windows doesn't have atomic symlink updates
    if (fs::exists(link_path)) {
        fs::remove(link_path);
    }
    
    try {
        fs::create_symlink(target, link_path);
        result.ok = true;
    } catch (const std::exception& e) {
        result.error = e.what();
    }
#else
    // Create symlink with temp name, then rename
    std::string temp_path = make_temp_filename(link_path);
    
    // Create symlink
    if (symlink(target.c_str(), temp_path.c_str()) != 0) {
        result.error = "failed to create symlink: " + std::string(strerror(errno));
        return result;
    }
    
    // Rename to final path
    if (rename(temp_path.c_str(), link_path.c_str()) != 0) {
        unlink(temp_path.c_str());
        result.error = "failed to rename symlink: " + std::string(strerror(errno));
        return result;
    }
    
    // fsync directory
    std::string parent = get_parent_directory(link_path);
    if (!parent.empty()) {
        fsync_directory(parent);
    }
    
    result.ok = true;
#endif
    
    return result;
}

std::string to_portable_path(const std::string& path) {
    std::string result = path;
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}

std::string get_parent_directory(const std::string& path) {
    fs::path p(path);
    return p.parent_path().string();
}

std::string get_filename(const std::string& path) {
    fs::path p(path);
    return p.filename().string();
}

std::string join_path(const std::string& base, const std::string& rel) {
    fs::path p(base);
    p /= rel;
    return to_portable_path(p.string());
}

bool path_exists(const std::string& path) {
    return fs::exists(path);
}

bool is_directory(const std::string& path) {
    return fs::is_directory(path);
}

bool is_regular_file(const std::string& path) {
    return fs::is_regular_file(path);
}

bool is_symlink(const std::string& path) {
    return fs::is_symlink(path);
}

std::optional<std::string> read_symlink(const std::string& path) {
    try {
        return fs::read_symlink(path).string();
    } catch (...) {
        return std::nullopt;
    }
}

std::vector<std::string> list_directory(const std::string& path) {
    std::vector<std::string> entries;
    
    if (!fs::is_directory(path)) return entries;
    
    for (const auto& entry : fs::directory_iterator(path)) {
        entries.push_back(entry.path().filename().string());
    }
    
    return entries;
}

bool create_directories(const std::string& path) {
    try {
        fs::create_directories(path);
        return true;
    } catch (...) {
        return false;
    }
}

bool remove_directory(const std::string& path) {
    try {
        fs::remove_all(path);
        return true;
    } catch (...) {
        return false;
    }
}

bool remove_file(const std::string& path) {
    try {
        return fs::remove(path);
    } catch (...) {
        return false;
    }
}

bool copy_file(const std::string& src, const std::string& dst) {
    try {
        fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
        return true;
    } catch (...) {
        return false;
    }
}

std::optional<std::string> get_env(const std::string& name) {
#ifdef _MSC_VER
    char* val = nullptr;
    size_t len = 0;
    if (_dupenv_s(&val, &len, name.c_str()) == 0 && val != nullptr) {
        std::string result(val);
        free(val);
        return result;
    }
    return std::nullopt;
#else
    const char* val = std::getenv(name.c_str());
    if (val) {
        return std::string(val);
    }
    return std::nullopt;
#endif
}

std::unordered_map<std::string, std::string> get_all_env() {
    std::unordered_map<std::string, std::string> env;
    
#ifdef _WIN32
    char* environ_block = GetEnvironmentStrings();
    if (environ_block) {
        const char* p = environ_block;
        while (*p) {
            std::string entry(p);
            auto eq = entry.find('=');
            if (eq != std::string::npos && eq > 0) {
                env[entry.substr(0, eq)] = entry.substr(eq + 1);
            }
            p += entry.size() + 1;
        }
        FreeEnvironmentStrings(environ_block);
    }
#else
    for (char** ep = environ; *ep; ++ep) {
        std::string entry(*ep);
        auto eq = entry.find('=');
        if (eq != std::string::npos) {
            env[entry.substr(0, eq)] = entry.substr(eq + 1);
        }
    }
#endif
    
    return env;
}

std::string get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    
    std::tm tm_buf;
#ifdef _WIN32
    gmtime_s(&tm_buf, &time_t_now);
#else
    gmtime_r(&time_t_now, &tm_buf);
#endif
    
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    return buf;
}

std::string generate_uuid() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    
    uint64_t a = dis(gen);
    uint64_t b = dis(gen);
    
    // Set version 4 (random) and variant bits
    a = (a & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;  // Version 4
    b = (b & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;  // Variant 1
    
    char buf[37];
    snprintf(buf, sizeof(buf),
             "%08x-%04x-%04x-%04x-%012llx",
             static_cast<uint32_t>(a >> 32),
             static_cast<uint16_t>((a >> 16) & 0xFFFF),
             static_cast<uint16_t>(a & 0xFFFF),
             static_cast<uint16_t>(b >> 48),
             static_cast<unsigned long long>(b & 0xFFFFFFFFFFFFULL));
    
    return buf;
}

} // namespace nah
