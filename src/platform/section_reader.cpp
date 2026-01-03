#include "nah/platform.hpp"

#include <cstring>
#include <fstream>
#include <vector>

namespace nah {

namespace {

// ============================================================================
// ELF Format Structures (Linux)
// ============================================================================

#pragma pack(push, 1)

struct Elf64_Ehdr {
    uint8_t e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct Elf64_Shdr {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
};

// ============================================================================
// Mach-O Format Structures (macOS)
// ============================================================================

struct mach_header_64 {
    uint32_t magic;
    uint32_t cputype;
    uint32_t cpusubtype;
    uint32_t filetype;
    uint32_t ncmds;
    uint32_t sizeofcmds;
    uint32_t flags;
    uint32_t reserved;
};

struct load_command {
    uint32_t cmd;
    uint32_t cmdsize;
};

struct segment_command_64 {
    uint32_t cmd;
    uint32_t cmdsize;
    char segname[16];
    uint64_t vmaddr;
    uint64_t vmsize;
    uint64_t fileoff;
    uint64_t filesize;
    uint32_t maxprot;
    uint32_t initprot;
    uint32_t nsects;
    uint32_t flags;
};

struct section_64 {
    char sectname[16];
    char segname[16];
    uint64_t addr;
    uint64_t size;
    uint32_t offset;
    uint32_t align;
    uint32_t reloff;
    uint32_t nreloc;
    uint32_t flags;
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t reserved3;
};

#pragma pack(pop)

constexpr uint32_t MH_MAGIC_64 = 0xfeedfacf;
constexpr uint32_t MH_CIGAM_64 = 0xcffaedfe;
constexpr uint32_t LC_SEGMENT_64 = 0x19;

constexpr uint8_t ELF_MAGIC[4] = {0x7f, 'E', 'L', 'F'};

// Read file contents
std::vector<uint8_t> read_binary_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    
    file.seekg(0, std::ios::end);
    size_t size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
    
    return data;
}

// ============================================================================
// ELF Section Reader
// ============================================================================

SectionReadResult read_elf_section(const std::vector<uint8_t>& data, const char* section_name) {
    SectionReadResult result;
    
    if (data.size() < sizeof(Elf64_Ehdr)) {
        result.error = "file too small for ELF header";
        return result;
    }
    
    // Check magic
    if (std::memcmp(data.data(), ELF_MAGIC, 4) != 0) {
        result.error = "not an ELF file";
        return result;
    }
    
    // Check 64-bit
    if (data[4] != 2) {
        result.error = "not a 64-bit ELF file";
        return result;
    }
    
    const auto* ehdr = reinterpret_cast<const Elf64_Ehdr*>(data.data());
    
    if (ehdr->e_shoff == 0 || ehdr->e_shnum == 0) {
        result.error = "no section headers";
        return result;
    }
    
    if (ehdr->e_shoff + ehdr->e_shnum * sizeof(Elf64_Shdr) > data.size()) {
        result.error = "section headers out of bounds";
        return result;
    }
    
    const auto* sections = reinterpret_cast<const Elf64_Shdr*>(data.data() + ehdr->e_shoff);
    
    // Get string table
    if (ehdr->e_shstrndx >= ehdr->e_shnum) {
        result.error = "invalid string table index";
        return result;
    }
    
    const auto& strtab_hdr = sections[ehdr->e_shstrndx];
    if (strtab_hdr.sh_offset + strtab_hdr.sh_size > data.size()) {
        result.error = "string table out of bounds";
        return result;
    }
    
    const char* strtab = reinterpret_cast<const char*>(data.data() + strtab_hdr.sh_offset);
    
    // Find the section
    for (uint16_t i = 0; i < ehdr->e_shnum; ++i) {
        const auto& shdr = sections[i];
        if (shdr.sh_name >= strtab_hdr.sh_size) continue;
        
        const char* name = strtab + shdr.sh_name;
        if (std::strcmp(name, section_name) == 0) {
            if (shdr.sh_offset + shdr.sh_size > data.size()) {
                result.error = "section data out of bounds";
                return result;
            }
            
            result.ok = true;
            result.data.assign(data.begin() + static_cast<ptrdiff_t>(shdr.sh_offset),
                              data.begin() + static_cast<ptrdiff_t>(shdr.sh_offset + shdr.sh_size));
            return result;
        }
    }
    
    result.error = "section not found";
    return result;
}

// ============================================================================
// Mach-O Section Reader
// ============================================================================

SectionReadResult read_macho_section(const std::vector<uint8_t>& data, 
                                      const char* segment_name, 
                                      const char* section_name) {
    SectionReadResult result;
    
    if (data.size() < sizeof(mach_header_64)) {
        result.error = "file too small for Mach-O header";
        return result;
    }
    
    const auto* header = reinterpret_cast<const mach_header_64*>(data.data());
    
    bool swap_bytes = false;
    if (header->magic == MH_MAGIC_64) {
        swap_bytes = false;
    } else if (header->magic == MH_CIGAM_64) {
        swap_bytes = true;
    } else {
        result.error = "not a 64-bit Mach-O file";
        return result;
    }
    
    uint32_t ncmds = swap_bytes ? __builtin_bswap32(header->ncmds) : header->ncmds;
    
    size_t offset = sizeof(mach_header_64);
    
    for (uint32_t i = 0; i < ncmds; ++i) {
        if (offset + sizeof(load_command) > data.size()) {
            result.error = "load command out of bounds";
            return result;
        }
        
        const auto* lc = reinterpret_cast<const load_command*>(data.data() + offset);
        uint32_t cmd = swap_bytes ? __builtin_bswap32(lc->cmd) : lc->cmd;
        uint32_t cmdsize = swap_bytes ? __builtin_bswap32(lc->cmdsize) : lc->cmdsize;
        
        if (cmd == LC_SEGMENT_64) {
            if (offset + sizeof(segment_command_64) > data.size()) {
                result.error = "segment command out of bounds";
                return result;
            }
            
            const auto* seg = reinterpret_cast<const segment_command_64*>(data.data() + offset);
            
            if (std::strncmp(seg->segname, segment_name, 16) == 0) {
                uint32_t nsects = swap_bytes ? __builtin_bswap32(seg->nsects) : seg->nsects;
                
                size_t sect_offset = offset + sizeof(segment_command_64);
                
                for (uint32_t j = 0; j < nsects; ++j) {
                    if (sect_offset + sizeof(section_64) > data.size()) {
                        result.error = "section header out of bounds";
                        return result;
                    }
                    
                    const auto* sect = reinterpret_cast<const section_64*>(data.data() + sect_offset);
                    
                    if (std::strncmp(sect->sectname, section_name, 16) == 0) {
                        uint32_t sect_offset_val = swap_bytes ? __builtin_bswap32(sect->offset) : sect->offset;
                        uint64_t sect_size = swap_bytes ? __builtin_bswap64(sect->size) : sect->size;
                        
                        if (sect_offset_val + sect_size > data.size()) {
                            result.error = "section data out of bounds";
                            return result;
                        }
                        
                        result.ok = true;
                        result.data.assign(data.begin() + static_cast<ptrdiff_t>(sect_offset_val),
                                          data.begin() + static_cast<ptrdiff_t>(sect_offset_val + sect_size));
                        return result;
                    }
                    
                    sect_offset += sizeof(section_64);
                }
            }
        }
        
        offset += cmdsize;
    }
    
    result.error = "section not found";
    return result;
}

} // namespace

// Internal implementation that works on binary data
SectionReadResult read_manifest_section_impl(const std::vector<uint8_t>& data) {
    SectionReadResult result;
    
    if (data.empty()) {
        result.error = "empty binary data";
        return result;
    }
    
    // Detect format and read appropriate section
    if (data.size() >= 4) {
        // Check for ELF
        if (std::memcmp(data.data(), ELF_MAGIC, 4) == 0) {
            return read_elf_section(data, ".nah_manifest");
        }
        
        // Check for Mach-O
        uint32_t magic = *reinterpret_cast<const uint32_t*>(data.data());
        if (magic == MH_MAGIC_64 || magic == MH_CIGAM_64) {
            return read_macho_section(data, "__NAH", "__manifest");
        }
    }
    
    result.error = "unknown binary format";
    return result;
}

SectionReadResult read_manifest_section(const std::string& binary_path) {
    std::vector<uint8_t> data = read_binary_file(binary_path);
    if (data.empty()) {
        SectionReadResult result;
        result.error = "failed to read file";
        return result;
    }
    
    return read_manifest_section_impl(data);
}

SectionReadResult read_manifest_section(const std::vector<uint8_t>& binary_data) {
    return read_manifest_section_impl(binary_data);
}

} // namespace nah
