#ifndef NID_MACHONIDPATCHER_HPP
#define NID_MACHONIDPATCHER_HPP

#include <nid/IBinaryPatcher.hpp>

namespace Nid {

struct MachHeader64 {
    std::uint32_t magic;
    std::int32_t cputype;
    std::int32_t cpusubtype;
    std::uint32_t filetype;
    std::uint32_t ncmds;
    std::uint32_t sizeofcmds;
    std::uint32_t flags;
    std::uint32_t reserved;
};

struct SegmentCommand64 {
    std::uint32_t cmd;
    std::uint32_t cmdsize;
    char segname[16];
    std::uint64_t vmaddr;
    std::uint64_t vmsize;
    std::uint64_t fileoff;
    std::uint64_t filesize;
    std::int32_t maxprot;
    std::int32_t initprot;
    std::uint32_t nsects;
    std::uint32_t flags;
};

struct SymtabCommand {
    std::uint32_t cmd;
    std::uint32_t cmdsize;
    std::uint32_t symoff;
    std::uint32_t nsyms;
    std::uint32_t stroff;
    std::uint32_t strsize;
};

struct DyldInfoCommand {
    std::uint32_t cmd;
    std::uint32_t cmdsize;
    std::uint32_t rebase_off;
    std::uint32_t rebase_size;
    std::uint32_t bind_off;
    std::uint32_t bind_size;
    std::uint32_t weak_bind_off;
    std::uint32_t weak_bind_size;
    std::uint32_t lazy_bind_off;
    std::uint32_t lazy_bind_size;
    std::uint32_t export_off;
    std::uint32_t export_size;
};

struct DyldExportsTrieCommand {
    std::uint32_t cmd;
    std::uint32_t cmdsize;
    std::uint32_t data_off;
    std::uint32_t data_size;
};

struct Nlist64 {
    std::uint32_t n_strx;
    std::uint8_t n_type;
    std::uint8_t n_sect;
    std::int16_t n_desc;
    std::uint64_t n_value;
};

static_assert(sizeof(MachHeader64) == 32u);
static_assert(sizeof(SegmentCommand64) == 72u);
static_assert(sizeof(SymtabCommand) == 24u);
static_assert(sizeof(DyldInfoCommand) == 48u);
static_assert(sizeof(DyldExportsTrieCommand) == 16u);
static_assert(sizeof(Nlist64) == 16u);

class MachONidPatcher final : public IBinaryPatcher {
public:
    void PatchNids(std::vector<std::uint8_t>& binary, const std::string& libraryName, const std::unordered_set<std::string>& excludedExports) const override;
};

}

#endif
