#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_QUEUESTATE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_QUEUESTATE_HPP

#include <cstdint>
#include <map>
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace AgcDriver {

using Registers = std::map<std::uint32_t, std::uint32_t>;

inline Registers InitialContextRegisters() {
    Registers result{
        {0x200, 0}, {0x201, 0}, {0x202, 0xcc0010}, {0x203, 0},
        {0x204, 0}, {0x205, 0}, {0x206, 1087}, {0x207, 0},
        {0x0, 0}, {0x2, 0}, {0x3, 0}, {0x4, 0}, {0x8, 0}, {0x9, 0x3f800000}, {0xa, 0}, {0xb, 0},
        {0x80, 0}, {0x83, 0xffff}, {0x8c, 0xaa99aaaa}, {0x8d, 0}, {0x8e, 0}, {0x8f, 0},
        {0xc, 0}, {0xd, 0x40004000}, {0x81, 0x80000000}, {0x82, 0x40004000},
        {0x90, 0x80000000}, {0x91, 0x40004000},
        {0x105, 0}, {0x106, 0}, {0x107, 0}, {0x108, 0},
        {0x1b1, 0}, {0x1b6, 0}, {0x1c3, 0}, {0x1c4, 0}, {0x1c5, 0},
        {0x1ff, 0}, {0x292, 2}, {0x293, 0}, {0x29b, 0},
        {0x2ce, 0}, {0x2d3, 0}, {0x2d5, 0}, {0x2d6, 0}, {0x2db, 0},
        {0x2dc, 0xaa00}, {0x2e4, 0}, {0x2f8, 0}, {0x2f9, 0x2d},
        {0x30e, 0xffffffff}, {0x30f, 0xffffffff}, {0x313, 0x6000},
        {0x318, 0}, {0x31b, 0}, {0x31c, 0}, {0x31d, 0},
        {0x390, 0}, {0x3b0, 0}, {0x3b8, 0}
    };
    for (std::uint32_t i = 0; i < 8; ++i) result.emplace(0x1e0 + i, 0x20010001);
    for (std::uint32_t i = 0; i < 16; ++i) {
        result.emplace(0x94 + 2 * i, 0x80000000);
        result.emplace(0x95 + 2 * i, 0x40004000);
        result.emplace(0xb4 + 2 * i, 0);
        result.emplace(0xb5 + 2 * i, 0);
        for (std::uint32_t j = 0; j < 6; ++j) result.emplace(0x10f + 6 * i + j, j % 2 == 0 ? 0x3f800000 : 0);
    }
    return result;
}

struct QueueState {
    Registers shader;
    Registers context = InitialContextRegisters();
    Registers userConfig{{0x24a, 0}, {0x24b, 0}};
    std::optional<Registers> savedContext;
    std::array<std::uint32_t, 0x3000> constantRam{};
    std::uint64_t indexBase = 0;
    std::uint64_t drawIndirectBase = 0;
    std::uint64_t dispatchIndirectBase = 0;
    std::uint32_t indexBufferSize = 0;
    std::uint32_t indexType = 0;
    std::uint32_t instanceCount = 1;
    std::vector<std::string> markers;

    void ClearContext() {
        context = InitialContextRegisters();
    }
};

}

#endif
