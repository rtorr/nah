/* Durable local package-store mutations. SPDX-License-Identifier: MIT */

#ifndef NAH_STORE_H
#define NAH_STORE_H

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace nah::store {
namespace fs = std::filesystem;

enum class Error {
    none,
    busy,
    invalid_path,
    already_exists,
    invalid_journal,
    io_error,
};

struct Result {
    bool ok = false;
    Error code = Error::io_error;
    std::string message;
};

namespace detail {

inline std::string unique_id() {
    static std::random_device random;
    static std::mt19937_64 generator(random());
    std::uniform_int_distribution<std::uint64_t> distribution;
    char value[33];
    std::snprintf(value, sizeof(value), "%016llx%016llx",
                  static_cast<unsigned long long>(distribution(generator)),
                  static_cast<unsigned long long>(distribution(generator)));
    return value;
}

inline bool safe_relative(const fs::path& path) {
    if (path.empty() || path.is_absolute() || path.has_root_name()) return false;
    for (const auto& part : path.lexically_normal()) {
        if (part == ".." || part == ".") return false;
    }
    return true;
}

inline bool allowed_pair(const fs::path& payload, const fs::path& record) {
    if (!safe_relative(payload) || !safe_relative(record)) return false;
    std::vector<fs::path> payload_parts(payload.begin(), payload.end());
    std::vector<fs::path> record_parts(record.begin(), record.end());
    if (record_parts.size() != 3 || record_parts[0] != "registry" ||
        record_parts[2].extension() != ".json") return false;
    return (payload_parts.size() == 2 && payload_parts[0] == "apps" && record_parts[1] == "apps") ||
           (payload_parts.size() == 3 && payload_parts[0] == "naks" && record_parts[1] == "naks");
}

inline bool sync_file(const fs::path& path, std::string& error) {
#ifdef _WIN32
    HANDLE handle = CreateFileW(path.wstring().c_str(), GENERIC_READ,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE || !FlushFileBuffers(handle)) {
        if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
        error = "cannot persist " + path.string();
        return false;
    }
    CloseHandle(handle);
#else
    const int descriptor = ::open(path.c_str(), O_RDONLY);
    if (descriptor < 0 || ::fsync(descriptor) != 0) {
        if (descriptor >= 0) ::close(descriptor);
        error = "cannot persist " + path.string();
        return false;
    }
    ::close(descriptor);
#endif
    return true;
}

inline bool write_file(const fs::path& path, const std::string& value, std::string& error) {
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output) { error = "cannot write " + path.string(); return false; }
        output << value;
        output.close();
        if (!output) { error = "cannot finish " + path.string(); return false; }
    }
    return sync_file(path, error);
}

inline bool sync_tree(const fs::path& root, std::string& error) {
    std::error_code ec;
    std::vector<fs::path> directories{root};
    for (fs::recursive_directory_iterator it(root, fs::directory_options::none, ec), end;
         !ec && it != end; it.increment(ec)) {
        const auto status = it->symlink_status(ec);
        if (ec) break;
        if (fs::is_regular_file(status) && !sync_file(it->path(), error)) return false;
        if (fs::is_directory(status)) directories.push_back(it->path());
    }
    if (ec) { error = "cannot inspect staged package: " + ec.message(); return false; }
#ifndef _WIN32
    for (auto it = directories.rbegin(); it != directories.rend(); ++it) {
        if (!sync_file(*it, error)) return false;
    }
    return true;
#else
    return true;
#endif
}

inline bool sync_directory(const fs::path& path, std::string& error) {
#ifdef _WIN32
    (void)path;
    (void)error;
    return true;
#else
    return sync_file(path, error);
#endif
}

inline bool rename_durable(const fs::path& from, const fs::path& to, std::string& error) {
#ifdef _WIN32
    if (!MoveFileExW(from.wstring().c_str(), to.wstring().c_str(), MOVEFILE_WRITE_THROUGH)) {
        error = "cannot rename " + from.string() + " to " + to.string();
        return false;
    }
#else
    std::error_code ec;
    fs::rename(from, to, ec);
    if (ec) { error = "cannot rename " + from.string() + ": " + ec.message(); return false; }
    if (!sync_directory(from.parent_path(), error)) return false;
    if (to.parent_path() != from.parent_path() && !sync_directory(to.parent_path(), error)) return false;
#endif
    return true;
}

class Lock {
public:
    explicit Lock(const fs::path& path) {
#ifdef _WIN32
        handle_ = CreateFileW(path.wstring().c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                              nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        acquired_ = handle_ != INVALID_HANDLE_VALUE;
#else
        descriptor_ = ::open(path.c_str(), O_CREAT | O_RDWR, 0600);
        acquired_ = descriptor_ >= 0 && ::flock(descriptor_, LOCK_EX | LOCK_NB) == 0;
#endif
    }

    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;

    ~Lock() {
#ifdef _WIN32
        if (handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_);
#else
        if (descriptor_ >= 0) {
            if (acquired_) ::flock(descriptor_, LOCK_UN);
            ::close(descriptor_);
        }
#endif
    }

    bool acquired() const { return acquired_; }

private:
    bool acquired_ = false;
#ifdef _WIN32
    HANDLE handle_ = INVALID_HANDLE_VALUE;
#else
    int descriptor_ = -1;
#endif
};

} // namespace detail

/**
 * Owns atomic mutations below one NAH root. Manifest interpretation, remote
 * acquisition, trust policy, and process launch remain outside this class.
 */
class Store {
public:
    explicit Store(fs::path root) : root_(fs::absolute(std::move(root)).lexically_normal()) {}

    const fs::path& root() const { return root_; }

    Result initialize() const {
        std::error_code ec;
        for (const auto* path : {"apps", "naks", "host", "registry", "registry/apps", "registry/naks", "staging"}) {
            fs::create_directories(root_ / path, ec);
            if (ec) return failure(Error::io_error, "cannot create NAH root: " + ec.message());
            const auto status = fs::symlink_status(root_ / path, ec);
            if (ec || fs::is_symlink(status) || !fs::is_directory(status)) {
                return failure(Error::invalid_path, "managed store directory is not a real directory: " +
                                                      (root_ / path).string());
            }
        }
        return success();
    }

    Result recover() const {
        const auto initialized = initialize();
        if (!initialized.ok) return initialized;
        detail::Lock lock(root_ / "staging" / "store.lock");
        if (!lock.acquired()) return failure(Error::busy, "another package operation owns this NAH root");
        return recover_locked();
    }

    Result install(const fs::path& source,
                   const fs::path& payload_relative,
                   const fs::path& record_relative,
                   const std::string& record_json,
                   bool force = false) const {
        if (!detail::allowed_pair(payload_relative, record_relative)) {
            return failure(Error::invalid_path, "installation paths are outside the managed store");
        }
        const auto initialized = initialize();
        if (!initialized.ok) return initialized;
        detail::Lock lock(root_ / "staging" / "store.lock");
        if (!lock.acquired()) return failure(Error::busy, "another package operation owns this NAH root");
        const auto recovered = recover_locked();
        if (!recovered.ok) return recovered;

        std::error_code ec;
        if (!fs::is_directory(source, ec)) return failure(Error::io_error, "installation source is not a directory");
        for (fs::recursive_directory_iterator it(source, fs::directory_options::none, ec), end;
             !ec && it != end; it.increment(ec)) {
            const auto status = it->symlink_status(ec);
            if (ec) break;
            if (fs::is_symlink(status) || (!fs::is_directory(status) && !fs::is_regular_file(status))) {
                return failure(Error::invalid_path, "installation source contains an unsupported file: " +
                                                    it->path().string());
            }
        }
        if (ec) return failure(Error::io_error, "cannot inspect installation source: " + ec.message());
        const auto final_path = root_ / payload_relative;
        const auto record_path = root_ / record_relative;
        if (!force && (fs::exists(final_path, ec) || fs::exists(record_path, ec))) {
            return failure(Error::already_exists, "package is already installed; use --force to replace it");
        }

        const auto operation_id = detail::unique_id();
        const auto operation = root_ / "staging" / operation_id;
        const auto staged_payload = operation / "payload";
        const auto staged_record = operation / "record.json";
        fs::create_directories(operation, ec);
        fs::copy(source, staged_payload, fs::copy_options::recursive, ec);
        if (ec) return cleanup_failure(operation, "cannot stage package: " + ec.message());
        std::string error;
        if (!detail::sync_tree(staged_payload, error)) return cleanup_failure(operation, error);
        if (!detail::write_file(staged_record, record_json, error)) return cleanup_failure(operation, error);

        nlohmann::json journal{{"version", 1}, {"kind", "install"}, {"phase", "prepared"},
                               {"operation", operation_id},
                               {"payload", payload_relative.generic_string()},
                               {"record", record_relative.generic_string()}};
        if (!write_journal(journal, error)) return cleanup_failure(operation, error);

        fs::create_directories(final_path.parent_path(), ec);
        if (ec) return rollback_failure(journal, "cannot create payload directory: " + ec.message());
        fs::create_directories(record_path.parent_path(), ec);
        if (ec) return rollback_failure(journal, "cannot create registry directory: " + ec.message());
        if (fs::exists(final_path, ec) && !detail::rename_durable(final_path, operation / "old-payload", error))
            return rollback_failure(journal, error);
        if (ec) return rollback_failure(journal, "cannot inspect existing payload: " + ec.message());
        if (fs::exists(record_path, ec) && !detail::rename_durable(record_path, operation / "old-record.json", error))
            return rollback_failure(journal, error);
        if (ec) return rollback_failure(journal, "cannot inspect existing record: " + ec.message());
        journal["phase"] = "backed_up";
        if (!write_journal(journal, error)) return rollback_failure(journal, error);

        if (!detail::rename_durable(staged_payload, final_path, error)) return rollback_failure(journal, error);
        journal["phase"] = "payload_active";
        if (!write_journal(journal, error)) return rollback_failure(journal, error);

        if (!detail::rename_durable(staged_record, record_path, error)) return rollback_failure(journal, error);
        journal["phase"] = "committed";
        if (!write_journal(journal, error)) return rollback_failure(journal, error);
        return finish_committed(operation);
    }

    Result remove(const fs::path& payload_relative, const fs::path& record_relative) const {
        if (!detail::allowed_pair(payload_relative, record_relative)) {
            return failure(Error::invalid_path, "removal paths are outside the managed store");
        }
        const auto initialized = initialize();
        if (!initialized.ok) return initialized;
        detail::Lock lock(root_ / "staging" / "store.lock");
        if (!lock.acquired()) return failure(Error::busy, "another package operation owns this NAH root");
        const auto recovered = recover_locked();
        if (!recovered.ok) return recovered;

        const auto operation_id = detail::unique_id();
        const auto operation = root_ / "staging" / operation_id;
        const auto final_path = root_ / payload_relative;
        const auto record_path = root_ / record_relative;
        std::error_code ec;
        fs::create_directories(operation, ec);
        if (ec) return failure(Error::io_error, "cannot stage removal: " + ec.message());
        nlohmann::json journal{{"version", 1}, {"kind", "remove"}, {"phase", "prepared"},
                               {"operation", operation_id},
                               {"payload", payload_relative.generic_string()},
                               {"record", record_relative.generic_string()}};
        std::string error;
        if (!write_journal(journal, error)) return cleanup_failure(operation, error);
        if (fs::exists(final_path, ec) && !detail::rename_durable(final_path, operation / "old-payload", error))
            return rollback_failure(journal, error);
        if (ec) return rollback_failure(journal, "cannot inspect installed payload: " + ec.message());
        if (fs::exists(record_path, ec) && !detail::rename_durable(record_path, operation / "old-record.json", error))
            return rollback_failure(journal, error);
        if (ec) return rollback_failure(journal, "cannot inspect registry record: " + ec.message());
        journal["phase"] = "committed";
        if (!write_journal(journal, error)) return rollback_failure(journal, error);
        return finish_committed(operation);
    }

private:
    static Result success() { return {true, Error::none, {}}; }
    static Result failure(Error code, std::string message) { return {false, code, std::move(message)}; }

    fs::path journal_path() const { return root_ / "staging" / "transaction.json"; }

    bool write_journal(const nlohmann::json& journal, std::string& error) const {
        const auto temporary = root_ / "staging" / "transaction.new";
        if (!detail::write_file(temporary, journal.dump() + "\n", error)) return false;
#ifdef _WIN32
        if (!MoveFileExW(temporary.wstring().c_str(), journal_path().wstring().c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            error = "cannot activate transaction journal";
            return false;
        }
#else
        std::error_code ec;
        fs::rename(temporary, journal_path(), ec);
        if (ec) { error = "cannot activate transaction journal: " + ec.message(); return false; }
        if (!detail::sync_file(journal_path().parent_path(), error)) return false;
#endif
        return true;
    }

    Result recover_locked() const {
        std::error_code ec;
        if (!fs::exists(journal_path(), ec)) return success();
        nlohmann::json journal;
        try {
            std::ifstream input(journal_path(), std::ios::binary);
            input >> journal;
        } catch (...) {
            return failure(Error::invalid_journal, "transaction journal is invalid; manual recovery is required");
        }
        if (!journal.is_object() || !journal.contains("version") || !journal["version"].is_number_integer() ||
            journal["version"].get<int>() != 1 ||
            !journal.contains("kind") || !journal["kind"].is_string() ||
            !journal.contains("phase") || !journal["phase"].is_string() ||
            !journal.contains("operation") || !journal["operation"].is_string() ||
            !journal.contains("payload") || !journal["payload"].is_string() ||
            !journal.contains("record") || !journal["record"].is_string()) {
            return failure(Error::invalid_journal, "transaction journal is incomplete; manual recovery is required");
        }
        const std::string kind = journal["kind"].get<std::string>();
        const std::string phase = journal["phase"].get<std::string>();
        const bool valid_phase =
            (kind == "install" && (phase == "prepared" || phase == "backed_up" ||
                                    phase == "payload_active" || phase == "committed")) ||
            (kind == "remove" && (phase == "prepared" || phase == "committed"));
        if (!valid_phase) {
            return failure(Error::invalid_journal, "transaction journal has an unsupported operation or phase");
        }
        const fs::path payload = journal["payload"].get<std::string>();
        const fs::path record = journal["record"].get<std::string>();
        const std::string operation_id = journal["operation"].get<std::string>();
        if (!detail::allowed_pair(payload, record) || operation_id.size() != 32 ||
            operation_id.find_first_not_of("0123456789abcdef") != std::string::npos) {
            return failure(Error::invalid_journal, "transaction journal contains unsafe paths");
        }
        const auto operation = root_ / "staging" / operation_id;
        if (phase == "committed") return finish_committed(operation);
        return rollback(journal);
    }

    Result rollback(const nlohmann::json& journal) const {
        const auto operation = root_ / "staging" / journal["operation"].get<std::string>();
        const auto final_path = root_ / fs::path(journal["payload"].get<std::string>());
        const auto record_path = root_ / fs::path(journal["record"].get<std::string>());
        std::error_code ec;
        if (journal.value("kind", "") == "install") {
            if (!fs::exists(operation / "payload", ec)) fs::remove_all(final_path, ec);
            ec.clear();
            if (!fs::exists(operation / "record.json", ec)) fs::remove(record_path, ec);
        }
        ec.clear();
        std::string error;
        if (fs::exists(operation / "old-payload", ec) &&
            !detail::rename_durable(operation / "old-payload", final_path, error))
            return failure(Error::io_error, error);
        if (ec) return failure(Error::io_error, "cannot inspect previous payload: " + ec.message());
        if (fs::exists(operation / "old-record.json", ec) &&
            !detail::rename_durable(operation / "old-record.json", record_path, error))
            return failure(Error::io_error, error);
        if (ec) return failure(Error::io_error, "cannot inspect previous record: " + ec.message());
        fs::remove_all(operation, ec);
        fs::remove(journal_path(), ec);
        return success();
    }

    Result rollback_failure(const nlohmann::json& journal, const std::string& message) const {
        const auto recovered = rollback(journal);
        return failure(Error::io_error, recovered.ok ? message : message + "; " + recovered.message);
    }

    Result cleanup_failure(const fs::path& operation, const std::string& message) const {
        std::error_code ec;
        fs::remove_all(operation, ec);
        fs::remove(journal_path(), ec);
        return failure(Error::io_error, message);
    }

    Result finish_committed(const fs::path& operation) const {
        std::error_code ec;
        fs::remove_all(operation, ec);
        if (ec) return failure(Error::io_error, "cannot clean committed transaction: " + ec.message());
        fs::remove(journal_path(), ec);
        if (ec) return failure(Error::io_error, "cannot remove transaction journal: " + ec.message());
        return success();
    }

    fs::path root_;
};

} // namespace nah::store

#endif // NAH_STORE_H
