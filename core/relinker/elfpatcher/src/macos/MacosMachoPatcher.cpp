#include <elfpatcher/macos/MacosMachoPatcher.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <set>
#include <utility>

namespace Elfpatcher::Macos {

namespace {

constexpr std::uint32_t kMachMagic64 = 0xFEEDFACF;
constexpr std::int32_t kCpuTypeX8664 = 0x01000007;
constexpr std::int32_t kCpuSubtypeX8664All = 3;
constexpr std::uint32_t kMachFileExecute = 2;
constexpr std::uint32_t kMachFlags = 0x1 | 0x4 | 0x80;
constexpr std::size_t kMachHeaderSize = 32;
constexpr std::size_t kSegmentCommandSize = 72;
constexpr std::size_t kSectionSize = 80;
constexpr std::size_t kMainCommandSize = 24;
constexpr std::size_t kDylibHeaderSize = 24;
constexpr std::size_t kRpathHeaderSize = 12;
constexpr std::size_t kDyldInfoCommandSize = 48;
constexpr std::size_t kBuildVersionCommandSize = 24;
constexpr std::size_t kUuidCommandSize = 24;
constexpr std::size_t kSymtabCommandSize = 24;
constexpr std::size_t kExecutablePathPrefixSize = 17;
constexpr std::uint32_t kLcSegment64 = 0x19;
constexpr std::uint32_t kLcMain = 0x80000028;
constexpr std::uint32_t kLcLoadDylib = 0xC;
constexpr std::uint32_t kLcRpath = 0x1C;
constexpr std::uint32_t kLcDyldInfoOnly = 0x80000022;
constexpr std::uint32_t kLcBuildVersion = 0x32;
constexpr std::uint32_t kLcUuid = 0x1B;
constexpr std::uint32_t kLcSymtab = 0x2;
constexpr std::uint32_t kVmProtRead = 0x1;
constexpr std::uint32_t kVmProtWrite = 0x2;
constexpr std::uint32_t kVmProtExecute = 0x4;
constexpr std::uint64_t kBaseVmAddress = 0x100000000;
constexpr std::uint64_t kPageSize = 0x1000;
constexpr std::uint64_t kLinkeditStringPoolSize = 16;
constexpr std::uint64_t kStubReserve = 48;
constexpr std::uint64_t kMaxBranchDisplacement = 0x7FFFFFFF;
constexpr std::uint64_t kMaxFileOffset32 = 0xFFFFFFFF;
constexpr std::uint32_t kPlatformMacos = 2;
constexpr std::uint32_t kMinos120 = 0x000C0000;
constexpr std::uint32_t kSdk1400 = 0x000E0000;
constexpr std::uint8_t kBindOpSetDylibFlatLookup = 0x32;
constexpr std::uint8_t kBindOpSetTypeImm = 0x50;
constexpr std::uint8_t kBindOpSetSegmentAndOffset = 0x70;
constexpr std::uint8_t kBindOpSetSymbolTrailing = 0x40;
constexpr std::uint8_t kBindOpDoBind = 0x90;
constexpr std::uint8_t kBindOpDone = 0x00;
constexpr std::uint8_t kBindTypePointer = 1;
constexpr std::uint8_t kBindTypeStub = 4;
constexpr std::uint32_t kDataSegmentIndex = 2;
constexpr std::uint32_t kSectionNonLazySymbolPointers = 6;
constexpr std::uint32_t kSectionAlign8 = 3;
constexpr std::uint8_t kStbGlobal = 1;
constexpr std::uint8_t kStbWeak = 2;
constexpr std::uint64_t kFnvOffsetBasis = 0xcbf29ce484222325;
constexpr std::uint64_t kFnvPrime = 0x100000001b3;

void appendFixedName(std::vector<std::uint8_t>& buf, const char* name) {
    std::size_t index = 0;
    for (; name[index] != 0; ++index)
        buf.push_back(static_cast<std::uint8_t>(name[index]));
    buf.resize(buf.size() + (16 - index), 0);
}

}

MacosMachoPatcher::MacosMachoPatcher(
    std::shared_ptr<IEntryStubBuilder> entryStubBuilder,
    std::shared_ptr<ISegmentFilter> segmentFilter,
    std::shared_ptr<Io::IByteWriter> byteWriter
)
    : _entryStubBuilder(std::move(entryStubBuilder))
    , _segmentFilter(std::move(segmentFilter))
    , _byteWriter(std::move(byteWriter))
{
}

std::vector<Domain::ProgramHeader> MacosMachoPatcher::_collectLoadSegments(const std::vector<Domain::ProgramHeader>& originalHeaders) const {
    std::vector<Domain::ProgramHeader> result;
    for (const auto& ph : originalHeaders) {
        if (_segmentFilter->ShouldSkip(ph) || ph.Type != PT_LOAD)
            continue;
        if (ph.FileSize > ph.MemorySize)
            throw Domain::RelinkerException("PT_LOAD file size exceeds memory size", ph.Offset);
        result.push_back(ph);
    }
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) { return left.MappedAddress < right.MappedAddress; });
    for (std::size_t index = 1; index < result.size(); ++index) {
        if (result[index].MappedAddress < result[index - 1].MappedAddress + result[index - 1].MemorySize)
            throw Domain::RelinkerException("Overlapping PT_LOAD memory ranges", result[index].MappedAddress);
        if ((result[index].MappedAddress - result.front().MappedAddress) % kPageSize != 0)
            throw Domain::RelinkerException("PT_LOAD vaddr is not page aligned relative to the image base", result[index].MappedAddress);
    }
    return result;
}

std::vector<std::string> MacosMachoPatcher::_readNeededLibraries(const Domain::SysVDynamicSection& dynSection) const {
    const auto& bytes = dynSection.DynamicSegmentData;
    if (bytes.size() % kDynEntrySize != 0)
        throw Domain::RelinkerException("Invalid dynamic segment size");
    std::vector<std::string> result;
    std::set<std::string> unique;
    for (std::size_t offset = 0; offset < bytes.size(); offset += kDynEntrySize) {
        const auto tag = Io::ReadU64(bytes, offset);
        if (tag != static_cast<std::uint64_t>(DT_NEEDED))
            throw Domain::RelinkerException("Unexpected tag in rebuilt ELF dependency table", offset);
        const auto nameOffset = Io::ReadU64(bytes, offset + 8);
        if (nameOffset >= dynSection.DynStrData.size())
            throw Domain::RelinkerException("DT_NEEDED string offset is out of bounds", nameOffset);
        auto name = _readString(dynSection.DynStrData, static_cast<std::size_t>(nameOffset));
        if (name.empty() || name.find_first_of("/\\:") != std::string::npos || !unique.insert(name).second)
            throw Domain::RelinkerException("Invalid or duplicate DT_NEEDED library: " + name);
        result.push_back(std::move(name));
    }
    return result;
}

std::vector<std::string> MacosMachoPatcher::_readUndefinedSymbols(const Domain::SysVDynamicSection& dynSection) const {
    const auto& dynsym = dynSection.DynSymData;
    if (dynsym.size() % kSymEntrySize != 0)
        throw Domain::RelinkerException("Invalid dynamic symbol table size");
    std::vector<std::string> result;
    for (std::size_t offset = kSymEntrySize; offset < dynsym.size(); offset += kSymEntrySize) {
        const auto nameOffset = Io::ReadU32(dynsym, offset);
        const auto binding = static_cast<std::uint8_t>(dynsym[offset + 4] >> 4);
        const auto shndx = Io::ReadU16(dynsym, offset + 6);
        if (shndx != 0 || (binding != kStbGlobal && binding != kStbWeak))
            continue;
        if (nameOffset >= dynSection.DynStrData.size())
            throw Domain::RelinkerException("Dynamic symbol name offset is out of bounds", nameOffset);
        auto name = _readString(dynSection.DynStrData, nameOffset);
        if (name.empty())
            throw Domain::RelinkerException("Undefined dynamic symbol has an empty name", offset);
        result.push_back(std::move(name));
    }
    return result;
}

std::string MacosMachoPatcher::_readString(const std::vector<std::uint8_t>& pool, const std::size_t offset) const {
    std::string result;
    for (std::size_t index = offset; index < pool.size() && pool[index] != 0; ++index)
        result.push_back(static_cast<char>(pool[index]));
    return result;
}

std::vector<std::uint8_t> MacosMachoPatcher::_buildBindStream(const std::vector<std::string>& symbols, const std::uint64_t firstSlotOffset, const std::uint8_t bindType) const {
    std::vector<std::uint8_t> stream;
    if (symbols.empty())
        return stream;
    _byteWriter->AppendU8(stream, kBindOpSetDylibFlatLookup);
    _byteWriter->AppendU8(stream, kBindOpSetTypeImm | bindType);
    for (std::size_t index = 0; index < symbols.size(); ++index) {
        _byteWriter->AppendU8(stream, kBindOpSetSegmentAndOffset | kDataSegmentIndex);
        _appendUleb128(stream, firstSlotOffset + index * 8);
        _byteWriter->AppendU8(stream, kBindOpSetSymbolTrailing);
        for (const char c : symbols[index])
            stream.push_back(static_cast<std::uint8_t>(c));
        stream.push_back(0);
        _byteWriter->AppendU8(stream, kBindOpDoBind);
    }
    _byteWriter->AppendU8(stream, kBindOpDone);
    return stream;
}

std::uint64_t MacosMachoPatcher::_fnv1a64(const std::uint8_t* data, const std::size_t size) const {
    std::uint64_t hash = kFnvOffsetBasis;
    for (std::size_t index = 0; index < size; ++index) {
        hash ^= data[index];
        hash *= kFnvPrime;
    }
    return hash;
}

std::array<std::uint8_t, 16> MacosMachoPatcher::_computeUuid(const std::vector<std::uint8_t>& image) const {
    const std::uint64_t primary = _fnv1a64(image.data(), image.size());
    std::array<std::uint8_t, 8> primaryBytes{};
    for (std::size_t index = 0; index < primaryBytes.size(); ++index)
        primaryBytes[index] = static_cast<std::uint8_t>((primary >> (56 - 8 * index)) & 0xFF);
    const std::uint64_t secondary = _fnv1a64(primaryBytes.data(), primaryBytes.size());
    std::array<std::uint8_t, 16> uuid{};
    for (std::size_t index = 0; index < 8; ++index) {
        uuid[index] = static_cast<std::uint8_t>((primary >> (56 - 8 * index)) & 0xFF);
        uuid[8 + index] = static_cast<std::uint8_t>((secondary >> (56 - 8 * index)) & 0xFF);
    }
    return uuid;
}

void MacosMachoPatcher::_appendUleb128(std::vector<std::uint8_t>& buf, std::uint64_t value) const {
    do {
        std::uint8_t byte = static_cast<std::uint8_t>(value & 0x7F);
        value >>= 7;
        if (value != 0)
            byte |= 0x80;
        buf.push_back(byte);
    } while (value != 0);
}

void MacosMachoPatcher::_appendSegmentCommand(std::vector<std::uint8_t>& cmds, const char* name, const std::uint64_t vmaddr, const std::uint64_t vmsize, const std::uint64_t fileoff, const std::uint64_t filesize, const std::uint32_t prot, const std::uint32_t nsects) const {
    _byteWriter->AppendU32(cmds, kLcSegment64);
    _byteWriter->AppendU32(cmds, static_cast<std::uint32_t>(kSegmentCommandSize + nsects * kSectionSize));
    appendFixedName(cmds, name);
    _byteWriter->AppendU64(cmds, vmaddr);
    _byteWriter->AppendU64(cmds, vmsize);
    _byteWriter->AppendU64(cmds, fileoff);
    _byteWriter->AppendU64(cmds, filesize);
    _byteWriter->AppendU32(cmds, prot);
    _byteWriter->AppendU32(cmds, prot);
    _byteWriter->AppendU32(cmds, nsects);
    _byteWriter->AppendU32(cmds, 0);
}

void MacosMachoPatcher::_appendGotSection(std::vector<std::uint8_t>& cmds, const std::uint64_t addr, const std::uint64_t size, const std::uint32_t fileOffset) const {
    appendFixedName(cmds, "__got");
    appendFixedName(cmds, "__DATA");
    _byteWriter->AppendU64(cmds, addr);
    _byteWriter->AppendU64(cmds, size);
    _byteWriter->AppendU32(cmds, fileOffset);
    _byteWriter->AppendU32(cmds, kSectionAlign8);
    _byteWriter->AppendU32(cmds, 0);
    _byteWriter->AppendU32(cmds, 0);
    _byteWriter->AppendU32(cmds, kSectionNonLazySymbolPointers);
    _byteWriter->AppendU32(cmds, 0);
    _byteWriter->AppendU32(cmds, 0);
    _byteWriter->AppendU32(cmds, 0);
}

void MacosMachoPatcher::_appendDylibCommand(std::vector<std::uint8_t>& cmds, const std::string& installName) const {
    const auto cmdSize = static_cast<std::uint32_t>(Io::AlignUp64(kDylibHeaderSize + installName.size() + 1, 8));
    _byteWriter->AppendU32(cmds, kLcLoadDylib);
    _byteWriter->AppendU32(cmds, cmdSize);
    _byteWriter->AppendU32(cmds, static_cast<std::uint32_t>(kDylibHeaderSize));
    _byteWriter->AppendU32(cmds, 1);
    _byteWriter->AppendU32(cmds, 0);
    _byteWriter->AppendU32(cmds, 0);
    Io::AppendString(cmds, installName);
    Io::AlignBuffer(cmds, 8);
}

void MacosMachoPatcher::_appendRpathCommand(std::vector<std::uint8_t>& cmds, const std::string& path) const {
    const auto cmdSize = static_cast<std::uint32_t>(Io::AlignUp64(kRpathHeaderSize + path.size() + 1, 8));
    _byteWriter->AppendU32(cmds, kLcRpath);
    _byteWriter->AppendU32(cmds, cmdSize);
    _byteWriter->AppendU32(cmds, static_cast<std::uint32_t>(kRpathHeaderSize));
    Io::AppendString(cmds, path);
    Io::AlignBuffer(cmds, 8);
}

void MacosMachoPatcher::_appendMainCommand(std::vector<std::uint8_t>& cmds, const std::uint32_t entryoff) const {
    _byteWriter->AppendU32(cmds, kLcMain);
    _byteWriter->AppendU32(cmds, static_cast<std::uint32_t>(kMainCommandSize));
    _byteWriter->AppendU32(cmds, entryoff);
    _byteWriter->AppendU64(cmds, 0);
}

void MacosMachoPatcher::_appendDyldInfoCommand(std::vector<std::uint8_t>& cmds, const std::uint32_t bindOff, const std::uint32_t bindSize, const std::uint32_t lazyBindOff, const std::uint32_t lazyBindSize) const {
    _byteWriter->AppendU32(cmds, kLcDyldInfoOnly);
    _byteWriter->AppendU32(cmds, static_cast<std::uint32_t>(kDyldInfoCommandSize));
    _byteWriter->AppendU32(cmds, 0);
    _byteWriter->AppendU32(cmds, 0);
    _byteWriter->AppendU32(cmds, bindOff);
    _byteWriter->AppendU32(cmds, bindSize);
    _byteWriter->AppendU32(cmds, 0);
    _byteWriter->AppendU32(cmds, 0);
    _byteWriter->AppendU32(cmds, lazyBindOff);
    _byteWriter->AppendU32(cmds, lazyBindSize);
    _byteWriter->AppendU32(cmds, 0);
    _byteWriter->AppendU32(cmds, 0);
}

void MacosMachoPatcher::_appendBuildVersionCommand(std::vector<std::uint8_t>& cmds) const {
    _byteWriter->AppendU32(cmds, kLcBuildVersion);
    _byteWriter->AppendU32(cmds, static_cast<std::uint32_t>(kBuildVersionCommandSize));
    _byteWriter->AppendU32(cmds, kPlatformMacos);
    _byteWriter->AppendU32(cmds, kMinos120);
    _byteWriter->AppendU32(cmds, kSdk1400);
    _byteWriter->AppendU32(cmds, 0);
}

void MacosMachoPatcher::_appendUuidCommand(std::vector<std::uint8_t>& cmds, const std::array<std::uint8_t, 16>& uuid) const {
    _byteWriter->AppendU32(cmds, kLcUuid);
    _byteWriter->AppendU32(cmds, static_cast<std::uint32_t>(kUuidCommandSize));
    for (const std::uint8_t byte : uuid)
        _byteWriter->AppendU8(cmds, byte);
}

void MacosMachoPatcher::_appendSymtabCommand(std::vector<std::uint8_t>& cmds, const std::uint32_t stroff) const {
    _byteWriter->AppendU32(cmds, kLcSymtab);
    _byteWriter->AppendU32(cmds, static_cast<std::uint32_t>(kSymtabCommandSize));
    _byteWriter->AppendU32(cmds, 0);
    _byteWriter->AppendU32(cmds, 0);
    _byteWriter->AppendU32(cmds, stroff);
    _byteWriter->AppendU32(cmds, static_cast<std::uint32_t>(kLinkeditStringPoolSize));
}

std::vector<std::uint8_t> MacosMachoPatcher::Patch(
    const std::vector<std::uint8_t>& sourceElf,
    const std::vector<Domain::ProgramHeader>& originalHeaders,
    const Domain::SysVDynamicSection& dynSection,
    const std::uint64_t originalPltGotVaddr,
    const std::string& runPath,
    const bool lazyBinding,
    const bool dependencyDiagnostics)
{
    if (dependencyDiagnostics)
        throw Domain::RelinkerException("macOS target does not support --windows-diagnostics");

    if (sourceElf.size() < 64 || Io::ReadU32(sourceElf, 0) != 0x464c457f || sourceElf[4] != 2 || sourceElf[5] != 1 || sourceElf[6] != 1 || Io::ReadU16(sourceElf, 18) != 62)
        throw Domain::RelinkerException("macOS output requires a little-endian ELF64 x86-64 image");
    const auto elfType = Io::ReadU16(sourceElf, 16);
    if (elfType != ET_DYN && elfType != 0xfe10 && elfType != 0xfe18)
        throw Domain::RelinkerException("macOS output requires a position-independent ELF image");

    const std::uint64_t realEntryVaddr = Io::ReadU64(sourceElf, 24);
    const std::vector<Domain::ProgramHeader> loadSegments = _collectLoadSegments(originalHeaders);
    if (loadSegments.empty())
        throw Domain::RelinkerException("No PT_LOAD segments found");

    std::vector<Domain::ProgramHeader> execSegments;
    std::vector<Domain::ProgramHeader> dataSegments;
    for (const auto& ph : loadSegments) {
        if ((ph.Flags & PF_X) != 0)
            execSegments.push_back(ph);
        else
            dataSegments.push_back(ph);
    }
    if (execSegments.empty())
        throw Domain::RelinkerException("No executable PT_LOAD segment to map as __TEXT");

    std::uint64_t execFileEndVaddr = 0;
    std::uint64_t execMemEndVaddr = 0;
    for (const auto& ph : execSegments) {
        execFileEndVaddr = std::max(execFileEndVaddr, ph.MappedAddress + ph.FileSize);
        execMemEndVaddr = std::max(execMemEndVaddr, ph.MappedAddress + ph.MemorySize);
    }
    if (!dataSegments.empty() && dataSegments.front().MappedAddress < execMemEndVaddr)
        throw Domain::RelinkerException("PT_LOAD writable region overlaps the executable region", dataSegments.front().MappedAddress);

    bool entryExecutable = false;
    for (const auto& ph : execSegments) {
        if (realEntryVaddr >= ph.MappedAddress && realEntryVaddr - ph.MappedAddress < ph.MemorySize) {
            entryExecutable = true;
            break;
        }
    }
    if (!entryExecutable)
        throw Domain::RelinkerException("ELF entry point is not inside an executable PT_LOAD segment", realEntryVaddr);

    if (originalPltGotVaddr != 0) {
        bool gotContained = false;
        for (const auto& ph : loadSegments) {
            if (originalPltGotVaddr >= ph.MappedAddress && originalPltGotVaddr - ph.MappedAddress + 8 <= ph.MemorySize) {
                gotContained = true;
                break;
            }
        }
        if (!gotContained)
            throw Domain::RelinkerException("Original PLT GOT is not contained in any kept PT_LOAD segment", originalPltGotVaddr);
    }

    const std::vector<std::string> bindSymbols = _readUndefinedSymbols(dynSection);
    const std::vector<std::string> neededLibraries = _readNeededLibraries(dynSection);
    const std::uint64_t gotSize = bindSymbols.size() * 8;
    const std::uint32_t dataNsects = bindSymbols.empty() ? 0u : 1u;

    std::uint64_t cmdsSize = 4 * kSegmentCommandSize + dataNsects * kSectionSize
        + kMainCommandSize + kDyldInfoCommandSize + kBuildVersionCommandSize + kUuidCommandSize + kSymtabCommandSize;
    for (const auto& lib : neededLibraries)
        cmdsSize += Io::AlignUp64(kDylibHeaderSize + kExecutablePathPrefixSize + lib.size() + 1, 8);
    if (!runPath.empty())
        cmdsSize += Io::AlignUp64(kRpathHeaderSize + runPath.size() + 1, 8);

    const std::uint64_t headerEnd = kMachHeaderSize + cmdsSize;
    const std::uint64_t headerBlockEnd = Io::AlignUp64(headerEnd + kStubReserve, kPageSize);
    const std::uint64_t stubFileOffset = Io::AlignUp64(headerEnd, 16);
    const std::uint64_t minVaddr = loadSegments.front().MappedAddress;

    const std::uint64_t textFileSize = headerBlockEnd + (execFileEndVaddr - minVaddr);
    const std::uint64_t textVmsize = Io::AlignUp64(headerBlockEnd + (execMemEndVaddr - minVaddr), kPageSize);

    std::uint64_t dataFileOffset;
    std::uint64_t gotFileOffset;
    if (!dataSegments.empty()) {
        dataFileOffset = headerBlockEnd + (dataSegments.front().MappedAddress - minVaddr);
        std::uint64_t dataMemEndVaddr = 0;
        for (const auto& ph : dataSegments)
            dataMemEndVaddr = std::max(dataMemEndVaddr, ph.MappedAddress + ph.MemorySize);
        gotFileOffset = Io::AlignUp64(headerBlockEnd + (dataMemEndVaddr - minVaddr), 16);
    } else {
        dataFileOffset = std::max(Io::AlignUp64(textFileSize, kPageSize), textVmsize);
        gotFileOffset = dataFileOffset;
    }
    const std::uint64_t dataFileSize = gotFileOffset + gotSize - dataFileOffset;
    const std::uint64_t dataVmsize = Io::AlignUp64(dataFileSize != 0 ? dataFileSize : 1, kPageSize);

    const std::vector<std::uint8_t> bindStream = _buildBindStream(bindSymbols, gotFileOffset - dataFileOffset, kBindTypePointer);
    std::vector<std::uint8_t> lazyStream;
    if (lazyBinding)
        lazyStream = _buildBindStream(bindSymbols, gotFileOffset - dataFileOffset, kBindTypeStub);

    const std::uint64_t linkeditFileOffset = Io::AlignUp64(dataFileOffset + dataVmsize, kPageSize);
    std::uint64_t cursor = linkeditFileOffset + bindStream.size();
    const std::uint64_t lazyBindOffset = lazyStream.empty() ? 0 : Io::AlignUp64(cursor, 8);
    if (!lazyStream.empty())
        cursor = lazyBindOffset + lazyStream.size();
    const std::uint64_t stringPoolOffset = Io::AlignUp64(cursor, 8);
    const std::uint64_t totalSize = stringPoolOffset + kLinkeditStringPoolSize;
    const std::uint64_t linkeditFileSize = totalSize - linkeditFileOffset;
    const std::uint64_t linkeditVmsize = Io::AlignUp64(linkeditFileSize, kPageSize);
    if (totalSize > kMaxFileOffset32)
        throw Domain::RelinkerException("Mach-O output exceeds the 32-bit file offset range", totalSize);

    const std::uint64_t stubVmaddr = kBaseVmAddress + stubFileOffset;
    const std::uint64_t mappedEntryVmaddr = kBaseVmAddress + headerBlockEnd + (realEntryVaddr - minVaddr);
    if (mappedEntryVmaddr < stubVmaddr || mappedEntryVmaddr - stubVmaddr > kMaxBranchDisplacement)
        throw Domain::RelinkerException("Entry stub cannot reach the mapped ELF entry point", realEntryVaddr);
    const std::vector<std::uint8_t> stubBytes = _entryStubBuilder->BuildEntryStub(stubVmaddr, mappedEntryVmaddr);

    const std::array<std::uint8_t, 16> uuid = _computeUuid(sourceElf);

    const std::uint32_t commandCount = 9 + static_cast<std::uint32_t>(neededLibraries.size()) + (runPath.empty() ? 0u : 1u);
    std::vector<std::uint8_t> cmds;
    _appendSegmentCommand(cmds, "__PAGEZERO", 0, kBaseVmAddress, 0, 0, 0, 0);
    _appendSegmentCommand(cmds, "__TEXT", kBaseVmAddress, textVmsize, 0, textFileSize, kVmProtRead | kVmProtExecute, 0);
    _appendSegmentCommand(cmds, "__DATA", kBaseVmAddress + dataFileOffset, dataVmsize, dataFileOffset, dataFileSize, kVmProtRead | kVmProtWrite, dataNsects);
    if (dataNsects != 0)
        _appendGotSection(cmds, kBaseVmAddress + gotFileOffset, gotSize, static_cast<std::uint32_t>(gotFileOffset));
    _appendSegmentCommand(cmds, "__LINKEDIT", kBaseVmAddress + linkeditFileOffset, linkeditVmsize, linkeditFileOffset, linkeditFileSize, kVmProtRead, 0);
    _appendMainCommand(cmds, static_cast<std::uint32_t>(stubFileOffset));
    for (const auto& lib : neededLibraries)
        _appendDylibCommand(cmds, "@executable_path/" + lib);
    if (!runPath.empty())
        _appendRpathCommand(cmds, runPath);
    _appendDyldInfoCommand(cmds,
        bindStream.empty() ? 0u : static_cast<std::uint32_t>(linkeditFileOffset),
        static_cast<std::uint32_t>(bindStream.size()),
        static_cast<std::uint32_t>(lazyBindOffset),
        static_cast<std::uint32_t>(lazyStream.size()));
    _appendBuildVersionCommand(cmds);
    _appendUuidCommand(cmds, uuid);
    _appendSymtabCommand(cmds, static_cast<std::uint32_t>(stringPoolOffset));
    if (cmds.size() != cmdsSize || cmds.size() % 8 != 0)
        throw Domain::RelinkerException("Load command size bookkeeping mismatch", cmds.size());

    std::vector<std::uint8_t> image(totalSize, 0);
    _byteWriter->WriteU32(image, 0, kMachMagic64);
    _byteWriter->WriteU32(image, 4, static_cast<std::uint32_t>(kCpuTypeX8664));
    _byteWriter->WriteU32(image, 8, static_cast<std::uint32_t>(kCpuSubtypeX8664All));
    _byteWriter->WriteU32(image, 12, kMachFileExecute);
    _byteWriter->WriteU32(image, 16, commandCount);
    _byteWriter->WriteU32(image, 20, static_cast<std::uint32_t>(cmds.size()));
    _byteWriter->WriteU32(image, 24, kMachFlags);
    _byteWriter->WriteU32(image, 28, 0);
    std::copy(cmds.begin(), cmds.end(), image.begin() + static_cast<std::ptrdiff_t>(kMachHeaderSize));
    std::copy(stubBytes.begin(), stubBytes.end(), image.begin() + static_cast<std::ptrdiff_t>(stubFileOffset));
    for (const auto& ph : loadSegments) {
        if (ph.FileSize == 0)
            continue;
        if (ph.Offset > sourceElf.size() || ph.FileSize > sourceElf.size() - ph.Offset)
            throw Domain::RelinkerException("PT_LOAD file range exceeds the source image", ph.Offset);
        const auto dst = static_cast<std::ptrdiff_t>(headerBlockEnd + (ph.MappedAddress - minVaddr));
        std::copy_n(sourceElf.begin() + static_cast<std::ptrdiff_t>(ph.Offset), static_cast<std::size_t>(ph.FileSize), image.begin() + dst);
    }
    if (!bindStream.empty())
        std::copy(bindStream.begin(), bindStream.end(), image.begin() + static_cast<std::ptrdiff_t>(linkeditFileOffset));
    if (!lazyStream.empty())
        std::copy(lazyStream.begin(), lazyStream.end(), image.begin() + static_cast<std::ptrdiff_t>(lazyBindOffset));

    return image;
}

}
