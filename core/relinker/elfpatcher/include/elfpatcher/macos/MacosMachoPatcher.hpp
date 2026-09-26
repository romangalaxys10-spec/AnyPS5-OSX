#ifndef ELFPATCHER_MACOS_MACOSMACHOPATCHER_HPP
#define ELFPATCHER_MACOS_MACOSMACHOPATCHER_HPP

#include <elfpatcher/general/IElfPatcher.hpp>
#include <elfpatcher/general/IEntryStubBuilder.hpp>
#include <elfpatcher/general/ISegmentFilter.hpp>
#include <io/IByteWriter.hpp>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Elfpatcher::Macos {

class MacosMachoPatcher : public IElfPatcher {
public:
    MacosMachoPatcher(
        std::shared_ptr<IEntryStubBuilder> entryStubBuilder,
        std::shared_ptr<ISegmentFilter> segmentFilter,
        std::shared_ptr<Io::IByteWriter> byteWriter
    );

    std::vector<std::uint8_t> Patch(
        const std::vector<std::uint8_t>& sourceElf,
        const std::vector<Domain::ProgramHeader>& originalHeaders,
        const Domain::SysVDynamicSection& dynamicSection,
        std::uint64_t originalPltGotVaddr,
        const std::string& runPath,
        bool lazyBinding,
        bool dependencyDiagnostics
    ) override;

private:
    std::shared_ptr<IEntryStubBuilder> _entryStubBuilder;
    std::shared_ptr<ISegmentFilter> _segmentFilter;
    std::shared_ptr<Io::IByteWriter> _byteWriter;

    std::vector<Domain::ProgramHeader> _collectLoadSegments(const std::vector<Domain::ProgramHeader>& originalHeaders) const;
    std::vector<std::string> _readNeededLibraries(const Domain::SysVDynamicSection& dynSection) const;
    std::vector<std::string> _readUndefinedSymbols(const Domain::SysVDynamicSection& dynSection) const;
    std::string _readString(const std::vector<std::uint8_t>& pool, std::size_t offset) const;
    std::vector<std::uint8_t> _buildBindStream(const std::vector<std::string>& symbols, std::uint64_t firstSlotOffset, std::uint8_t bindType) const;
    std::array<std::uint8_t, 16> _computeUuid(const std::vector<std::uint8_t>& image) const;
    std::uint64_t _fnv1a64(const std::uint8_t* data, std::size_t size) const;
    void _appendUleb128(std::vector<std::uint8_t>& buf, std::uint64_t value) const;
    void _appendSegmentCommand(std::vector<std::uint8_t>& cmds, const char* name, std::uint64_t vmaddr, std::uint64_t vmsize, std::uint64_t fileoff, std::uint64_t filesize, std::uint32_t prot, std::uint32_t nsects) const;
    void _appendGotSection(std::vector<std::uint8_t>& cmds, std::uint64_t addr, std::uint64_t size, std::uint32_t fileOffset) const;
    void _appendDylibCommand(std::vector<std::uint8_t>& cmds, const std::string& installName) const;
    void _appendRpathCommand(std::vector<std::uint8_t>& cmds, const std::string& path) const;
    void _appendMainCommand(std::vector<std::uint8_t>& cmds, std::uint32_t entryoff) const;
    void _appendDyldInfoCommand(std::vector<std::uint8_t>& cmds, std::uint32_t bindOff, std::uint32_t bindSize, std::uint32_t lazyBindOff, std::uint32_t lazyBindSize) const;
    void _appendBuildVersionCommand(std::vector<std::uint8_t>& cmds) const;
    void _appendUuidCommand(std::vector<std::uint8_t>& cmds, const std::array<std::uint8_t, 16>& uuid) const;
    void _appendSymtabCommand(std::vector<std::uint8_t>& cmds, std::uint32_t stroff) const;
};

}

#endif
