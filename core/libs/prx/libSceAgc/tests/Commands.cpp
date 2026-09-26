#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/RegisterDefaults.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>

extern "C" std::uint32_t* APS5_VABI sceAgcDcbResetQueue(CommandBuffer* buf, std::uint32_t op, std::uint32_t state);
extern "C" std::uint32_t* APS5_VABI sceAgcDcbSetFlip(CommandBuffer* buf, std::uint32_t handle, std::int32_t index, std::uint32_t mode, std::int64_t argument);
extern "C" int APS5_VABI sceAgcSuspendPoint();
extern "C" int APS5_VABI sceAgcInit(std::uint32_t* state, std::uint32_t version);
extern "C" std::uint32_t* APS5_VABI sceAgcDcbDrawIndexAuto(CommandBuffer* buf, std::uint32_t indexCount, std::uint64_t modifier);
extern "C" int APS5_VABI sceAgcWaitRegMemPatchReference(std::uint32_t* cmd, std::uint64_t reference);
extern "C" int APS5_VABI sceAgcGetDataPacketPayloadAddressUnk(std::uint32_t** addr, std::uint32_t* cmd, int type);
extern "C" std::uint32_t* APS5_VABI sceAgcCbSetShRegisterRangeDirect(CommandBuffer* buf, std::uint32_t offset, const std::uint32_t* values, std::uint32_t numValues);
extern "C" std::uint32_t* APS5_VABI sceAgcDcbContextStateAnotherOp(CommandBuffer* buf, std::uint32_t operation);

namespace {

void check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename TAction>
void expectFailure(TAction action) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        check(error.what()[0] != '\0', "empty exception message");
        return;
    }
    throw std::runtime_error("expected an exception");
}

struct Storage {
    std::array<std::uint32_t, 64> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
};

bool APS5_VABI grow(CommandBuffer* buffer, std::uint32_t count, void* userData) {
    auto& storage = *static_cast<Storage*>(userData);
    check(count == 5, "incorrect callback allocation including reserved words");
    buffer->bottom = storage.words.data();
    buffer->top = storage.words.data() + storage.words.size();
    buffer->cursor_up = buffer->bottom;
    buffer->cursor_down = buffer->top;
    return true;
}

void testPackets() {
    Storage storage;
    sceAgcDcbResetQueue(&storage.buffer, 0, 3);
    sceAgcDcbDrawIndexAuto(&storage.buffer, 17, 0);
    const std::array<std::uint32_t, 5> expected{0xc0001200u, 3, 0xc0012d00u, 17, 2};
    check(std::equal(expected.begin(), expected.end(), storage.words.begin()), "reset or draw packet mismatch");
    check(storage.buffer.cursor_up == storage.words.data() + expected.size(), "incorrect packet cursor advance");
    const auto before = storage.words;
    expectFailure([&] { sceAgcDcbResetQueue(&storage.buffer, 0, 16); });
    check(storage.words == before, "invalid reset modified packet memory");
    Storage destination;
    CommandBuffer empty{nullptr, nullptr, nullptr, nullptr, grow, &destination, 0};
    Agc::Command::Emit(&empty, 0x15u, {1, 1, 1, 0x41u}, __func__);
    check(empty.cursor_up == destination.words.data() + 5, "guest ABI allocation callback failed");
    Storage exhausted;
    exhausted.buffer.cursor_down = exhausted.words.data() + 2;
    expectFailure([&] { Agc::Command::WriteNop(&exhausted.buffer, 3, __func__); });
    check(exhausted.buffer.cursor_up == exhausted.words.data(), "failed allocation advanced cursor");
}

struct ContextGrowth {
    Storage destination;
    std::uint32_t* expectedCursor;
    std::uint32_t expectedCount;
    std::uint32_t calls = 0;
    bool success = true;
};

bool APS5_VABI growContext(CommandBuffer* buffer, std::uint32_t count, void* userData) {
    auto& growth = *static_cast<ContextGrowth*>(userData);
    check(++growth.calls == 1, "unexpected repeated context allocation callback");
    check(buffer->cursor_up == growth.expectedCursor, "context callback at wrong packet boundary");
    check(count == growth.expectedCount, "incorrect context reservation size");
    if (!growth.success) {
        return false;
    }
    buffer->bottom = growth.destination.buffer.bottom;
    buffer->top = growth.destination.buffer.top;
    buffer->cursor_up = growth.destination.buffer.cursor_up;
    buffer->cursor_down = growth.destination.buffer.cursor_down;
    return true;
}

void testContextState() {
    const std::array<std::array<std::uint32_t, 6>, 4> sizes{{{5}, {5, 8, 9, 3, 2}, {3, 5, 8, 9, 2}, {5, 8, 9, 3, 2, 5}}};
    const std::array<std::array<std::uint32_t, 4>, 4> reservations{{{5}, {22, 3, 2}, {3, 22, 2}, {22, 3, 2, 5}}};
    const std::array<std::uint32_t, 4> totals{5, 27, 27, 32};
    for (std::uint32_t operation = 0; operation < sizes.size(); ++operation) {
        for (std::uint32_t capacity = 0; capacity <= totals[operation]; ++capacity) {
            Storage source;
            source.words.fill(0xdeadbeefu);
            std::uint32_t split = 0;
            std::uint32_t requested = 0;
            for (const auto count : reservations[operation]) {
                if (count > capacity - split) {
                    requested = count;
                    break;
                }
                split += count;
            }
            ContextGrowth growth{{}, source.words.data() + split, requested + 2};
            growth.destination.words.fill(0xdeadbeefu);
            source.buffer.cursor_down = source.words.data() + capacity + 2;
            source.buffer.reserved_dw = 2;
            source.buffer.callback = growContext;
            source.buffer.user_data = &growth;
            auto* first = sceAgcDcbContextStateAnotherOp(&source.buffer, operation);
            check(first == (split == 0 ? growth.destination.words.data() : source.words.data()), "incorrect first context packet address");
            check(growth.calls == (requested == 0 ? 0u : 1u), "incorrect context callback count");
            auto* end = requested == 0 ? source.words.data() + totals[operation] : growth.destination.words.data() + totals[operation] - split;
            check(source.buffer.cursor_up == end, "incorrect context cursor advance");
            check(*end == 0xdeadbeefu && source.words[split] == 0xdeadbeefu, "context allocation overwrote adjacent memory");
            std::uint32_t offset = 0;
            for (const auto count : sizes[operation]) {
                if (count == 0) {
                    break;
                }
                const auto* packet = offset < split ? source.words.data() + offset : growth.destination.words.data() + offset - split;
                const auto header = 0xc0001000u | ((count - 2u) << 16u) | (offset == 0 ? 0x68u : 0u);
                check(packet[0] == header, "incorrect context packet header");
                for (std::uint32_t i = 1; i < count; ++i) {
                    check(packet[i] == (offset == 0 && i == 1 ? operation : 0u), "incorrect context packet payload");
                }
                offset += count;
            }
        }
    }
    expectFailure([] { sceAgcDcbContextStateAnotherOp(nullptr, 0); });
    Storage invalid;
    expectFailure([&] { sceAgcDcbContextStateAnotherOp(&invalid.buffer, 4); });
    check(invalid.buffer.cursor_up == invalid.words.data(), "invalid context operation advanced cursor");
    invalid.buffer.cursor_up = invalid.words.data() + 1;
    invalid.buffer.cursor_down = invalid.words.data();
    expectFailure([&] { sceAgcDcbContextStateAnotherOp(&invalid.buffer, 0); });
    invalid.buffer.cursor_up = invalid.words.data();
    expectFailure([&] { sceAgcDcbContextStateAnotherOp(&invalid.buffer, 1); });
    ContextGrowth growth{{}, invalid.words.data(), 22};
    invalid.buffer.callback = growContext;
    invalid.buffer.user_data = &growth;
    growth.success = false;
    expectFailure([&] { sceAgcDcbContextStateAnotherOp(&invalid.buffer, 1); });
    growth.calls = 0;
    growth.success = true;
    growth.destination.buffer.cursor_down = growth.destination.words.data() + 21;
    expectFailure([&] { sceAgcDcbContextStateAnotherOp(&invalid.buffer, 1); });
    check(invalid.buffer.cursor_up == growth.destination.words.data(), "failed reservation advanced cursor");
}

void testFlip() {
    Storage storage;
    storage.words.fill(0xdeadbeefu);
    auto* packet = sceAgcDcbSetFlip(&storage.buffer, 0xfedcba98u, -2, 0x12345678u, -0x123456789abcdefLL);
    const std::array<std::uint32_t, 6> expected{0xc004105cu, 0xfedcba98u, 0xfffffffeu, 0x12345678u, 0x76543211u, 0xfedcba98u};
    check(packet == storage.words.data(), "flip returned wrong packet address");
    check(std::equal(expected.begin(), expected.end(), packet), "flip packet lost argument bits");
    check(storage.buffer.cursor_up == packet + 6 && packet[6] == 0xdeadbeefu, "flip packet overran allocation");
    expectFailure([] { sceAgcDcbSetFlip(nullptr, 1, 0, 1, 0); });
    Storage exhausted;
    exhausted.buffer.cursor_down = exhausted.words.data() + 5;
    expectFailure([&] { sceAgcDcbSetFlip(&exhausted.buffer, 1, 0, 1, 0); });
    check(exhausted.buffer.cursor_up == exhausted.words.data(), "failed flip allocation advanced cursor");
    check(sceAgcSuspendPoint() == 0, "empty suspend failed");
}

void testRegisters() {
    Storage storage;
    const std::array<ShaderRegister, 3> registers{{{0x10, 7}, {0x11, 8}, {0x20, 9}}};
    Agc::Command::WriteRegisters(&storage.buffer, 0x76u, registers.data(), registers.size(), true, __func__);
    const std::array<std::uint32_t, 7> expected{0xc0027600u, 0x10, 7, 8, 0xc0017600u, 0x20, 9};
    check(std::equal(expected.begin(), expected.end(), storage.words.begin()), "register run packet mismatch");
    auto* packet = Agc::Command::WriteIndirectRegisters(&storage.buffer, 0x63u, registers.data(), 0x3ffeu, __func__);
    Agc::Command::PatchIndirectCount(packet, 0x63u, 1, __func__);
    check(packet[4] == 0x3fffu, "indirect register count mismatch");
    const auto before = storage.words;
    expectFailure([&] { Agc::Command::PatchIndirectCount(packet, 0x63u, 1, __func__); });
    expectFailure([&] { Agc::Command::PatchIndirectAddress(packet, 0x64u, registers.data(), __func__); });
    check(storage.words == before, "invalid indirect patch modified memory");
}

void testRegisterRange() {
    Storage storage;
    storage.words.fill(0xdeadbeefu);
    auto* packet = sceAgcCbSetShRegisterRangeDirect(&storage.buffer, 0x8c, nullptr, 4);
    auto expected = storage.words;
    expected.fill(0xdeadbeefu);
    expected[0] = 0xc0047600u;
    expected[1] = 0x8c;
    check(packet == storage.words.data(), "incorrect register range packet address");
    check(storage.buffer.cursor_up == storage.words.data() + 6, "incorrect register range allocation");
    check(storage.words == expected, "null register values modified payload or adjacent memory");
    const std::array<std::uint32_t, 4> values{1, 2, 3, 4};
    packet = sceAgcCbSetShRegisterRangeDirect(&storage.buffer, 0x90, values.data(), values.size());
    expected[6] = 0xc0047600u;
    expected[7] = 0x90;
    std::copy(values.begin(), values.end(), expected.begin() + 8);
    check(packet == storage.words.data() + 6 && storage.buffer.cursor_up == storage.words.data() + 12, "incorrect populated register range allocation");
    check(storage.words == expected, "register values were not copied correctly");
    const auto* misaligned = reinterpret_cast<const std::uint32_t*>(reinterpret_cast<const unsigned char*>(values.data()) + 1);
    expectFailure([&] { sceAgcCbSetShRegisterRangeDirect(&storage.buffer, 0x8c, misaligned, 4); });
    check(storage.words == expected && storage.buffer.cursor_up == storage.words.data() + 12, "misaligned register values modified command buffer");
}

void testPacketPayloadAddress() {
    Storage storage;
    auto* packet = sceAgcCbSetShRegisterRangeDirect(&storage.buffer, 0x8c, nullptr, 4);
    std::uint32_t* payload = nullptr;
    check(sceAgcGetDataPacketPayloadAddressUnk(&payload, packet, 1) == 0 && payload == packet + 2, "incorrect register packet payload address");
    const std::array<std::uint32_t, 4> values{11, 22, 33, 44};
    std::copy(values.begin(), values.end(), payload);
    const std::array<std::uint32_t, 6> expected{0xc0047600u, 0x8c, 11, 22, 33, 44};
    check(std::equal(expected.begin(), expected.end(), packet), "payload write corrupted register packet");
    check(sceAgcGetDataPacketPayloadAddressUnk(&payload, packet, 0) == 0 && payload == packet + 1, "incorrect generic packet payload address");
    packet[0] = 0xffff1000u;
    check(sceAgcGetDataPacketPayloadAddressUnk(&payload, packet, 0) == 0 && payload == nullptr, "empty payload marker was not recognized");
    check(sceAgcGetDataPacketPayloadAddressUnk(&payload, packet, -1) == 0 && payload == packet + 2, "nonzero payload type did not skip two words");
    expectFailure([&] { sceAgcGetDataPacketPayloadAddressUnk(nullptr, packet, 1); });
    expectFailure([&] { sceAgcGetDataPacketPayloadAddressUnk(&payload, nullptr, 1); });
    auto* misaligned = reinterpret_cast<std::uint32_t*>(reinterpret_cast<unsigned char*>(packet) + 1);
    expectFailure([&] { sceAgcGetDataPacketPayloadAddressUnk(&payload, misaligned, 0); });
    check(payload == packet + 2, "invalid packet changed output address");
}

void testMemory() {
    Storage storage;
    Agc::Command::WriteDma(&storage.buffer, false, 1, 0, 0, 0x2000, 2, 0, 0x12345678, 16, 0, 1, 1, __func__);
    const std::array<std::uint32_t, 7> expected{0xc0055000u, 0xc0000001u, 0x12345678, 0, 0x2000, 0, 0x80000010u};
    check(std::equal(expected.begin(), expected.end(), storage.words.begin()), "DMA packet mismatch");
    std::uint64_t value = 0;
    auto* packet = Agc::Command::WriteWait(&storage.buffer, 1, 3, 0, 0, &value, 0x1122334455667788ull, 0xffffffffffffffffull, 32, __func__);
    check(packet[0] == 0xc0027901u && packet[4] == 0xc0079300u && packet[5] == 0x13u, "wait packet header mismatch");
    check(packet[8] == 0x55667788u && packet[9] == 0x11223344u && packet[12] == 2u, "wait reference or poll interval mismatch");
    sceAgcWaitRegMemPatchReference(packet, 7);
    check(packet[8] == 7 && packet[9] == 0x11223344u, "reference patch changed the high word");
    const auto before = storage.words;
    expectFailure([&] { sceAgcWaitRegMemPatchReference(packet, 0x100000000ull); });
    expectFailure([&] { Agc::Command::WriteWait(&storage.buffer, 0, 3, 0, 0, &value, 0x100000000ull, 0, 32, __func__); });
    check(storage.words == before, "invalid memory operation modified packet memory");
}

void testDefaults() {
    std::uint32_t state = 0x12345678;
    check(sceAgcInit(&state, 8) == 0 && state == 0x12345678, "AGC initialization failed or modified caller state");
    expectFailure([] { sceAgcInit(nullptr, 8); });
    expectFailure([&] { sceAgcInit(&state, 14); });
    for (std::uint32_t version = 0; version < 14; ++version) {
        for (const bool internal : {false, true}) {
            auto* first = Agc::Command::GetRegisterDefaults(version, internal, __func__);
            check(first != nullptr && first == Agc::Command::GetRegisterDefaults(version, internal, __func__), "unstable register defaults pointer");
        }
    }
    expectFailure([] { Agc::Command::GetRegisterDefaults(14, false, __func__); });
    expectFailure([] { Agc::Command::GetRegisterDefaults(0xffffffffu, true, __func__); });
}

}

int main() {
    try {
        testPackets();
        testContextState();
        testFlip();
        testRegisters();
        testRegisterRange();
        testPacketPayloadAddress();
        testMemory();
        testDefaults();
        LibcRunShutdown_nid_postfix();
        std::puts("AGC command tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { LibcRunShutdown_nid_postfix(); }
        catch (const std::exception& shutdown) { std::fprintf(stderr, "shutdown: %s\n", shutdown.what()); }
        return 1;
    }
}
