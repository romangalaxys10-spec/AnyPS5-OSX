#pragma once
#include <cstddef>
#include <cstdint>

// FreeBSD 11 ABI used by the PS5 SDK: names start at byte 8.
struct GuestDirectoryEntry {
    std::uint32_t fileNumber;
    std::uint16_t recordLength;
    std::uint8_t type;
    std::uint8_t nameLength;
    char name[256];
};
static_assert(offsetof(GuestDirectoryEntry, name) == 8);
static_assert(sizeof(GuestDirectoryEntry) == 264);
