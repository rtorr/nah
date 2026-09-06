#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include <zlib.h>

namespace nah::archive {

namespace fs = std::filesystem;

inline constexpr std::uintmax_t max_entry_size = 512ULL * 1024 * 1024;
inline constexpr std::uintmax_t max_archive_size = 1024ULL * 1024 * 1024;
inline constexpr std::size_t max_entries = 100000;

struct Result {
    bool ok;
    std::string error;
};

inline bool write_gzip(gzFile out, const void* data, std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    while (size > 0) {
        const auto chunk = static_cast<unsigned int>(std::min<std::size_t>(size, std::numeric_limits<unsigned int>::max()));
        if (gzwrite(out, bytes, chunk) != static_cast<int>(chunk)) return false;
        bytes += chunk;
        size -= chunk;
    }
    return true;
}

inline bool read_gzip(gzFile in, void* data, std::size_t size) {
    auto* bytes = static_cast<unsigned char*>(data);
    while (size > 0) {
        const auto chunk = static_cast<unsigned int>(std::min<std::size_t>(size, std::numeric_limits<unsigned int>::max()));
        const int count = gzread(in, bytes, chunk);
        if (count <= 0) return false;
        bytes += count;
        size -= static_cast<std::size_t>(count);
    }
    return true;
}

inline std::optional<std::uint64_t> parse_octal(const unsigned char* value, std::size_t length) {
    std::uint64_t result = 0;
    std::size_t i = 0;
    while (i < length && (value[i] == ' ' || value[i] == '\0')) ++i;
    for (; i < length && value[i] != '\0' && value[i] != ' '; ++i) {
        if (value[i] < '0' || value[i] > '7') return std::nullopt;
        if (result > (std::numeric_limits<std::uint64_t>::max() >> 3)) return std::nullopt;
        result = (result << 3) + static_cast<std::uint64_t>(value[i] - '0');
    }
    return result;
}

inline bool put_octal(unsigned char* field, std::size_t length, std::uint64_t value) {
    if (length < 2) return false;
    std::string digits;
    do {
        digits.push_back(static_cast<char>('0' + (value & 7)));
        value >>= 3;
    } while (value != 0);
    if (digits.size() + 1 > length) return false;
    std::memset(field, '0', length);
    field[length - 1] = '\0';
    for (std::size_t i = 0; i < digits.size(); ++i) field[length - 2 - i] = static_cast<unsigned char>(digits[i]);
    return true;
}

inline std::optional<std::pair<std::string, std::string>> split_ustar_path(const std::string& path) {
    if (path.size() <= 100) return std::pair<std::string, std::string>{"", path};
    for (std::size_t pos = path.rfind('/'); pos != std::string::npos; pos = pos == 0 ? std::string::npos : path.rfind('/', pos - 1)) {
        if (pos <= 155 && path.size() - pos - 1 <= 100) {
            return std::pair<std::string, std::string>{path.substr(0, pos), path.substr(pos + 1)};
        }
    }
    return std::nullopt;
}

inline Result create(const fs::path& source, const fs::path& output) {
    std::error_code ec;
    const auto source_root = fs::weakly_canonical(source, ec);
    if (ec || !fs::is_directory(source_root, ec)) return {false, "source is not a readable directory"};

    struct Entry { fs::path disk_path; std::string archive_path; bool directory; std::uintmax_t size; unsigned int mode; };
    std::vector<Entry> entries;
    std::uintmax_t total = 0;
    for (fs::recursive_directory_iterator it(source_root, fs::directory_options::none, ec), end; !ec && it != end; it.increment(ec)) {
        const auto status = it->symlink_status(ec);
        if (ec) break;
        if (fs::is_symlink(status)) return {false, "symbolic links are not supported: " + it->path().string()};
        if (!fs::is_directory(status) && !fs::is_regular_file(status)) return {false, "unsupported file type: " + it->path().string()};
        auto relative = fs::relative(it->path(), source_root, ec);
        if (ec) break;
        std::string archive_path = relative.generic_string();
        if (archive_path.empty() || archive_path.find('\\') != std::string::npos || !split_ustar_path(archive_path)) {
            return {false, "path cannot be represented safely in a package: " + archive_path};
        }
        const bool directory = fs::is_directory(status);
        const auto size = directory ? 0 : fs::file_size(it->path(), ec);
        if (ec) break;
        if (size > max_entry_size || total > max_archive_size - size) return {false, "package exceeds the size limit"};
        total += size;
        const auto perms = static_cast<unsigned int>(status.permissions()) & 0777U;
        entries.push_back({it->path(), archive_path, directory, size, perms});
        if (entries.size() > max_entries) return {false, "package contains too many entries"};
    }
    if (ec) return {false, "failed to enumerate source: " + ec.message()};
    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.archive_path < b.archive_path; });

    const auto absolute_output = fs::absolute(output, ec).lexically_normal();
    if (ec) return {false, "invalid output path"};
    auto source_it = source_root.begin();
    auto output_it = absolute_output.begin();
    for (; source_it != source_root.end() && output_it != absolute_output.end() && *source_it == *output_it;
         ++source_it, ++output_it) {}
    if (source_it == source_root.end()) return {false, "output package must be outside the source directory"};
    fs::create_directories(absolute_output.parent_path(), ec);
    if (ec) return {false, "cannot create output directory: " + ec.message()};
    std::random_device random;
    const auto temporary = absolute_output.string() + ".tmp." + std::to_string(random()) + std::to_string(random());
    gzFile out = gzopen(temporary.c_str(), "wb9");
    if (!out) return {false, "cannot create package"};

    auto fail = [&](const std::string& message) {
        gzclose(out);
        fs::remove(temporary, ec);
        return Result{false, message};
    };
    std::array<unsigned char, 512> block{};
    std::array<char, 64 * 1024> buffer{};
    for (const auto& entry : entries) {
        block.fill(0);
        const auto split = split_ustar_path(entry.archive_path);
        std::memcpy(block.data(), split->second.data(), split->second.size());
        std::memcpy(block.data() + 345, split->first.data(), split->first.size());
        if (!put_octal(block.data() + 100, 8, entry.mode ? entry.mode : (entry.directory ? 0755 : 0644)) ||
            !put_octal(block.data() + 108, 8, 0) || !put_octal(block.data() + 116, 8, 0) ||
            !put_octal(block.data() + 124, 12, entry.size) || !put_octal(block.data() + 136, 12, 0)) {
            return fail("package metadata is too large");
        }
        std::memset(block.data() + 148, ' ', 8);
        block[156] = entry.directory ? '5' : '0';
        std::memcpy(block.data() + 257, "ustar", 5);
        std::memcpy(block.data() + 263, "00", 2);
        std::uint64_t checksum = 0;
        for (const auto byte : block) checksum += byte;
        if (!put_octal(block.data() + 148, 7, checksum)) return fail("package checksum overflow");
        block[155] = ' ';
        if (!write_gzip(out, block.data(), block.size())) return fail("failed to write package");
        if (!entry.directory) {
            std::ifstream input(entry.disk_path, std::ios::binary);
            if (!input) return fail("cannot read: " + entry.disk_path.string());
            std::uintmax_t remaining = entry.size;
            while (remaining > 0) {
                const auto count = static_cast<std::streamsize>(std::min<std::uintmax_t>(remaining, buffer.size()));
                input.read(buffer.data(), count);
                if (input.gcount() != count || !write_gzip(out, buffer.data(), static_cast<std::size_t>(count))) return fail("failed to package file");
                remaining -= static_cast<std::uintmax_t>(count);
            }
            const auto padding = static_cast<std::size_t>((512 - (entry.size % 512)) % 512);
            block.fill(0);
            if (padding && !write_gzip(out, block.data(), padding)) return fail("failed to write package padding");
        }
    }
    block.fill(0);
    if (!write_gzip(out, block.data(), block.size()) || !write_gzip(out, block.data(), block.size()) || gzclose(out) != Z_OK) {
        fs::remove(temporary, ec);
        return {false, "failed to finalize package"};
    }
    const auto backup = absolute_output.string() + ".backup." + std::to_string(random());
    const bool had_output = fs::exists(absolute_output, ec);
    if (had_output) fs::rename(absolute_output, backup, ec);
    if (ec) { fs::remove(temporary, ec); return {false, "cannot stage existing output package"}; }
    fs::rename(temporary, absolute_output, ec);
    if (ec) {
        if (had_output) fs::rename(backup, absolute_output, ec);
        fs::remove(temporary, ec);
        return {false, "cannot activate output package"};
    }
    if (had_output) fs::remove(backup, ec);
    return {true, {}};
}

inline Result extract(const fs::path& archive, const fs::path& destination) {
    std::error_code ec;
    const auto root = fs::absolute(destination, ec).lexically_normal();
    if (ec || fs::exists(root, ec)) return {false, "extraction directory must not already exist"};
    fs::create_directories(root, ec);
    if (ec) return {false, "cannot create extraction directory"};
    gzFile input = gzopen(archive.string().c_str(), "rb");
    if (!input) { fs::remove_all(root, ec); return {false, "cannot open package"}; }

    auto fail = [&](const std::string& message) {
        gzclose(input);
        fs::remove_all(root, ec);
        return Result{false, message};
    };
    std::array<unsigned char, 512> header{};
    std::array<char, 64 * 1024> buffer{};
    std::uintmax_t total = 0;
    std::size_t entries = 0;
    while (true) {
        if (!read_gzip(input, header.data(), header.size())) return fail("truncated package header");
        bool zero = true;
        for (const auto byte : header) zero = zero && byte == 0;
        if (zero) {
            int count = 0;
            std::size_t trailing = 0;
            while ((count = gzread(input, buffer.data(), static_cast<unsigned int>(buffer.size()))) > 0) {
                trailing += static_cast<std::size_t>(count);
                if (trailing > 1024 * 1024) return fail("package terminator is oversized");
                for (int i = 0; i < count; ++i) if (buffer[static_cast<std::size_t>(i)] != 0) return fail("data follows the package terminator");
            }
            int gzip_error = Z_OK;
            gzerror(input, &gzip_error);
            if (count < 0 || (gzip_error != Z_OK && gzip_error != Z_STREAM_END)) return fail("invalid gzip stream");
            break;
        }
        if (++entries > max_entries) return fail("package contains too many entries");

        const auto stored_checksum = parse_octal(header.data() + 148, 8);
        if (!stored_checksum) return fail("invalid package checksum");
        std::uint64_t checksum = 0;
        for (std::size_t i = 0; i < header.size(); ++i) checksum += (i >= 148 && i < 156) ? static_cast<unsigned char>(' ') : header[i];
        if (checksum != *stored_checksum) return fail("package checksum mismatch");
        const auto size = parse_octal(header.data() + 124, 12);
        const auto mode = parse_octal(header.data() + 100, 8);
        if (!size || !mode || *size > max_entry_size || total > max_archive_size - *size) return fail("invalid or oversized package entry");
        total += *size;

        const auto field = [](const unsigned char* data, std::size_t length) {
            const auto* end = static_cast<const unsigned char*>(std::memchr(data, '\0', length));
            return std::string(reinterpret_cast<const char*>(data), end ? static_cast<std::size_t>(end - data) : length);
        };
        const auto name = field(header.data(), 100);
        const auto prefix = field(header.data() + 345, 155);
        const auto archive_path = prefix.empty() ? name : prefix + "/" + name;
        fs::path relative(archive_path);
        if (archive_path.empty() || archive_path.find('\\') != std::string::npos || relative.is_absolute() || relative.has_root_name()) return fail("unsafe package path");
        relative = relative.lexically_normal();
        for (const auto& part : relative) if (part == "..") return fail("package path escapes the destination");
        const auto target = (root / relative).lexically_normal();
        auto mismatch = std::mismatch(root.begin(), root.end(), target.begin(), target.end());
        if (mismatch.first != root.end() || target == root) return fail("package path escapes the destination");

        const unsigned char type = header[156];
        if (type == '5') {
            if (*size != 0) return fail("invalid directory entry");
            fs::create_directories(target, ec);
        } else if (type == '0' || type == '\0') {
            fs::create_directories(target.parent_path(), ec);
            if (!ec) {
                std::ofstream output(target, std::ios::binary | std::ios::trunc);
                if (!output) return fail("cannot create extracted file");
                std::uint64_t remaining = *size;
                while (remaining > 0) {
                    const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, buffer.size()));
                    if (!read_gzip(input, buffer.data(), count)) return fail("truncated package entry");
                    output.write(buffer.data(), static_cast<std::streamsize>(count));
                    if (!output) return fail("cannot write extracted file");
                    remaining -= count;
                }
            }
        } else {
            return fail("links and special files are not supported in packages");
        }
        if (ec) return fail("cannot create extracted path: " + ec.message());
        fs::permissions(target, static_cast<fs::perms>(static_cast<unsigned int>(*mode) & 0777U), fs::perm_options::replace, ec);
        if (ec) return fail("cannot set extracted permissions");
        const auto padding = static_cast<std::size_t>((512 - (*size % 512)) % 512);
        if (padding && !read_gzip(input, buffer.data(), padding)) return fail("truncated package padding");
    }
    if (gzclose(input) != Z_OK) { fs::remove_all(root, ec); return {false, "invalid gzip stream"}; }
    return {true, {}};
}

} // namespace nah::archive
