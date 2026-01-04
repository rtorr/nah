#include "nah/packaging.hpp"
#include "nah/platform.hpp"
#include "nah/manifest.hpp"
#include "nah/nak_record.hpp"
#include "nah/install_record.hpp"
#include "nah/nak_selection.hpp"
#include "nah/host_profile.hpp"
#include "nah/warnings.hpp"

#include <algorithm>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <zlib.h>

namespace fs = std::filesystem;

namespace nah {

// ============================================================================
// Tar Format Constants (POSIX ustar)
// ============================================================================

static constexpr size_t TAR_BLOCK_SIZE = 512;
static constexpr size_t TAR_NAME_SIZE = 100;
static constexpr size_t TAR_MODE_SIZE = 8;
static constexpr size_t TAR_UID_SIZE = 8;
static constexpr size_t TAR_GID_SIZE = 8;
static constexpr size_t TAR_SIZE_SIZE = 12;
static constexpr size_t TAR_MTIME_SIZE = 12;
static constexpr size_t TAR_CHKSUM_SIZE = 8;
static constexpr size_t TAR_LINKNAME_SIZE = 100;
static constexpr size_t TAR_MAGIC_SIZE = 6;
static constexpr size_t TAR_VERSION_SIZE = 2;
static constexpr size_t TAR_UNAME_SIZE = 32;
static constexpr size_t TAR_GNAME_SIZE = 32;
static constexpr size_t TAR_PREFIX_SIZE = 155;

// Tar type flags
static constexpr char TAR_REGTYPE = '0';
static constexpr char TAR_DIRTYPE = '5';
static constexpr char TAR_SYMTYPE = '2';
static constexpr char TAR_LNKTYPE = '1';

// ============================================================================
// Tar Header Structure
// ============================================================================

#pragma pack(push, 1)
struct TarHeader {
    char name[TAR_NAME_SIZE];       // 0
    char mode[TAR_MODE_SIZE];       // 100
    char uid[TAR_UID_SIZE];         // 108
    char gid[TAR_GID_SIZE];         // 116
    char size[TAR_SIZE_SIZE];       // 124
    char mtime[TAR_MTIME_SIZE];     // 136
    char chksum[TAR_CHKSUM_SIZE];   // 148
    char typeflag;                   // 156
    char linkname[TAR_LINKNAME_SIZE]; // 157
    char magic[TAR_MAGIC_SIZE];     // 257
    char version[TAR_VERSION_SIZE]; // 263
    char uname[TAR_UNAME_SIZE];     // 265
    char gname[TAR_GNAME_SIZE];     // 297
    char devmajor[8];               // 329
    char devminor[8];               // 337
    char prefix[TAR_PREFIX_SIZE];   // 345
    char padding[12];               // 500
};
#pragma pack(pop)

static_assert(sizeof(TarHeader) == TAR_BLOCK_SIZE, "TarHeader must be 512 bytes");

// ============================================================================
// Helper Functions
// ============================================================================

// Write an octal value with leading zeros into a fixed-size field
static void write_octal(char* dest, size_t size, uint64_t value) {
    // Leave room for null terminator
    size_t digits = size - 1;
    dest[digits] = '\0';
    for (size_t i = digits; i > 0; --i) {
        dest[i - 1] = '0' + (value & 7);
        value >>= 3;
    }
}

// Calculate tar header checksum
static uint32_t calculate_checksum(const TarHeader& header) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&header);
    uint32_t sum = 0;
    
    for (size_t i = 0; i < sizeof(TarHeader); ++i) {
        // Checksum field is treated as spaces during calculation
        if (i >= 148 && i < 156) {
            sum += ' ';
        } else {
            sum += bytes[i];
        }
    }
    
    return sum;
}

// Parse octal value from tar header field
static uint64_t parse_octal(const char* data, size_t size) {
    uint64_t result = 0;
    for (size_t i = 0; i < size && data[i] != '\0' && data[i] != ' '; ++i) {
        if (data[i] >= '0' && data[i] <= '7') {
            result = (result << 3) | static_cast<uint64_t>(data[i] - '0');
        }
    }
    return result;
}

// Create a tar header for an entry
static TarHeader create_tar_header(const TarEntry& entry) {
    TarHeader header;
    std::memset(&header, 0, sizeof(header));
    
    // Split path into prefix and name if needed
    std::string path = entry.path;
    if (entry.type == TarEntryType::Directory && !path.empty() && path.back() != '/') {
        path += '/';
    }
    
    if (path.size() <= TAR_NAME_SIZE - 1) {
        std::strncpy(header.name, path.c_str(), TAR_NAME_SIZE - 1);
    } else {
        // Use prefix for long paths
        size_t split = path.rfind('/', TAR_NAME_SIZE - 2);
        if (split != std::string::npos && split <= TAR_PREFIX_SIZE - 1) {
            std::strncpy(header.prefix, path.substr(0, split).c_str(), TAR_PREFIX_SIZE - 1);
            std::strncpy(header.name, path.substr(split + 1).c_str(), TAR_NAME_SIZE - 1);
        } else {
            // Path too long - truncate (shouldn't happen with reasonable paths)
            std::strncpy(header.name, path.substr(0, TAR_NAME_SIZE - 1).c_str(), TAR_NAME_SIZE - 1);
        }
    }
    
    // Mode: dirs=0755, files=0644/0755
    uint32_t mode;
    if (entry.type == TarEntryType::Directory) {
        mode = 0755;
    } else {
        mode = entry.executable ? 0755 : 0644;
    }
    write_octal(header.mode, TAR_MODE_SIZE, mode);
    
    // UID/GID: 0 (per SPEC deterministic packaging)
    write_octal(header.uid, TAR_UID_SIZE, 0);
    write_octal(header.gid, TAR_GID_SIZE, 0);
    
    // Size: file size or 0 for directories
    if (entry.type == TarEntryType::Directory) {
        write_octal(header.size, TAR_SIZE_SIZE, 0);
    } else {
        write_octal(header.size, TAR_SIZE_SIZE, entry.data.size());
    }
    
    // Mtime: 0 (per SPEC deterministic packaging)
    write_octal(header.mtime, TAR_MTIME_SIZE, 0);
    
    // Type flag
    switch (entry.type) {
        case TarEntryType::Directory:
            header.typeflag = TAR_DIRTYPE;
            break;
        case TarEntryType::RegularFile:
        default:
            header.typeflag = TAR_REGTYPE;
            break;
    }
    
    // USTAR magic and version
    std::memcpy(header.magic, "ustar", 5);
    header.magic[5] = '\0';
    header.version[0] = '0';
    header.version[1] = '0';
    
    // Uname/Gname: empty (per SPEC deterministic packaging)
    // Already zeroed
    
    // Calculate checksum
    uint32_t checksum = calculate_checksum(header);
    
    // Write checksum with special formatting: 6 octal digits + NUL + space
    char chksum_str[8];
    std::snprintf(chksum_str, sizeof(chksum_str), "%06o", checksum);
    std::memcpy(header.chksum, chksum_str, 6);
    header.chksum[6] = '\0';
    header.chksum[7] = ' ';
    
    return header;
}

// ============================================================================
// Gzip Compression
// ============================================================================

// Compress data using gzip with deterministic settings
// Per SPEC: mtime=0, no original filename, OS=255 (unknown)
static std::vector<uint8_t> gzip_compress(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> result;
    
    // Write gzip header manually for determinism
    result.push_back(0x1f);  // Magic 1
    result.push_back(0x8b);  // Magic 2
    result.push_back(0x08);  // Compression method: deflate
    result.push_back(0x00);  // Flags: none (no name, no comment, etc.)
    result.push_back(0x00);  // mtime[0] = 0
    result.push_back(0x00);  // mtime[1] = 0
    result.push_back(0x00);  // mtime[2] = 0
    result.push_back(0x00);  // mtime[3] = 0
    result.push_back(0x00);  // Extra flags
    result.push_back(0xff);  // OS = 255 (unknown) per SPEC
    
    // Compress data using zlib deflate
    z_stream strm;
    std::memset(&strm, 0, sizeof(strm));
    
    // Use raw deflate (negative window bits)
    if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return {};
    }
    
    strm.next_in = const_cast<Bytef*>(data.data());
    strm.avail_in = static_cast<uInt>(data.size());
    
    std::vector<uint8_t> compressed;
    compressed.resize(deflateBound(&strm, data.size()));
    
    strm.next_out = compressed.data();
    strm.avail_out = static_cast<uInt>(compressed.size());
    
    int ret = deflate(&strm, Z_FINISH);
    deflateEnd(&strm);
    
    if (ret != Z_STREAM_END) {
        return {};
    }
    
    compressed.resize(strm.total_out);
    result.insert(result.end(), compressed.begin(), compressed.end());
    
    // Write gzip trailer: CRC32 + original size
    uint32_t crc = static_cast<uint32_t>(crc32(0, data.data(), static_cast<uInt>(data.size())));
    result.push_back(crc & 0xff);
    result.push_back((crc >> 8) & 0xff);
    result.push_back((crc >> 16) & 0xff);
    result.push_back((crc >> 24) & 0xff);
    
    uint32_t size = static_cast<uint32_t>(data.size());
    result.push_back(size & 0xff);
    result.push_back((size >> 8) & 0xff);
    result.push_back((size >> 16) & 0xff);
    result.push_back((size >> 24) & 0xff);
    
    return result;
}

// Decompress gzip data
static std::vector<uint8_t> gzip_decompress(const std::vector<uint8_t>& data) {
    if (data.size() < 18) {
        return {};  // Too small for gzip
    }
    
    // Verify gzip magic
    if (data[0] != 0x1f || data[1] != 0x8b) {
        return {};
    }
    
    // Skip gzip header
    size_t offset = 10;
    uint8_t flags = data[3];
    
    // Skip extra field
    if (flags & 0x04) {
        if (offset + 2 > data.size()) return {};
        uint16_t xlen = static_cast<uint16_t>(data[offset] | (data[offset + 1] << 8));
        offset += 2 + xlen;
    }
    
    // Skip original file name
    if (flags & 0x08) {
        while (offset < data.size() && data[offset] != 0) offset++;
        offset++;
    }
    
    // Skip comment
    if (flags & 0x10) {
        while (offset < data.size() && data[offset] != 0) offset++;
        offset++;
    }
    
    // Skip header CRC
    if (flags & 0x02) {
        offset += 2;
    }
    
    if (offset >= data.size()) {
        return {};
    }
    
    // Get original size from trailer
    uint32_t orig_size = static_cast<uint32_t>(data[data.size() - 4]) |
                         (static_cast<uint32_t>(data[data.size() - 3]) << 8) |
                         (static_cast<uint32_t>(data[data.size() - 2]) << 16) |
                         (static_cast<uint32_t>(data[data.size() - 1]) << 24);
    
    // Decompress
    z_stream strm;
    std::memset(&strm, 0, sizeof(strm));
    
    if (inflateInit2(&strm, -15) != Z_OK) {
        return {};
    }
    
    strm.next_in = const_cast<Bytef*>(data.data() + offset);
    strm.avail_in = static_cast<uInt>(data.size() - offset - 8);  // Exclude trailer
    
    std::vector<uint8_t> result;
    result.resize(orig_size);
    
    strm.next_out = result.data();
    strm.avail_out = static_cast<uInt>(result.size());
    
    int ret = inflate(&strm, Z_FINISH);
    inflateEnd(&strm);
    
    if (ret != Z_STREAM_END) {
        return {};
    }
    
    return result;
}

// ============================================================================
// Entry Sorting for Deterministic Output
// ============================================================================

// Compare function for deterministic entry ordering
// Per SPEC: lexicographic by full path, directories before files within same prefix
static bool compare_entries(const TarEntry& a, const TarEntry& b) {
    // Add trailing slash to directories for comparison
    std::string path_a = a.path;
    std::string path_b = b.path;
    
    if (a.type == TarEntryType::Directory && !path_a.empty() && path_a.back() != '/') {
        path_a += '/';
    }
    if (b.type == TarEntryType::Directory && !path_b.empty() && path_b.back() != '/') {
        path_b += '/';
    }
    
    // Find common prefix
    size_t slash_a = path_a.rfind('/');
    size_t slash_b = path_b.rfind('/');
    
    std::string prefix_a = (slash_a != std::string::npos) ? path_a.substr(0, slash_a + 1) : "";
    std::string prefix_b = (slash_b != std::string::npos) ? path_b.substr(0, slash_b + 1) : "";
    
    // If same prefix, directories come before files
    if (prefix_a == prefix_b) {
        if (a.type == TarEntryType::Directory && b.type != TarEntryType::Directory) {
            return true;
        }
        if (a.type != TarEntryType::Directory && b.type == TarEntryType::Directory) {
            return false;
        }
    }
    
    // Lexicographic ordering
    return path_a < path_b;
}

// ============================================================================
// Public API Implementation
// ============================================================================

PackResult create_deterministic_archive(const std::vector<TarEntry>& entries) {
    PackResult result;
    
    // Check for prohibited entry types
    for (const auto& entry : entries) {
        if (entry.type == TarEntryType::Symlink) {
            result.error = "symlinks are not permitted: " + entry.path;
            return result;
        }
        if (entry.type == TarEntryType::Hardlink) {
            result.error = "hardlinks are not permitted: " + entry.path;
            return result;
        }
        if (entry.type == TarEntryType::Other) {
            result.error = "unsupported entry type: " + entry.path;
            return result;
        }
    }
    
    // Sort entries deterministically
    std::vector<TarEntry> sorted_entries = entries;
    std::sort(sorted_entries.begin(), sorted_entries.end(), compare_entries);
    
    // Build tar archive
    std::vector<uint8_t> tar_data;
    
    for (const auto& entry : sorted_entries) {
        // Write header
        TarHeader header = create_tar_header(entry);
        const uint8_t* header_bytes = reinterpret_cast<const uint8_t*>(&header);
        tar_data.insert(tar_data.end(), header_bytes, header_bytes + TAR_BLOCK_SIZE);
        
        // Write file data (padded to 512-byte boundary)
        if (entry.type == TarEntryType::RegularFile && !entry.data.empty()) {
            tar_data.insert(tar_data.end(), entry.data.begin(), entry.data.end());
            
            // Pad to block boundary
            size_t padding = (TAR_BLOCK_SIZE - (entry.data.size() % TAR_BLOCK_SIZE)) % TAR_BLOCK_SIZE;
            tar_data.insert(tar_data.end(), padding, 0);
        }
    }
    
    // Write two empty blocks to mark end of archive
    tar_data.insert(tar_data.end(), TAR_BLOCK_SIZE * 2, 0);
    
    // Gzip compress with deterministic settings
    result.archive_data = gzip_compress(tar_data);
    if (result.archive_data.empty()) {
        result.error = "gzip compression failed";
        return result;
    }
    
    result.ok = true;
    return result;
}

CollectResult collect_directory_entries(const std::string& dir_path) {
    CollectResult result;
    
    if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
        result.error = "directory not found: " + dir_path;
        return result;
    }
    
    fs::path base_path = fs::path(dir_path);
    
    try {
        for (const auto& entry : fs::recursive_directory_iterator(dir_path)) {
            fs::path rel_path = fs::relative(entry.path(), base_path);
            std::string path_str = rel_path.string();
            
            // Replace backslashes with forward slashes for portability
            std::replace(path_str.begin(), path_str.end(), '\\', '/');
            
            TarEntry tar_entry;
            tar_entry.path = path_str;
            
            if (fs::is_symlink(entry.path())) {
                result.error = "symlinks are not permitted: " + path_str;
                return result;
            }
            
            if (fs::is_directory(entry.path())) {
                tar_entry.type = TarEntryType::Directory;
            } else if (fs::is_regular_file(entry.path())) {
                tar_entry.type = TarEntryType::RegularFile;
                
                // Read file content
                std::ifstream file(entry.path(), std::ios::binary);
                if (!file) {
                    result.error = "failed to read file: " + path_str;
                    return result;
                }
                tar_entry.data = std::vector<uint8_t>(
                    (std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>());
                
                // Check if executable
                auto perms = fs::status(entry.path()).permissions();
                bool has_exec = (perms & fs::perms::owner_exec) != fs::perms::none ||
                               (perms & fs::perms::group_exec) != fs::perms::none ||
                               (perms & fs::perms::others_exec) != fs::perms::none;
                
                // Also check if under bin/ directory
                bool in_bin = path_str.rfind("bin/", 0) == 0 || 
                             path_str.find("/bin/") != std::string::npos;
                
                tar_entry.executable = has_exec || in_bin;
            } else {
                result.error = "unsupported file type: " + path_str;
                return result;
            }
            
            result.entries.push_back(std::move(tar_entry));
        }
    } catch (const fs::filesystem_error& e) {
        result.error = std::string("filesystem error: ") + e.what();
        return result;
    }
    
    result.ok = true;
    return result;
}

PackResult pack_directory(const std::string& dir_path) {
    auto collect_result = collect_directory_entries(dir_path);
    if (!collect_result.ok) {
        PackResult result;
        result.error = collect_result.error;
        return result;
    }
    
    return create_deterministic_archive(collect_result.entries);
}

PathValidation validate_extraction_path(const std::string& entry_path,
                                         const std::string& extraction_root) {
    PathValidation result;
    
    // Reject absolute paths
    if (!entry_path.empty() && (entry_path[0] == '/' || entry_path[0] == '\\')) {
        result.error = "absolute path not allowed: " + entry_path;
        return result;
    }
    
    // Normalize path and check for traversal
    fs::path path(entry_path);
    fs::path normalized;
    
    for (const auto& component : path) {
        std::string comp = component.string();
        if (comp == "..") {
            result.error = "path traversal not allowed: " + entry_path;
            return result;
        }
        if (comp != "." && !comp.empty()) {
            normalized /= comp;
        }
    }
    
    // Verify the path stays within extraction root
    fs::path full_path = fs::path(extraction_root) / normalized;
    fs::path canonical_root = fs::weakly_canonical(extraction_root);
    fs::path canonical_full = fs::weakly_canonical(full_path);
    
    std::string root_str = canonical_root.string();
    std::string full_str = canonical_full.string();
    
    if (full_str.rfind(root_str, 0) != 0) {
        result.error = "path escapes extraction root: " + entry_path;
        return result;
    }
    
    result.safe = true;
    result.normalized_path = normalized.string();
    return result;
}

UnpackResult extract_archive_safe(const std::vector<uint8_t>& archive_data,
                                   const std::string& staging_dir) {
    UnpackResult result;
    
    // Decompress
    std::vector<uint8_t> tar_data = gzip_decompress(archive_data);
    if (tar_data.empty()) {
        result.error = "failed to decompress archive";
        return result;
    }
    
    // Create staging directory
    if (!create_directories(staging_dir)) {
        result.error = "failed to create staging directory";
        return result;
    }
    
    // Parse tar entries
    size_t offset = 0;
    
    while (offset + TAR_BLOCK_SIZE <= tar_data.size()) {
        const TarHeader* header = reinterpret_cast<const TarHeader*>(tar_data.data() + offset);
        
        // Check for end of archive (two empty blocks)
        bool empty = true;
        for (size_t i = 0; i < TAR_BLOCK_SIZE; ++i) {
            if (tar_data[offset + i] != 0) {
                empty = false;
                break;
            }
        }
        if (empty) break;
        
        // Extract path
        std::string path;
        if (header->prefix[0] != '\0') {
            path = std::string(header->prefix, strnlen(header->prefix, TAR_PREFIX_SIZE));
            path += '/';
        }
        path += std::string(header->name, strnlen(header->name, TAR_NAME_SIZE));
        
        // Remove trailing slash from path for validation
        while (!path.empty() && path.back() == '/') {
            path.pop_back();
        }
        
        // Strip leading "./" if present
        if (path.rfind("./", 0) == 0) {
            path = path.substr(2);
        }
        
        if (path.empty()) {
            offset += TAR_BLOCK_SIZE;
            continue;
        }
        
        // Validate path
        auto validation = validate_extraction_path(path, staging_dir);
        if (!validation.safe) {
            // Clean up staging directory
            remove_directory(staging_dir);
            result.error = validation.error;
            return result;
        }
        
        // Check entry type
        char typeflag = header->typeflag;
        if (typeflag == '\0') typeflag = TAR_REGTYPE;  // Old tar compatibility
        
        if (typeflag == TAR_SYMTYPE || typeflag == TAR_LNKTYPE) {
            remove_directory(staging_dir);
            result.error = "symlinks and hardlinks not permitted: " + path;
            return result;
        }
        
        if (typeflag != TAR_REGTYPE && typeflag != TAR_DIRTYPE) {
            remove_directory(staging_dir);
            result.error = "unsupported entry type: " + path;
            return result;
        }
        
        // Get file size
        uint64_t size = parse_octal(header->size, TAR_SIZE_SIZE);
        
        // Get permissions
        uint64_t mode = parse_octal(header->mode, TAR_MODE_SIZE);
        
        std::string full_path = join_path(staging_dir, validation.normalized_path);
        
        if (typeflag == TAR_DIRTYPE) {
            if (!create_directories(full_path)) {
                remove_directory(staging_dir);
                result.error = "failed to create directory: " + path;
                return result;
            }
        } else {
            // Create parent directories
            std::string parent = get_parent_directory(full_path);
            if (!parent.empty() && !create_directories(parent)) {
                remove_directory(staging_dir);
                result.error = "failed to create parent directory for: " + path;
                return result;
            }
            
            // Write file
            offset += TAR_BLOCK_SIZE;
            
            if (offset + size > tar_data.size()) {
                remove_directory(staging_dir);
                result.error = "truncated archive: " + path;
                return result;
            }
            
            std::ofstream file(full_path, std::ios::binary);
            if (!file) {
                remove_directory(staging_dir);
                result.error = "failed to create file: " + path;
                return result;
            }
            
            file.write(reinterpret_cast<const char*>(tar_data.data() + offset), static_cast<std::streamsize>(size));
            file.close();
            
            // Set permissions
            std::error_code ec;
            if ((mode & 0111) != 0) {
                fs::permissions(full_path, fs::perms::owner_all | fs::perms::group_read | 
                               fs::perms::group_exec | fs::perms::others_read | 
                               fs::perms::others_exec, ec);
            } else {
                fs::permissions(full_path, fs::perms::owner_read | fs::perms::owner_write |
                               fs::perms::group_read | fs::perms::others_read, ec);
            }
            
            result.entries.push_back(path);
            
            // Skip to next block boundary
            size_t blocks = (size + TAR_BLOCK_SIZE - 1) / TAR_BLOCK_SIZE;
            offset += blocks * TAR_BLOCK_SIZE;
            continue;
        }
        
        result.entries.push_back(path);
        offset += TAR_BLOCK_SIZE;
    }
    
    result.ok = true;
    return result;
}

UnpackResult extract_archive_safe(const std::string& archive_path,
                                   const std::string& staging_dir) {
    std::ifstream file(archive_path, std::ios::binary);
    if (!file) {
        UnpackResult result;
        result.error = "failed to open archive: " + archive_path;
        return result;
    }
    
    std::vector<uint8_t> data(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    
    return extract_archive_safe(data, staging_dir);
}

// ============================================================================
// NAP Package Operations
// ============================================================================

NapPackageInfo inspect_nap_package(const std::vector<uint8_t>& archive_data) {
    NapPackageInfo result;
    
    // Decompress to inspect contents
    std::vector<uint8_t> tar_data = gzip_decompress(archive_data);
    if (tar_data.empty()) {
        result.error = "failed to decompress archive";
        return result;
    }
    
    // Scan tar entries - collect file data for binaries and manifest
    size_t offset = 0;
    std::vector<uint8_t> manifest_file_data;
    std::vector<std::pair<std::string, std::vector<uint8_t>>> binary_data;  // path -> data
    
    while (offset + TAR_BLOCK_SIZE <= tar_data.size()) {
        const TarHeader* header = reinterpret_cast<const TarHeader*>(tar_data.data() + offset);
        
        // Check for end of archive
        bool empty = true;
        for (size_t i = 0; i < TAR_BLOCK_SIZE; ++i) {
            if (tar_data[offset + i] != 0) {
                empty = false;
                break;
            }
        }
        if (empty) break;
        
        // Extract path
        std::string path;
        if (header->prefix[0] != '\0') {
            path = std::string(header->prefix, strnlen(header->prefix, TAR_PREFIX_SIZE));
            path += '/';
        }
        path += std::string(header->name, strnlen(header->name, TAR_NAME_SIZE));
        
        while (!path.empty() && path.back() == '/') {
            path.pop_back();
        }
        
        // Strip leading "./" if present
        if (path.rfind("./", 0) == 0) {
            path = path.substr(2);
        }
        
        char typeflag = header->typeflag;
        if (typeflag == '\0') typeflag = TAR_REGTYPE;
        
        uint64_t size = parse_octal(header->size, TAR_SIZE_SIZE);
        
        offset += TAR_BLOCK_SIZE;
        
        if (typeflag == TAR_REGTYPE && size > 0) {
            // Categorize entry
            if (path.rfind("bin/", 0) == 0) {
                result.binaries.push_back(path);
                // Extract binary data for embedded manifest detection
                if (offset + size <= tar_data.size()) {
                    auto start = tar_data.begin() + static_cast<std::ptrdiff_t>(offset);
                    auto end = start + static_cast<std::ptrdiff_t>(size);
                    binary_data.emplace_back(path, std::vector<uint8_t>(start, end));
                }
            } else if (path.rfind("lib/", 0) == 0) {
                result.libraries.push_back(path);
            } else if (path.rfind("share/", 0) == 0) {
                result.assets.push_back(path);
            }
            
            // Check for manifest file
            if (path == "manifest.nah" && offset + size <= tar_data.size()) {
                auto start = tar_data.begin() + static_cast<std::ptrdiff_t>(offset);
                auto end = start + static_cast<std::ptrdiff_t>(size);
                manifest_file_data.assign(start, end);
                result.has_manifest_file = true;
            }
            
            // Skip file data
            size_t blocks = (size + TAR_BLOCK_SIZE - 1) / TAR_BLOCK_SIZE;
            offset += blocks * TAR_BLOCK_SIZE;
        }
    }
    
    // Try manifest.nah first (per SPEC, manifest.nah takes precedence)
    if (result.has_manifest_file && !manifest_file_data.empty()) {
        auto manifest_result = parse_manifest(manifest_file_data);
        if (!manifest_result.critical_missing) {
            result.app_id = manifest_result.manifest.id;
            result.app_version = manifest_result.manifest.version;
            result.nak_id = manifest_result.manifest.nak_id;
            if (manifest_result.manifest.nak_version_req) {
                result.nak_version_req = manifest_result.manifest.nak_version_req->selection_key;
            }
            result.entrypoint = manifest_result.manifest.entrypoint_path;
            result.manifest_source = "file:manifest.nah";
            result.ok = true;
        } else {
            result.error = "invalid manifest: " + manifest_result.error;
        }
    } else if (!binary_data.empty()) {
        // Try to find embedded manifest in binaries
        for (const auto& [bin_path, data] : binary_data) {
            auto section_result = read_manifest_section(data);
            if (section_result.ok && !section_result.data.empty()) {
                auto manifest_result = parse_manifest(section_result.data);
                if (!manifest_result.critical_missing) {
                    result.app_id = manifest_result.manifest.id;
                    result.app_version = manifest_result.manifest.version;
                    result.nak_id = manifest_result.manifest.nak_id;
                    if (manifest_result.manifest.nak_version_req) {
                        result.nak_version_req = manifest_result.manifest.nak_version_req->selection_key;
                    }
                    result.entrypoint = manifest_result.manifest.entrypoint_path;
                    result.manifest_source = "embedded:" + bin_path;
                    result.has_embedded_manifest = true;
                    result.ok = true;
                    break;
                }
            }
        }
        
        if (!result.ok) {
            result.error = "no valid manifest found in binaries";
        }
    } else {
        result.error = "no manifest found in package";
    }
    
    return result;
}

NapPackageInfo inspect_nap_package(const std::string& package_path) {
    std::ifstream file(package_path, std::ios::binary);
    if (!file) {
        NapPackageInfo result;
        result.error = "failed to open package: " + package_path;
        return result;
    }
    
    std::vector<uint8_t> data(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    
    return inspect_nap_package(data);
}

PackResult pack_nap(const std::string& dir_path) {
    PackResult result;
    
    // Validate structure
    if (!path_exists(dir_path)) {
        result.error = "directory not found: " + dir_path;
        return result;
    }
    
    // Check for manifest (either embedded in binary or as manifest.nah)
    bool has_manifest = path_exists(join_path(dir_path, "manifest.nah"));
    
    // If no manifest.nah, check for binaries with embedded manifests
    if (!has_manifest) {
        std::string bin_dir = join_path(dir_path, "bin");
        if (is_directory(bin_dir)) {
            for (const auto& entry : list_directory(bin_dir)) {
                std::string bin_path = join_path(bin_dir, entry);
                if (is_regular_file(bin_path)) {
                    auto section_result = read_manifest_section(bin_path);
                    if (section_result.ok) {
                        has_manifest = true;
                        break;
                    }
                }
            }
        }
    }
    
    if (!has_manifest) {
        result.error = "no manifest found (need manifest.nah or embedded manifest in binary)";
        return result;
    }
    
    // Collect and pack
    return pack_directory(dir_path);
}

// ============================================================================
// NAK Pack Operations
// ============================================================================

NakPackInfo inspect_nak_pack(const std::vector<uint8_t>& archive_data) {
    NakPackInfo result;
    
    // Decompress
    std::vector<uint8_t> tar_data = gzip_decompress(archive_data);
    if (tar_data.empty()) {
        result.error = "failed to decompress archive";
        return result;
    }
    
    // Scan for META/nak.toml
    size_t offset = 0;
    std::string nak_toml_content;
    
    while (offset + TAR_BLOCK_SIZE <= tar_data.size()) {
        const TarHeader* header = reinterpret_cast<const TarHeader*>(tar_data.data() + offset);
        
        bool empty = true;
        for (size_t i = 0; i < TAR_BLOCK_SIZE; ++i) {
            if (tar_data[offset + i] != 0) {
                empty = false;
                break;
            }
        }
        if (empty) break;
        
        std::string path;
        if (header->prefix[0] != '\0') {
            path = std::string(header->prefix, strnlen(header->prefix, TAR_PREFIX_SIZE));
            path += '/';
        }
        path += std::string(header->name, strnlen(header->name, TAR_NAME_SIZE));
        
        while (!path.empty() && path.back() == '/') {
            path.pop_back();
        }
        
        // Strip leading "./" if present
        if (path.rfind("./", 0) == 0) {
            path = path.substr(2);
        }
        
        char typeflag = header->typeflag;
        if (typeflag == '\0') typeflag = TAR_REGTYPE;
        
        uint64_t size = parse_octal(header->size, TAR_SIZE_SIZE);
        
        offset += TAR_BLOCK_SIZE;
        
        if (typeflag == TAR_REGTYPE && size > 0) {
            // Categorize entry
            if (path.rfind("resources/", 0) == 0) {
                result.resources.push_back(path);
            } else if (path.rfind("lib/", 0) == 0) {
                result.libraries.push_back(path);
            } else if (path.rfind("bin/", 0) == 0) {
                result.binaries.push_back(path);
            }
            
            // Check for nak.toml
            if (path == "META/nak.toml" && offset + size <= tar_data.size()) {
                nak_toml_content.assign(
                    reinterpret_cast<const char*>(tar_data.data() + offset),
                    size);
            }
            
            size_t blocks = (size + TAR_BLOCK_SIZE - 1) / TAR_BLOCK_SIZE;
            offset += blocks * TAR_BLOCK_SIZE;
        }
    }
    
    if (nak_toml_content.empty()) {
        result.error = "META/nak.toml not found in pack";
        return result;
    }
    
    // Parse nak.toml
    auto pack_result = parse_nak_pack_manifest(nak_toml_content);
    if (!pack_result.ok) {
        result.error = pack_result.error;
        return result;
    }
    
    const auto& pack = pack_result.manifest;
    
    // Validate schema
    if (pack.schema != "nah.nak.pack.v1") {
        result.error = "invalid schema: expected nah.nak.pack.v1, got " + pack.schema;
        return result;
    }
    
    result.schema = pack.schema;
    result.nak_id = pack.nak.id;
    result.nak_version = pack.nak.version;
    result.resource_root = pack.paths.resource_root;
    result.lib_dirs = pack.paths.lib_dirs;
    result.has_loader = pack.loader.present;
    result.loader_exec_path = pack.loader.exec_path;
    result.loader_args_template = pack.loader.args_template;
    result.execution_cwd = pack.execution.cwd;
    
    result.ok = true;
    return result;
}

NakPackInfo inspect_nak_pack(const std::string& pack_path) {
    std::ifstream file(pack_path, std::ios::binary);
    if (!file) {
        NakPackInfo result;
        result.error = "failed to open pack: " + pack_path;
        return result;
    }
    
    std::vector<uint8_t> data(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    
    return inspect_nak_pack(data);
}

PackResult pack_nak(const std::string& dir_path) {
    PackResult result;
    
    // Validate META/nak.toml exists
    std::string nak_toml_path = join_path(dir_path, "META/nak.toml");
    if (!path_exists(nak_toml_path)) {
        result.error = "META/nak.toml not found";
        return result;
    }
    
    // Read and validate nak.toml
    std::ifstream file(nak_toml_path);
    if (!file) {
        result.error = "failed to read META/nak.toml";
        return result;
    }
    
    std::stringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();
    
    auto pack_result = parse_nak_pack_manifest(content);
    if (!pack_result.ok) {
        result.error = "invalid META/nak.toml: " + pack_result.error;
        return result;
    }
    
    if (pack_result.manifest.schema != "nah.nak.pack.v1") {
        result.error = "invalid schema: expected nah.nak.pack.v1";
        return result;
    }
    
    // Collect and pack
    return pack_directory(dir_path);
}

// ============================================================================
// Installation Operations
// ============================================================================

AppInstallResult install_nap_package(const std::string& package_path,
                                      const AppInstallOptions& options) {
    AppInstallResult result;
    
    // Read package
    std::ifstream file(package_path, std::ios::binary);
    if (!file) {
        result.error = "failed to open package: " + package_path;
        return result;
    }
    
    std::vector<uint8_t> archive_data(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    file.close();
    
    // Create staging directory
    std::string staging_dir = join_path(options.nah_root, ".staging-" + generate_uuid());
    
    // Extract to staging
    auto extract_result = extract_archive_safe(archive_data, staging_dir);
    if (!extract_result.ok) {
        result.error = extract_result.error;
        return result;
    }
    
    // Find and parse manifest
    std::string manifest_path = join_path(staging_dir, "manifest.nah");
    std::vector<uint8_t> manifest_data;
    std::string manifest_source;
    
    if (path_exists(manifest_path)) {
        std::ifstream mf(manifest_path, std::ios::binary);
        manifest_data = std::vector<uint8_t>(
            (std::istreambuf_iterator<char>(mf)),
            std::istreambuf_iterator<char>());
        manifest_source = "file:manifest.nah";
    } else {
        // Try to find embedded manifest in binaries
        std::string bin_dir = join_path(staging_dir, "bin");
        if (is_directory(bin_dir)) {
            for (const auto& entry : list_directory(bin_dir)) {
                std::string bin_path = join_path(bin_dir, entry);
                if (is_regular_file(bin_path)) {
                    auto section_result = read_manifest_section(bin_path);
                    if (section_result.ok) {
                        manifest_data = section_result.data;
                        manifest_source = "embedded:" + entry;
                        break;
                    }
                }
            }
        }
    }
    
    if (manifest_data.empty()) {
        remove_directory(staging_dir);
        result.error = "no manifest found in package";
        return result;
    }
    
    auto manifest_result = parse_manifest(manifest_data);
    if (manifest_result.critical_missing) {
        remove_directory(staging_dir);
        result.error = "invalid manifest: " + manifest_result.error;
        return result;
    }
    
    const auto& manifest = manifest_result.manifest;
    
    // Perform NAK selection at install time
    NakPin selected_nak_pin;
    
    if (!manifest.nak_id.empty()) {
        // Load host profile for NAK selection
        // Per SPEC L599-L606: resolve profile in order:
        // 1. Explicit profile name if provided
        // 2. profile.current symlink if exists
        // 3. default.toml
        std::string profile_path;
        if (!options.profile_name.empty()) {
            profile_path = join_path(options.nah_root, 
                "host/profiles/" + options.profile_name + ".toml");
        } else {
            // Check for profile.current symlink
            std::string current_link = join_path(options.nah_root, "host/profile.current");
            std::error_code ec;
            if (std::filesystem::is_symlink(current_link, ec)) {
                auto target = std::filesystem::read_symlink(current_link, ec);
                if (!ec) {
                    // Resolve relative to host/ directory
                    profile_path = join_path(options.nah_root, "host/" + target.string());
                }
            }
            // Fall back to default.toml
            if (profile_path.empty() || !path_exists(profile_path)) {
                profile_path = join_path(options.nah_root, "host/profiles/default.toml");
            }
        }
        
        std::ifstream pf(profile_path);
        if (!pf) {
            remove_directory(staging_dir);
            result.error = "failed to load host profile: " + profile_path;
            return result;
        }
        std::string profile_toml((std::istreambuf_iterator<char>(pf)),
                                  std::istreambuf_iterator<char>());
        auto profile_result = parse_host_profile_full(profile_toml, profile_path);
        if (!profile_result.ok) {
            remove_directory(staging_dir);
            result.error = "invalid host profile: " + profile_result.error;
            return result;
        }
        
        // Scan NAK registry
        auto registry = scan_nak_registry(options.nah_root);
        
        // Perform NAK selection
        WarningCollector warnings;
        auto selection = select_nak_for_install(
            manifest, profile_result.profile, registry, warnings);
        
        if (selection.resolved) {
            selected_nak_pin = selection.pin;
        } else {
            // NAK selection failed - check if this is a hard failure
            if (manifest.nak_id.empty()) {
                // No NAK required, continue without
            } else {
                remove_directory(staging_dir);
                result.error = "NAK selection failed: no matching NAK found for " + manifest.nak_id;
                return result;
            }
        }
    }
    
    // Determine final install location
    std::string app_dir_name = manifest.id + "-" + manifest.version;
    std::string final_dir = join_path(options.nah_root, "apps/" + app_dir_name);
    
    // Check for existing installation
    if (path_exists(final_dir)) {
        if (!options.force) {
            remove_directory(staging_dir);
            result.error = "application already installed: " + app_dir_name;
            return result;
        }
        // Remove existing installation
        remove_directory(final_dir);
    }
    
    // Create apps directory if needed
    create_directories(join_path(options.nah_root, "apps"));
    
    // Atomic rename from staging to final location
    std::error_code ec;
    fs::rename(staging_dir, final_dir, ec);
    if (ec) {
        remove_directory(staging_dir);
        result.error = "failed to install application: " + ec.message();
        return result;
    }
    
    // Fsync parent directory
    // (Platform-specific; simplified here)
    
    // Generate instance ID
    std::string instance_id = generate_uuid();
    
    // Write App Install Record
    std::string record_dir = join_path(options.nah_root, "registry/installs");
    create_directories(record_dir);
    
    // SPEC: registry/installs/<id>-<version>-<instance_id>.toml
    std::string record_path = join_path(record_dir, 
        manifest.id + "-" + manifest.version + "-" + instance_id + ".toml");
    
    std::ostringstream record;
    record << "schema = \"nah.app.install.v1\"\n\n";
    record << "[install]\n";
    record << "installed_at = \"" << get_current_timestamp() << "\"\n";
    record << "instance_id = \"" << instance_id << "\"\n";
    record << "manifest_source = \"" << manifest_source << "\"\n\n";
    record << "[app]\n";
    record << "id = \"" << manifest.id << "\"\n";
    record << "version = \"" << manifest.version << "\"\n\n";
    record << "[nak]\n";
    record << "id = \"" << selected_nak_pin.id << "\"\n";
    record << "version = \"" << selected_nak_pin.version << "\"\n";
    record << "record_ref = \"" << selected_nak_pin.record_ref << "\"\n\n";
    record << "[paths]\n";
    record << "install_root = \"" << final_dir << "\"\n\n";
    record << "[trust]\n";
    record << "state = \"verified\"\n";
    record << "source = \"install\"\n";
    record << "evaluated_at = \"" << get_current_timestamp() << "\"\n";
    
    auto write_result = atomic_write_file(record_path, record.str());
    if (!write_result.ok) {
        // Installation succeeded but record write failed - try to clean up
        remove_directory(final_dir);
        result.error = "failed to write install record: " + write_result.error;
        return result;
    }
    
    result.ok = true;
    result.install_root = final_dir;
    result.record_path = record_path;
    result.instance_id = instance_id;
    result.nak_id = selected_nak_pin.id;
    result.nak_version = selected_nak_pin.version;
    
    return result;
}

NakInstallResult install_nak_pack(const std::string& pack_path,
                                   const NakInstallOptions& options) {
    NakInstallResult result;
    
    // Inspect pack first
    auto pack_info = inspect_nak_pack(pack_path);
    if (!pack_info.ok) {
        result.error = pack_info.error;
        return result;
    }
    
    // Read pack
    std::ifstream file(pack_path, std::ios::binary);
    if (!file) {
        result.error = "failed to open pack: " + pack_path;
        return result;
    }
    
    std::vector<uint8_t> archive_data(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    file.close();
    
    // Create staging directory
    std::string staging_dir = join_path(options.nah_root, ".staging-" + generate_uuid());
    
    // Extract to staging
    auto extract_result = extract_archive_safe(archive_data, staging_dir);
    if (!extract_result.ok) {
        result.error = extract_result.error;
        return result;
    }
    
    // Determine final install location
    std::string final_dir = join_path(options.nah_root, 
        "naks/" + pack_info.nak_id + "/" + pack_info.nak_version);
    
    // Check for existing installation
    if (path_exists(final_dir)) {
        if (!options.force) {
            remove_directory(staging_dir);
            result.error = "NAK already installed: " + pack_info.nak_id + "@" + pack_info.nak_version;
            return result;
        }
        remove_directory(final_dir);
    }
    
    // Create parent directories
    create_directories(get_parent_directory(final_dir));
    
    // Atomic rename
    std::error_code ec;
    fs::rename(staging_dir, final_dir, ec);
    if (ec) {
        remove_directory(staging_dir);
        result.error = "failed to install NAK: " + ec.message();
        return result;
    }
    
    // Resolve absolute paths for NAK Install Record
    std::string abs_resource_root = pack_info.resource_root.empty() ? 
        final_dir : join_path(final_dir, pack_info.resource_root);
    
    std::vector<std::string> abs_lib_dirs;
    for (const auto& lib_dir : pack_info.lib_dirs) {
        abs_lib_dirs.push_back(join_path(final_dir, lib_dir));
    }
    
    std::string abs_loader_path;
    if (pack_info.has_loader && !pack_info.loader_exec_path.empty()) {
        abs_loader_path = join_path(final_dir, pack_info.loader_exec_path);
    }
    
    // Write NAK Install Record
    std::string record_dir = join_path(options.nah_root, "registry/naks");
    create_directories(record_dir);
    
    std::string record_path = join_path(record_dir,
        pack_info.nak_id + "@" + pack_info.nak_version + ".toml");
    
    std::ostringstream record;
    record << "schema = \"nah.nak.install.v1\"\n\n";
    record << "[nak]\n";
    record << "id = \"" << pack_info.nak_id << "\"\n";
    record << "version = \"" << pack_info.nak_version << "\"\n\n";
    record << "[paths]\n";
    record << "root = \"" << final_dir << "\"\n";
    record << "resource_root = \"" << abs_resource_root << "\"\n";
    record << "lib_dirs = [";
    for (size_t i = 0; i < abs_lib_dirs.size(); ++i) {
        if (i > 0) record << ", ";
        record << "\"" << abs_lib_dirs[i] << "\"";
    }
    record << "]\n";
    
    if (pack_info.has_loader) {
        record << "\n[loader]\n";
        record << "exec_path = \"" << abs_loader_path << "\"\n";
        record << "args_template = [";
        for (size_t i = 0; i < pack_info.loader_args_template.size(); ++i) {
            if (i > 0) record << ", ";
            record << "\"" << pack_info.loader_args_template[i] << "\"";
        }
        record << "]\n";
    }
    
    record << "\n[execution]\n";
    record << "cwd = \"" << pack_info.execution_cwd << "\"\n";
    
    auto write_result = atomic_write_file(record_path, record.str());
    if (!write_result.ok) {
        remove_directory(final_dir);
        result.error = "failed to write NAK install record: " + write_result.error;
        return result;
    }
    
    result.ok = true;
    result.install_root = final_dir;
    result.record_path = record_path;
    
    return result;
}

// ============================================================================
// Uninstallation Operations
// ============================================================================

UninstallResult uninstall_app(const std::string& nah_root,
                               const std::string& app_id,
                               const std::string& version) {
    UninstallResult result;
    
    // Find the app install record
    // SPEC: registry/installs/<id>-<version>-<instance_id>.toml
    std::string record_dir = join_path(nah_root, "registry/installs");
    std::string version_to_remove = version;
    std::string record_path;
    
    if (is_directory(record_dir)) {
        std::string prefix = app_id + "-";
        for (const auto& entry : list_directory(record_dir)) {
            if (entry.rfind(prefix, 0) == 0 && entry.size() > 5 &&
                entry.substr(entry.size() - 5) == ".toml") {
                // Filename format: <id>-<version>-<instance_id>.toml
                // Parse to extract version
                std::string without_ext = entry.substr(0, entry.size() - 5);
                size_t first_dash = without_ext.find('-');
                if (first_dash != std::string::npos) {
                    std::string rest = without_ext.substr(first_dash + 1);
                    // rest is <version>-<instance_id>, find last dash for instance_id
                    size_t last_dash = rest.rfind('-');
                    if (last_dash != std::string::npos) {
                        std::string found_version = rest.substr(0, last_dash);
                        if (version.empty() || found_version == version) {
                            version_to_remove = found_version;
                            record_path = join_path(record_dir, entry);
                            break;
                        }
                    }
                }
            }
        }
    }
    
    if (version_to_remove.empty() || record_path.empty()) {
        result.error = "application not found: " + app_id;
        return result;
    }
    
    std::string app_dir = join_path(nah_root, "apps/" + app_id + "-" + version_to_remove);
    
    // Remove app directory
    if (is_directory(app_dir)) {
        if (!remove_directory(app_dir)) {
            result.error = "failed to remove app directory";
            return result;
        }
    }
    
    // Remove install record
    if (path_exists(record_path)) {
        if (!remove_file(record_path)) {
            result.error = "failed to remove install record";
            return result;
        }
    }
    
    result.ok = true;
    return result;
}

UninstallResult uninstall_nak(const std::string& nah_root,
                               const std::string& nak_id,
                               const std::string& version) {
    UninstallResult result;
    
    // Check if any apps reference this NAK version
    // SPEC: registry/installs/<id>-<version>-<instance_id>.toml
    std::string app_record_dir = join_path(nah_root, "registry/installs");
    if (is_directory(app_record_dir)) {
        for (const auto& entry : list_directory(app_record_dir)) {
            if (entry.size() > 5 && entry.substr(entry.size() - 5) == ".toml") {
                std::string record_path = join_path(app_record_dir, entry);
                std::ifstream file(record_path);
                if (file) {
                    std::stringstream ss;
                    ss << file.rdbuf();
                    std::string content = ss.str();
                    
                    auto app_record = parse_app_install_record_full(content, record_path);
                    if (app_record.ok && 
                        app_record.record.nak.id == nak_id &&
                        app_record.record.nak.version == version) {
                        result.error = "NAK is in use by: " + app_record.record.app.id;
                        return result;
                    }
                }
            }
        }
    }
    
    std::string record_path = join_path(nah_root, "registry/naks/" + nak_id + "@" + version + ".toml");
    std::string nak_dir = join_path(nah_root, "naks/" + nak_id + "/" + version);
    
    // Remove NAK directory
    if (is_directory(nak_dir)) {
        if (!remove_directory(nak_dir)) {
            result.error = "failed to remove NAK directory";
            return result;
        }
    }
    
    // Remove install record
    if (path_exists(record_path)) {
        if (!remove_file(record_path)) {
            result.error = "failed to remove NAK install record";
            return result;
        }
    }
    
    // Clean up empty parent directories
    std::string nak_parent = join_path(nah_root, "naks/" + nak_id);
    if (is_directory(nak_parent) && list_directory(nak_parent).empty()) {
        remove_directory(nak_parent);
    }
    
    result.ok = true;
    return result;
}

// ============================================================================
// Verification Operations
// ============================================================================

VerifyResult verify_app(const std::string& nah_root,
                         const std::string& app_id,
                         const std::string& version) {
    VerifyResult result;
    
    // Find the app install record by scanning all .toml files and checking contents
    // SPEC: registry/installs/<id>-<version>-<instance_id>.toml
    // Note: We parse the file contents rather than the filename because
    // all three components (id, version, instance_id) can contain dashes
    std::string record_dir = join_path(nah_root, "registry/installs");
    std::string version_to_verify;
    std::string record_path;
    
    if (is_directory(record_dir)) {
        for (const auto& entry : list_directory(record_dir)) {
            if (entry.size() > 5 && entry.substr(entry.size() - 5) == ".toml") {
                std::string candidate_path = join_path(record_dir, entry);
                std::ifstream file(candidate_path);
                if (file) {
                    std::stringstream ss;
                    ss << file.rdbuf();
                    auto parse_result = parse_app_install_record_full(ss.str(), candidate_path);
                    if (parse_result.ok) {
                        // Check if this record matches the requested app
                        if (parse_result.record.app.id == app_id) {
                            if (version.empty() || parse_result.record.app.version == version) {
                                version_to_verify = parse_result.record.app.version;
                                record_path = candidate_path;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    
    if (version_to_verify.empty() || record_path.empty()) {
        result.error = "application not found: " + app_id;
        return result;
    }
    
    std::string app_dir = join_path(nah_root, "apps/" + app_id + "-" + version_to_verify);
    
    // Check install record exists and is valid
    if (!path_exists(record_path)) {
        result.issues.push_back("install record missing");
    } else {
        std::ifstream file(record_path);
        if (file) {
            std::stringstream ss;
            ss << file.rdbuf();
            auto record_result = parse_app_install_record_full(ss.str(), record_path);
            if (!record_result.ok) {
                result.issues.push_back("install record invalid: " + record_result.error);
            }
        } else {
            result.issues.push_back("install record unreadable");
        }
    }
    
    // Check app directory exists
    if (!is_directory(app_dir)) {
        result.issues.push_back("app directory missing");
        result.structure_valid = false;
    } else {
        result.structure_valid = true;
    }
    
    // Check manifest
    if (result.structure_valid) {
        std::string manifest_path = join_path(app_dir, "manifest.nah");
        bool found_manifest = false;
        
        if (path_exists(manifest_path)) {
            std::ifstream mf(manifest_path, std::ios::binary);
            std::vector<uint8_t> data(
                (std::istreambuf_iterator<char>(mf)),
                std::istreambuf_iterator<char>());
            auto m_result = parse_manifest(data);
            if (m_result.critical_missing) {
                result.issues.push_back("manifest.nah invalid: " + m_result.error);
            } else {
                found_manifest = true;
            }
        }
        
        // Check for embedded manifest in binaries
        if (!found_manifest) {
            std::string bin_dir = join_path(app_dir, "bin");
            if (is_directory(bin_dir)) {
                for (const auto& entry : list_directory(bin_dir)) {
                    std::string bin_path = join_path(bin_dir, entry);
                    if (is_regular_file(bin_path)) {
                        auto section_result = read_manifest_section(bin_path);
                        if (section_result.ok) {
                            auto m_result = parse_manifest(section_result.data);
                            if (!m_result.critical_missing) {
                                found_manifest = true;
                                break;
                            }
                        }
                    }
                }
            }
        }
        
        result.manifest_valid = found_manifest;
        if (!found_manifest) {
            result.issues.push_back("no valid manifest found");
        }
    }
    
    // Check NAK availability
    if (path_exists(record_path)) {
        std::ifstream file(record_path);
        if (file) {
            std::stringstream ss;
            ss << file.rdbuf();
            auto record_result = parse_app_install_record_full(ss.str(), record_path);
            if (record_result.ok && !record_result.record.nak.id.empty()) {
                std::string nak_record = join_path(nah_root, "registry/naks/" + 
                    record_result.record.nak.id + "@" + record_result.record.nak.version + ".toml");
                
                if (path_exists(nak_record)) {
                    result.nak_available = true;
                } else {
                    result.issues.push_back("pinned NAK not found: " + 
                        record_result.record.nak.id + "@" + record_result.record.nak.version);
                }
            }
        }
    }
    
    result.ok = result.issues.empty();
    if (!result.ok && result.error.empty()) {
        result.error = "verification failed with " + std::to_string(result.issues.size()) + " issue(s)";
    }
    
    return result;
}

} // namespace nah
