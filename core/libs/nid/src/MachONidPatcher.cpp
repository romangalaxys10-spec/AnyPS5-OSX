#include <nid/MachONidPatcher.hpp>
#include <nid/NidResolver.hpp>
#include <nid/NidPatcherUtils.hpp>
#include <nid/NidCompute.hpp>
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Nid {

namespace {

constexpr std::uint32_t kMachMagic64 = 0xFEEDFACFu;
constexpr std::uint32_t kLcSegment64 = 0x19u;
constexpr std::uint32_t kLcSymtab = 0x2u;
constexpr std::uint32_t kLcDyldInfo = 0x22u;
constexpr std::uint32_t kLcDyldInfoOnly = 0x80000022u;
constexpr std::uint32_t kLcDyldExportsTrie = 0x80000033u;
constexpr std::uint32_t kLcDyldChainedFixups = 0x80000034u;
constexpr std::uint8_t kNExt = 0x01u;
constexpr std::uint8_t kNPext = 0x10u;
constexpr std::uint8_t kNStab = 0xE0u;
constexpr std::size_t kNlistSize = 16u;
constexpr std::string_view kLinkeditName = "__LINKEDIT";
constexpr std::uint8_t kBindOpcodeSetSymbolTrailingFlagsImm = 0x40u;

bool HasScePrefix(const std::string& name) {
    return name.size() >= 3u &&
        std::tolower(static_cast<unsigned char>(name[0])) == 's' &&
        std::tolower(static_cast<unsigned char>(name[1])) == 'c' &&
        std::tolower(static_cast<unsigned char>(name[2])) == 'e';
}

bool HasNidPostfix(const std::string& name) {
    using namespace Internal;
    return name.size() >= kNidPostfixLen &&
        name.compare(name.size() - kNidPostfixLen, kNidPostfixLen, kNidPostfix) == 0;
}

std::string ResolveImportName(const std::string& name) {
    if (HasNidPostfix(name) || HasScePrefix(name)) return ResolveOneName(name);
    return name;
}

std::uint64_t ReadUleb(const std::vector<std::uint8_t>& data, std::size_t& offset) {
    std::uint64_t result = 0u;
    unsigned shift = 0u;
    for (unsigned i = 0u; i < 10u; ++i) {
        if (offset >= data.size()) throw std::runtime_error("uleb read out of bounds");
        const auto byte = data[offset++];
        if (shift == 63u && (byte & 0x7Eu) != 0u) throw std::runtime_error("uleb overflow");
        result |= static_cast<std::uint64_t>(byte & 0x7Fu) << shift;
        if ((byte & 0x80u) == 0u) return result;
        shift += 7u;
    }
    throw std::runtime_error("uleb overflow");
}

void WriteUleb(std::vector<std::uint8_t>& out, std::uint64_t value) {
    while (true) {
        auto byte = static_cast<std::uint8_t>(value & 0x7Fu);
        value >>= 7u;
        if (value != 0u) byte |= 0x80u;
        out.push_back(byte);
        if (value == 0u) return;
    }
}

std::size_t UlebSize(std::uint64_t value) {
    std::size_t size = 1u;
    while (value >= 0x80u) {
        value >>= 7u;
        ++size;
    }
    return size;
}

struct TrieNode {
    std::string edge;
    std::vector<std::uint8_t> terminalData;
    std::vector<TrieNode> children;
};

TrieNode ParseTrieNode(const std::vector<std::uint8_t>& blob, std::size_t offset, std::unordered_set<std::size_t>& visited) {
    if (!visited.insert(offset).second) throw std::runtime_error("export trie contains a cycle");
    if (offset >= blob.size()) throw std::runtime_error("export trie node out of bounds");

    TrieNode node;
    std::size_t cursor = offset;
    const auto terminalSize = ReadUleb(blob, cursor);
    if (terminalSize > blob.size() - cursor) throw std::runtime_error("export trie terminal out of bounds");
    node.terminalData.assign(
        blob.begin() + static_cast<std::ptrdiff_t>(cursor),
        blob.begin() + static_cast<std::ptrdiff_t>(cursor + terminalSize));
    cursor += static_cast<std::size_t>(terminalSize);

    const auto childCount = ReadUleb(blob, cursor);
    for (std::uint64_t i = 0u; i < childCount; ++i) {
        std::string edge;
        while (true) {
            if (cursor >= blob.size()) throw std::runtime_error("export trie edge not terminated");
            const auto byte = blob[cursor++];
            if (byte == 0u) break;
            edge.push_back(static_cast<char>(byte));
        }
        const auto childOffset = ReadUleb(blob, cursor);
        auto child = ParseTrieNode(blob, static_cast<std::size_t>(childOffset), visited);
        child.edge = std::move(edge);
        node.children.push_back(std::move(child));
    }
    return node;
}

void CollectTerminals(const TrieNode& node, std::string& prefix, std::vector<std::pair<std::string, std::vector<std::uint8_t>>>& terminals) {
    if (!node.terminalData.empty())
        terminals.emplace_back(prefix, node.terminalData);
    for (const auto& child : node.children) {
        const auto savedSize = prefix.size();
        prefix += child.edge;
        CollectTerminals(child, prefix, terminals);
        prefix.resize(savedSize);
    }
}

TrieNode BuildTrieLevel(std::vector<std::pair<std::string, std::vector<std::uint8_t>>>& entries) {
    std::size_t common = entries[0].first.size();
    for (std::size_t i = 1u; i < entries.size(); ++i) {
        std::size_t j = 0u;
        while (j < common && j < entries[i].first.size() && entries[0].first[j] == entries[i].first[j]) ++j;
        common = j;
    }

    TrieNode node;
    node.edge = entries[0].first.substr(0u, common);
    for (auto& entry : entries)
        entry.first.erase(0u, common);

    std::size_t i = 0u;
    while (i < entries.size()) {
        if (entries[i].first.empty()) {
            node.terminalData = std::move(entries[i].second);
            ++i;
            continue;
        }
        std::size_t j = i + 1u;
        while (j < entries.size() && !entries[j].first.empty() && entries[j].first[0] == entries[i].first[0]) ++j;
        std::vector<std::pair<std::string, std::vector<std::uint8_t>>> group(
            std::make_move_iterator(entries.begin() + static_cast<std::ptrdiff_t>(i)),
            std::make_move_iterator(entries.begin() + static_cast<std::ptrdiff_t>(j)));
        node.children.push_back(BuildTrieLevel(group));
        i = j;
    }
    return node;
}

TrieNode BuildTrie(std::vector<std::pair<std::string, std::vector<std::uint8_t>>> terminals) {
    TrieNode root;
    std::size_t i = 0u;
    while (i < terminals.size()) {
        std::size_t j = i + 1u;
        while (j < terminals.size() && !terminals[j].first.empty() && terminals[j].first[0] == terminals[i].first[0]) ++j;
        std::vector<std::pair<std::string, std::vector<std::uint8_t>>> group(
            std::make_move_iterator(terminals.begin() + static_cast<std::ptrdiff_t>(i)),
            std::make_move_iterator(terminals.begin() + static_cast<std::ptrdiff_t>(j)));
        root.children.push_back(BuildTrieLevel(group));
        i = j;
    }
    return root;
}

void EmitTrieNode(const TrieNode& node, std::vector<std::uint8_t>& out) {
    WriteUleb(out, node.terminalData.size());
    out.insert(out.end(), node.terminalData.begin(), node.terminalData.end());
    WriteUleb(out, node.children.size());
    for (const auto& child : node.children) {
        out.insert(out.end(), child.edge.begin(), child.edge.end());
        out.push_back(0u);
        std::uint64_t childOffset = 0u;
        for (std::size_t length = 1u; length <= 5u; ++length) {
            const auto candidate = static_cast<std::uint64_t>(out.size()) + length;
            if (UlebSize(candidate) == length) {
                childOffset = candidate;
                break;
            }
        }
        if (childOffset == 0u) throw std::runtime_error("export trie offset overflow");
        WriteUleb(out, childOffset);
        EmitTrieNode(child, out);
    }
}

void VerifyTrie(const std::vector<std::uint8_t>& blob, const std::vector<std::pair<std::string, std::vector<std::uint8_t>>>& expected) {
    std::unordered_set<std::size_t> visited;
    const auto root = ParseTrieNode(blob, 0u, visited);
    std::string prefix;
    std::vector<std::pair<std::string, std::vector<std::uint8_t>>> actual;
    CollectTerminals(root, prefix, actual);
    if (actual.size() != expected.size()) throw std::runtime_error("export trie rebuild verification failed");
    for (std::size_t i = 0u; i < expected.size(); ++i) {
        if (actual[i].first != expected[i].first || actual[i].second != expected[i].second)
            throw std::runtime_error("export trie rebuild verification failed");
    }
}

std::string_view SegmentName(const SegmentCommand64& segment) {
    std::string_view name(segment.segname, sizeof(segment.segname));
    const auto terminator = name.find('\0');
    if (terminator != std::string_view::npos)
        name.remove_suffix(name.size() - terminator);
    return name;
}

struct SegmentRef {
    std::size_t commandOffset;
    SegmentCommand64 command;
};

struct TrieLocation {
    std::size_t commandOffset;
    std::uint32_t dataOffset;
    std::uint32_t dataSize;
    bool exportsTrieCommand;
};

struct SymbolRef {
    std::size_t nlistOffset;
    std::uint32_t strx;
    std::string name;
    bool defined;
};

struct BindRegionSpec {
    std::uint32_t offset;
    std::uint32_t size;
    std::size_t offsetFieldOffset;
    std::size_t sizeFieldOffset;
};

std::size_t AppendRegion(std::vector<std::uint8_t>& macho, const SegmentRef& linkedit, const std::vector<std::uint8_t>& data) {
    if (linkedit.commandOffset == 0u) throw std::runtime_error("no __LINKEDIT segment");

    auto offset = macho.size();
    offset = (offset + 7u) & ~static_cast<std::size_t>(7u);
    macho.resize(offset, 0u);
    macho.insert(macho.end(), data.begin(), data.end());

    const auto current = Internal::Read<SegmentCommand64>(macho, linkedit.commandOffset);
    const std::uint64_t regionEnd = static_cast<std::uint64_t>(offset) + data.size();
    if (regionEnd > current.fileoff + current.filesize) {
        auto updated = current;
        updated.filesize = regionEnd - current.fileoff;
        updated.vmsize = updated.filesize;
        Internal::Write(macho, linkedit.commandOffset, updated);
    }
    return offset;
}

void RewriteSymbolTable(
    std::vector<std::uint8_t>& macho,
    std::size_t symtabCommandOffset,
    const SegmentRef& linkedit,
    const std::unordered_map<std::string, std::string>& nidMap,
    const std::vector<std::uint8_t>& oldStrTab,
    const std::vector<SymbolRef>& externals
) {
    using namespace Internal;

    std::vector<std::uint8_t> appended;
    std::unordered_map<std::string, std::uint32_t> appendedOffsets;
    bool changed = false;
    const std::size_t appendBase = oldStrTab.empty() ? 1u : oldStrTab.size();
    for (const auto& symbol : externals) {
        std::string newName = symbol.name;
        if (symbol.defined) {
            const auto mapped = nidMap.find(symbol.name);
            if (mapped != nidMap.end()) newName = mapped->second;
        } else {
            newName = ResolveImportName(symbol.name);
        }
        if (newName == symbol.name) continue;
        changed = true;

        auto slot = appendedOffsets.find(newName);
        if (slot == appendedOffsets.end()) {
            const auto offset = static_cast<std::uint32_t>(appendBase + appended.size());
            appended.insert(appended.end(), newName.begin(), newName.end());
            appended.push_back(0u);
            slot = appendedOffsets.emplace(newName, offset).first;
        }
        Write(macho, symbol.nlistOffset, slot->second);
    }
    if (!changed) return;

    if (appendBase + appended.size() > std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("rebuilt string table exceeds 32-bit offsets");

    std::vector<std::uint8_t> newStrTab(oldStrTab);
    if (oldStrTab.empty()) newStrTab.push_back(0u);
    newStrTab.insert(newStrTab.end(), appended.begin(), appended.end());

    const auto appendedOffset = AppendRegion(macho, linkedit, newStrTab);
    if (appendedOffset > std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("appended string table exceeds 32-bit file offsets");

    auto command = Read<SymtabCommand>(macho, symtabCommandOffset);
    command.stroff = static_cast<std::uint32_t>(appendedOffset);
    command.strsize = static_cast<std::uint32_t>(newStrTab.size());
    Write(macho, symtabCommandOffset, command);
}

void RewriteExportTrie(
    std::vector<std::uint8_t>& macho,
    const SegmentRef& linkedit,
    const TrieLocation& trie,
    const std::unordered_map<std::string, std::string>& nidMap,
    std::vector<std::pair<std::string, std::vector<std::uint8_t>>> terminals
) {
    using namespace Internal;

    if (trie.dataSize == 0u || terminals.empty()) return;

    bool changed = false;
    std::unordered_set<std::string> uniqueNames;
    for (auto& terminal : terminals) {
        const auto mapped = nidMap.find(terminal.first);
        if (mapped != nidMap.end() && mapped->second != terminal.first) {
            terminal.first = mapped->second;
            changed = true;
        }
        if (!uniqueNames.insert(terminal.first).second)
            throw std::runtime_error("duplicate export name after NID patching: " + terminal.first);
    }
    if (!changed) return;

    std::sort(terminals.begin(), terminals.end(), [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
    const auto expected = terminals;
    const auto rebuilt = BuildTrie(std::move(terminals));

    std::vector<std::uint8_t> newBlob;
    EmitTrieNode(rebuilt, newBlob);
    VerifyTrie(newBlob, expected);

    if (newBlob.size() <= trie.dataSize) {
        std::memcpy(macho.data() + trie.dataOffset, newBlob.data(), newBlob.size());
        std::memset(macho.data() + trie.dataOffset + newBlob.size(), 0u, trie.dataSize - newBlob.size());
        if (trie.exportsTrieCommand) {
            auto command = Read<DyldExportsTrieCommand>(macho, trie.commandOffset);
            command.data_size = static_cast<std::uint32_t>(newBlob.size());
            Write(macho, trie.commandOffset, command);
        } else {
            auto command = Read<DyldInfoCommand>(macho, trie.commandOffset);
            command.export_size = static_cast<std::uint32_t>(newBlob.size());
            Write(macho, trie.commandOffset, command);
        }
        return;
    }

    const auto appendedOffset = AppendRegion(macho, linkedit, newBlob);
    if (appendedOffset > std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("appended export trie exceeds 32-bit file offsets");
    if (trie.exportsTrieCommand) {
        auto command = Read<DyldExportsTrieCommand>(macho, trie.commandOffset);
        command.data_off = static_cast<std::uint32_t>(appendedOffset);
        command.data_size = static_cast<std::uint32_t>(newBlob.size());
        Write(macho, trie.commandOffset, command);
    } else {
        auto command = Read<DyldInfoCommand>(macho, trie.commandOffset);
        command.export_off = static_cast<std::uint32_t>(appendedOffset);
        command.export_size = static_cast<std::uint32_t>(newBlob.size());
        Write(macho, trie.commandOffset, command);
    }
}

std::string ReadBindName(const std::vector<std::uint8_t>& stream, std::size_t& cursor) {
    std::string name;
    while (true) {
        if (cursor >= stream.size()) throw std::runtime_error("bind symbol name not terminated");
        const auto byte = stream[cursor++];
        if (byte == 0u) return name;
        name.push_back(static_cast<char>(byte));
    }
}

void CopyLebOperand(const std::vector<std::uint8_t>& stream, std::size_t& cursor, std::vector<std::uint8_t>& out) {
    while (true) {
        if (cursor >= stream.size()) throw std::runtime_error("bind operand not terminated");
        const auto byte = stream[cursor++];
        out.push_back(byte);
        if ((byte & 0x80u) == 0u) return;
    }
}

std::vector<std::uint8_t> RewriteBindStream(const std::vector<std::uint8_t>& stream) {
    std::vector<std::uint8_t> result;
    result.reserve(stream.size());
    std::size_t cursor = 0u;
    while (cursor < stream.size()) {
        const auto opcode = stream[cursor++];
        result.push_back(opcode);
        switch (opcode & 0xF0u) {
        case kBindOpcodeSetSymbolTrailingFlagsImm: {
            const auto name = ReadBindName(stream, cursor);
            const auto newName = ResolveImportName(name);
            result.insert(result.end(), newName.begin(), newName.end());
            result.push_back(0u);
            break;
        }
        case 0x20u:
        case 0x60u:
        case 0x70u:
        case 0x80u:
        case 0xA0u:
            CopyLebOperand(stream, cursor, result);
            break;
        case 0xC0u:
            CopyLebOperand(stream, cursor, result);
            CopyLebOperand(stream, cursor, result);
            break;
        case 0x00u:
        case 0x10u:
        case 0x30u:
        case 0x50u:
        case 0x90u:
        case 0xB0u:
            break;
        default:
            throw std::runtime_error("unknown bind opcode in dyld info");
        }
    }
    return result;
}

void RewriteBindStreams(std::vector<std::uint8_t>& macho, std::size_t dyldInfoCommandOffset, const SegmentRef& linkedit) {
    using namespace Internal;

    const auto command = Read<DyldInfoCommand>(macho, dyldInfoCommandOffset);
    const BindRegionSpec regions[3] = {
        {command.bind_off, command.bind_size, offsetof(DyldInfoCommand, bind_off), offsetof(DyldInfoCommand, bind_size)},
        {command.weak_bind_off, command.weak_bind_size, offsetof(DyldInfoCommand, weak_bind_off), offsetof(DyldInfoCommand, weak_bind_size)},
        {command.lazy_bind_off, command.lazy_bind_size, offsetof(DyldInfoCommand, lazy_bind_off), offsetof(DyldInfoCommand, lazy_bind_size)},
    };

    for (const auto& region : regions) {
        if (region.size == 0u) continue;
        if (static_cast<std::size_t>(region.offset) + region.size > macho.size())
            throw std::runtime_error("dyld bind info out of file bounds");

        const std::vector<std::uint8_t> stream(
            macho.begin() + static_cast<std::ptrdiff_t>(region.offset),
            macho.begin() + static_cast<std::ptrdiff_t>(region.offset + region.size));
        const auto rebuilt = RewriteBindStream(stream);
        if (rebuilt == stream) continue;

        if (rebuilt.size() <= stream.size()) {
            std::memcpy(macho.data() + region.offset, rebuilt.data(), rebuilt.size());
            std::memset(macho.data() + region.offset + rebuilt.size(), 0u, stream.size() - rebuilt.size());
            Write(macho, dyldInfoCommandOffset + region.sizeFieldOffset, static_cast<std::uint32_t>(rebuilt.size()));
            continue;
        }

        const auto appendedOffset = AppendRegion(macho, linkedit, rebuilt);
        if (appendedOffset > std::numeric_limits<std::uint32_t>::max())
            throw std::runtime_error("appended dyld bind info exceeds 32-bit file offsets");
        Write(macho, dyldInfoCommandOffset + region.offsetFieldOffset, static_cast<std::uint32_t>(appendedOffset));
        Write(macho, dyldInfoCommandOffset + region.sizeFieldOffset, static_cast<std::uint32_t>(rebuilt.size()));
    }
}

}

void MachONidPatcher::PatchNids(std::vector<std::uint8_t>& macho, const std::string& libraryName, const std::unordered_set<std::string>& excludedExports) const {
    using namespace Internal;

    if (macho.size() < sizeof(MachHeader64)) throw std::runtime_error("file too small");
    const auto header = Read<MachHeader64>(macho, 0u);
    if (header.magic != kMachMagic64) throw std::runtime_error("not Mach-O 64");

    std::size_t symtabCommandOffset = 0u;
    auto symtab = SymtabCommand{};
    std::size_t dyldInfoCommandOffset = 0u;
    std::size_t exportsTrieCommandOffset = 0u;
    auto exportsTrie = DyldExportsTrieCommand{};
    SegmentRef linkedit{};

    const std::size_t loadCommandEnd = sizeof(MachHeader64) + header.sizeofcmds;
    if (loadCommandEnd > macho.size()) throw std::runtime_error("load commands exceed file size");

    std::size_t cursor = sizeof(MachHeader64);
    for (std::uint32_t i = 0u; i < header.ncmds; ++i) {
        if (cursor + 8u > loadCommandEnd) throw std::runtime_error("truncated load command");
        const auto cmd = Read<std::uint32_t>(macho, cursor);
        const auto cmdsize = Read<std::uint32_t>(macho, cursor + 4u);
        if (cmdsize < 8u || cursor + cmdsize > loadCommandEnd) throw std::runtime_error("invalid load command size");

        if (cmd == kLcSegment64) {
            if (cmdsize < sizeof(SegmentCommand64)) throw std::runtime_error("malformed LC_SEGMENT_64");
            const auto segment = Read<SegmentCommand64>(macho, cursor);
            if (SegmentName(segment) == kLinkeditName) linkedit = {cursor, segment};
        } else if (cmd == kLcSymtab) {
            if (cmdsize < sizeof(SymtabCommand)) throw std::runtime_error("malformed LC_SYMTAB");
            symtabCommandOffset = cursor;
            symtab = Read<SymtabCommand>(macho, cursor);
        } else if (cmd == kLcDyldInfo || cmd == kLcDyldInfoOnly) {
            if (cmdsize < sizeof(DyldInfoCommand)) throw std::runtime_error("malformed LC_DYLD_INFO");
            dyldInfoCommandOffset = cursor;
        } else if (cmd == kLcDyldExportsTrie) {
            if (cmdsize < sizeof(DyldExportsTrieCommand)) throw std::runtime_error("malformed LC_DYLD_EXPORTS_TRIE");
            exportsTrieCommandOffset = cursor;
            exportsTrie = Read<DyldExportsTrieCommand>(macho, cursor);
        } else if (cmd == kLcDyldChainedFixups) {
            throw std::runtime_error("LC_DYLD_CHAINED_FIXUPS is not supported");
        }

        cursor += cmdsize;
    }

    if (symtabCommandOffset == 0u) throw std::runtime_error("no LC_SYMTAB load command");

    TrieLocation trie{};
    if (exportsTrieCommandOffset != 0u) {
        trie = {exportsTrieCommandOffset, exportsTrie.data_off, exportsTrie.data_size, true};
    } else if (dyldInfoCommandOffset != 0u) {
        const auto dyldInfo = Read<DyldInfoCommand>(macho, dyldInfoCommandOffset);
        trie = {dyldInfoCommandOffset, dyldInfo.export_off, dyldInfo.export_size, false};
    }

    if (symtab.nsyms == 0u && trie.dataSize == 0u) throw std::runtime_error("symbol table is empty");

    if (static_cast<std::size_t>(symtab.symoff) + static_cast<std::size_t>(symtab.nsyms) * kNlistSize > macho.size() ||
        static_cast<std::size_t>(symtab.stroff) + symtab.strsize > macho.size())
        throw std::runtime_error("symbol table out of file bounds");

    const std::size_t strTabEnd = static_cast<std::size_t>(symtab.stroff) + symtab.strsize;
    const std::vector<std::uint8_t> oldStrTab(
        macho.begin() + static_cast<std::ptrdiff_t>(symtab.stroff),
        macho.begin() + static_cast<std::ptrdiff_t>(strTabEnd));

    std::vector<std::pair<std::string, std::vector<std::uint8_t>>> trieTerminals;
    if (trie.dataSize != 0u) {
        if (static_cast<std::size_t>(trie.dataOffset) + trie.dataSize > macho.size())
            throw std::runtime_error("export trie out of file bounds");
        const std::vector<std::uint8_t> blob(
            macho.begin() + static_cast<std::ptrdiff_t>(trie.dataOffset),
            macho.begin() + static_cast<std::ptrdiff_t>(trie.dataOffset + trie.dataSize));
        std::unordered_set<std::size_t> visited;
        const auto root = ParseTrieNode(blob, 0u, visited);
        std::string prefix;
        CollectTerminals(root, prefix, trieTerminals);
        for (const auto& terminal : trieTerminals) {
            if (terminal.first.empty()) throw std::runtime_error("empty export name in trie");
        }
    }

    std::vector<SymbolRef> externals;
    std::vector<std::string> exportedNames;
    for (std::uint32_t i = 0u; i < symtab.nsyms; ++i) {
        const std::size_t nlistOffset = static_cast<std::size_t>(symtab.symoff) + i * kNlistSize;
        const auto entry = Read<Nlist64>(macho, nlistOffset);
        if ((entry.n_type & kNStab) != 0u) continue;
        if ((entry.n_type & kNExt) == 0u) continue;
        if (entry.n_sect != 0u && (entry.n_type & kNPext) != 0u) continue;

        const std::string name = ReadCStr(oldStrTab, entry.n_strx);
        if (name.empty()) continue;

        const bool defined = entry.n_sect != 0u;
        externals.push_back({nlistOffset, entry.n_strx, name, defined});
        if (defined) exportedNames.push_back(name);
    }

    std::unordered_set<std::string> knownNames(exportedNames.begin(), exportedNames.end());
    for (const auto& terminal : trieTerminals) {
        if (knownNames.insert(terminal.first).second)
            exportedNames.push_back(terminal.first);
    }

    const auto nidMap = ResolveNids(exportedNames, libraryName, excludedExports);

    RewriteSymbolTable(macho, symtabCommandOffset, linkedit, nidMap, oldStrTab, externals);
    RewriteExportTrie(macho, linkedit, trie, nidMap, std::move(trieTerminals));
    if (dyldInfoCommandOffset != 0u) RewriteBindStreams(macho, dyldInfoCommandOffset, linkedit);
}

}
