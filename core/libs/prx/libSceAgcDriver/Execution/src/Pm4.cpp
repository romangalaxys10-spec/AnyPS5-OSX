#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace AgcDriver::Pm4 {
namespace {

void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

std::uint64_t address(std::uint32_t low, std::uint32_t high) {
    return low | (static_cast<std::uint64_t>(high) << 32u);
}

std::uint32_t registerOffset(std::uint32_t value) {
    require(value != 0xffffffffu, "indirect register sentinel semantics are not implemented");
    const auto offset = value & ~0x70000000u;
    require(offset <= 0xffffu, "extended register semantics are not implemented");
    return offset;
}

Registers& registersFor(QueueState& queue, std::uint32_t opcode) {
    if (opcode == 0x69 || opcode == 0x9f) return queue.context;
    if (opcode == 0x76 || opcode == 0x63) return queue.shader;
    return queue.userConfig;
}

void writeRegister(QueueState& queue, std::uint32_t opcode, std::uint32_t offset, std::uint32_t value) {
    registersFor(queue, opcode).insert_or_assign(offset, value);
    if ((opcode == 0x64 || opcode == 0x79 || opcode == 0x7a) && offset == 0x243) queue.indexType = value & 3u;
}

bool memorySelector(std::uint32_t selector) {
    return selector == 0 || selector == 3;
}

std::uint32_t dmaSource(std::span<const std::uint32_t> packet) {
    return ((packet[1] >> 29u) & 3u) | ((packet[6] >> 24u) & 4u) | ((packet[6] >> 25u) & 8u);
}

std::uint32_t dmaDestination(std::span<const std::uint32_t> packet) {
    return ((packet[1] >> 20u) & 3u) | ((packet[6] >> 25u) & 4u) | ((packet[6] >> 26u) & 8u);
}

void copyMemory(std::uint64_t source, std::uint64_t destination, std::size_t bytes, bool immediate) {
    if (bytes == 0) return;
    GuestMemory::CheckRange(reinterpret_cast<void*>(destination), bytes, 1, true);
    std::vector<std::byte> data(bytes);
    if (immediate) {
        const auto value = static_cast<std::uint32_t>(source);
        for (std::size_t i = 0; i < bytes; ++i) data[i] = static_cast<std::byte>(value >> ((i % 4) * 8));
    } else {
        GuestMemory::Read(source, data);
    }
    GuestMemory::Write(destination, data);
}

}

std::string Name(std::uint32_t header) {
    const auto opcode = (header >> 8u) & 0xffu;
    if (opcode == 0x10 && (header & 0xfcu) != 0) {
        switch ((header >> 2u) & 0x3fu) {
            case 0x05: return "DRAW_RESET";
            case 0x06: return "WAIT_FLIP_DONE";
            case 0x09: return "DISPATCH_RESET";
            case 0x0b: return "PUSH_MARKER";
            case 0x0c: return "POP_MARKER";
            case 0x14: return "ACQUIRE_MEM_CUSTOM";
            case 0x15: return "WRITE_DATA_CUSTOM";
            case 0x17: return "FLIP";
            case 0x18: return "RELEASE_MEM_CUSTOM";
            case 0x19: return "DMA_DATA_CUSTOM";
            case 0x1a: return "CONTEXT_STATE";
            default: return "UNKNOWN_CUSTOM";
        }
    }
    for (const auto& entry : Opcodes) if (entry.value == opcode) return std::string(entry.name);
    char text[24]{};
    std::snprintf(text, sizeof(text), "UNKNOWN_0x%02x", opcode);
    return text;
}

std::string_view UnsupportedReason(std::uint32_t header) {
    const auto opcode = (header >> 8u) & 0xffu;
    if (opcode == 0x10) {
        switch ((header >> 2u) & 0x3fu) {
            case 0: case 0x06: case 0x09: case 0x0b: case 0x0c: case 0x17: case 0x1a: return {};
            case 0x14: case 0x18: return "guest cache actions and GPU release events are not implemented";
            default: return "custom packet has no implemented contract in the reference dispatch table";
        }
    }
    switch (opcode) {
        case 0x11: case 0x12: case 0x13: case 0x15: case 0x16: case 0x26:
        case 0x2a: case 0x2d: case 0x2f: case 0x35: case 0x37: case 0x40: case 0x42: case 0x46: case 0x50:
        case 0x58: case 0x63: case 0x64: case 0x69: case 0x76: case 0x79: case 0x7a:
        case 0x81: case 0x83: case 0x9f: return {};
        case 0x24: case 0x25: case 0x27: case 0x2c: case 0x38: case 0x3a: case 0x8d:
            return "graphics draw, shader stages and guest render-target materialization are not implemented";
        case 0x20: return "GPU query predication is not implemented";
        case 0x22: return "conditional command execution and conditional flip reservation are not implemented";
        case 0x33: case 0x3f: return "nested command buffers, branching and nested flip reservation are not implemented";
        case 0x39: case 0x3c: case 0x59: case 0x93:
            return "cooperative command-queue waits are not implemented";
        case 0x84: case 0x85: case 0x86: case 0x88:
            return "separate CE/DE execution and counter synchronization are not implemented";
        case 0x43: case 0x47: case 0x48: case 0x49:
            return "guest cache actions, GPU events and interrupt delivery are not implemented";
        case 0x8e: return "GPU LOD statistics are not implemented; synthetic results are forbidden";
        case 0x28: case 0x41: case 0x68: case 0x78:
            return "opcode is named but has no handler in the reference dispatch table";
        default: return "opcode is not known in the reference";
    }
}

void Validate(std::span<const std::uint32_t> packet, std::uint32_t queue) {
    require(packet.size() >= 2, "truncated PM4 header or payload");
    const auto header = packet[0];
    require((header & 0xc0000000u) == 0xc0000000u, "unsupported PM4 packet type");
    require(packet.size() == ((header >> 16u) & 0x3fffu) + 2u, "invalid PM4 packet size");
    const auto opcode = (header >> 8u) & 0xffu;
    const auto size = [&](std::size_t count) { require(packet.size() == count, "invalid packet size"); };
    const auto graphics = [&] { require(queue == 0, "graphics packet in compute queue"); };
    const auto reason = UnsupportedReason(header);
    if (!reason.empty()) throw std::runtime_error(std::string(reason));
    if (opcode == 0x10) {
        require((header & 3u) == 0, "unsupported NOP header flags");
        switch ((header >> 2u) & 0x3fu) {
            case 0:
                require((packet[1] & 0xffff0000u) != 0x68750000u, "typed user-data and legacy flip markers are not implemented");
                break;
            case 0x09: size(2); break;
            case 0x06: graphics(); size(4); require(packet[3] == 0, "unsupported rendering wait mode"); break;
            case 0x0b: {
                const auto data = std::as_bytes(packet.subspan(1));
                require(std::find(data.begin(), data.end(), std::byte{}) != data.end(), "unterminated marker text");
                break;
            }
            case 0x0c: break;
            case 0x17: graphics(); size(6); break;
            case 0x1a:
                graphics();
                require(packet.size() == 3 || packet.size() == 5, "invalid context-state packet size");
                require(packet[1] <= 3, "unknown context-state operation");
                require(std::all_of(packet.begin() + 2, packet.end(), [](auto value) { return value == 0; }), "context-state trailing fields are not implemented");
                break;
        }
        return;
    }
    require((header & 0xffu) == 0 || (opcode == 0x11 && (header & 0xffu) == 2), "PM4 header flags are not implemented");
    switch (opcode) {
        case 0x11:
            size(4);
            require(packet[1] == 1 && (packet[2] & 7u) == 0 && packet[3] <= 0xffffu, "unsupported indirect base index, alignment or address bits");
            if ((header & 2u) == 0) graphics();
            break;
        case 0x12: graphics(); size(2); require((packet[1] & ~0xfu) == 0, "unsupported CLEAR_STATE payload bits"); break;
        case 0x13: case 0x2f: graphics(); size(2); break;
        case 0x26: graphics(); size(3); break;
        case 0x2a: graphics(); size(2); require(packet[1] <= 3, "unsupported index-type modifiers"); break;
        case 0x2d:
            graphics();
            size(3);
            require((packet[2] & ~0x20u) == 2u, "unsupported auto draw flags");
            break;
        case 0x35:
            graphics();
            size(5);
            require(packet[3] <= packet[1], "index count exceeds maximum index size");
            require((packet[4] & ~0x20u) == 0, "unsupported indexed draw flags");
            break;
        case 0x15: size(5); require((packet[4] & ~0x8000u) == 0x41u, "dispatch modifiers are not implemented"); break;
        case 0x16:
            require(packet.size() == 3 || packet.size() == 4, "invalid indirect dispatch size");
            require((packet.back() & ~0x8000u) == 0x41u, "indirect dispatch modifiers are not implemented");
            break;
        case 0x42: size(2); require(packet[1] == 0, "unsupported PFP_SYNC_ME payload"); break;
        case 0x46: {
            require((packet[1] & ~0x73fu) == 0, "unsupported EVENT_WRITE flags or reserved bits");
            const auto eventType = packet[1] & 0x3fu;
            const auto eventIndex = (packet[1] >> 8u) & 7u;
            switch (eventType) {
                case 0x07: case 0x0f: case 0x10:
                    size(2);
                    require(eventIndex == 4, "invalid partial-flush event index");
                    if (eventType != 0x07) graphics();
                    break;
                case 0x16: case 0x31: case 0x2a: case 0x2c: case 0x2e:
                    graphics();
                    size(2);
                    require(eventIndex == 0 || eventIndex == 7, "invalid cache-flush event index");
                    break;
                default: throw std::runtime_error("EVENT_WRITE event type " + std::to_string(eventType) + " is not implemented");
            }
            break;
        }
        case 0x58: {
            require(packet.size() == 7 || packet.size() == 8, "invalid ACQUIRE_MEM packet size");
            const auto controlMask = packet.size() == 8 ? 0x86287fc3u : 0xfeecfffbu;
            require((packet[1] & ~controlMask) == 0, "unsupported ACQUIRE_MEM control flags");
            require(queue == 0 || (packet[1] & 0x06287fc3u) == 0, "graphics cache operation in compute queue");
            require(packet[3] == 0 && packet[5] == 0, "ACQUIRE_MEM ranges above 40 bits are not implemented");
            require(packet[6] <= 0xffffu, "invalid ACQUIRE_MEM poll interval");
            const auto base = static_cast<std::uint64_t>(packet[4]) << 8u;
            const auto bytes = static_cast<std::uint64_t>(packet[2]) << 8u;
            require(bytes <= (1ull << 40u) - base, "ACQUIRE_MEM range exceeds 40-bit address space");
            if (packet.size() == 8) {
                require((packet[7] & ~0x3ffffu) == 0, "unsupported ACQUIRE_MEM GCR flags");
                require((packet[7] & 0x2000u) == 0, "ACQUIRE_MEM cache discard is not implemented");
            }
            break;
        }
        case 0x63: case 0x64: case 0x9f:
            if (opcode != 0x63) graphics();
            size(5);
            require((packet[1] & 3u) == 0 && packet[3] == 0x80000000u && packet[4] <= 0x3fffu, "unsupported indirect-register address or control fields");
            break;
        case 0x69: case 0x76: case 0x79: case 0x7a: {
            if (opcode != 0x76) graphics();
            require(packet.size() >= 3, "register packet has no values");
            if (opcode == 0x7a) require((packet[1] & 0xf0000000u) == 0 || (packet.size() == 3 && packet[1] == 0x20000243u), "indexed register bank selection is not implemented");
            const auto offset = registerOffset(packet[1]);
            require(packet.size() - 2 <= 0x10000u - offset, "register range overflow");
            break;
        }
        case 0x81:
            graphics();
            require(packet[1] <= 0xbffcu && (packet[1] & 3u) == 0 && packet.size() - 2 <= 0x3000u - packet[1] / 4u, "constant RAM write range overflow or misalignment");
            break;
        case 0x83:
            graphics(); size(5);
            require(packet[1] <= 0xbffcu && (packet[1] & 3u) == 0 && packet[2] <= 0x3000u - packet[1] / 4u, "constant RAM dump range overflow or misalignment");
            break;
        case 0x37: {
            require(packet.size() >= 5, "WRITE_DATA has no data");
            require((packet[1] & ~0x00110f00u) == 0, "WRITE_DATA engine, cache or reserved fields are not implemented");
            const auto destination = (packet[1] >> 8u) & 0xfu;
            require(destination == 1 || destination == 2 || (queue != 0 && destination == 5), "WRITE_DATA register or GDS destination is not implemented");
            require((packet[2] & 3u) == 0, "misaligned WRITE_DATA destination");
            break;
        }
        case 0x40: {
            size(6);
            require((packet[1] & ~0x40110f0fu) == 0, "COPY_DATA engine, cache or reserved fields are not implemented");
            const auto source = ((packet[1] & 0xfu) << 1u) | ((packet[1] >> 30u) & 1u);
            const auto destination = ((packet[1] >> 8u) & 0xfu) << 1u;
            require(destination == 2 || destination == 4, "COPY_DATA register or GDS destination is not implemented");
            require(source == 2 || source == 4 || source == 5 || source == 10 || source == 11, "COPY_DATA register, GDS or reference-clock source is not implemented");
            require(source < 10 || ((packet[1] & 0x10000u) == 0 && packet[3] == 0), "64-bit immediate COPY_DATA is not implemented");
            break;
        }
        case 0x50:
            size(7);
            require((packet[1] & ~0xe0300001u) == 0, "DMA_DATA cache or reserved fields are not implemented");
            require(memorySelector(dmaDestination(packet)), "DMA_DATA register, GDS or prefetch destination is not implemented");
            require(memorySelector(dmaSource(packet)) || dmaSource(packet) == 2, "DMA_DATA register or GDS source is not implemented");
            require(dmaSource(packet) != 2 || packet[3] == 0, "DMA_DATA immediate exceeds 32 bits");
            break;
        default: throw std::runtime_error("known packet has no validator");
    }
}

bool UsesGpuCacheBarrier(std::span<const std::uint32_t> packet) {
    require(!packet.empty() && ((packet[0] >> 8u) & 0xffu) == 0x58, "cache barrier requires ACQUIRE_MEM");
    Validate(packet, 0);
    return packet.size() == 8 && (packet[7] & 0xfc00u) == 0;
}

bool AccessesMemory(std::uint32_t header) {
    switch ((header >> 8u) & 0xffu) {
        case 0x16: case 0x2d: case 0x35: case 0x37: case 0x40: case 0x50: case 0x63: case 0x64: case 0x83: case 0x9f: return true;
        default: return false;
    }
}

std::array<std::uint32_t, 5> ResolveDispatch(std::span<const std::uint32_t> packet, const QueueState& queue) {
    std::uint64_t source = 0;
    if (packet.size() == 4) source = address(packet[1], packet[2]);
    else {
        require(queue.dispatchIndirectBase != 0, "indirect dispatch base has not been set");
        require(packet[1] <= std::numeric_limits<std::uint64_t>::max() - queue.dispatchIndirectBase, "indirect dispatch address overflow");
        source = queue.dispatchIndirectBase + packet[1];
    }
    std::array<std::uint32_t, 5> result{0xc0031500u, 0, 0, 0, packet.back()};
    GuestMemory::Read(source, std::as_writable_bytes(std::span(result).subspan(1, 3)), 4);
    return result;
}

DrawParameters ResolveDraw(std::span<const std::uint32_t> packet, const QueueState& queue) {
    Validate(packet, 0);
    if (((packet[0] >> 8u) & 0xffu) == 0x2d) {
        const auto offset = queue.userConfig.find(0x24a);
        require(offset != queue.userConfig.end(), "missing GE_INDX_OFFSET register");
        const auto firstVertex = offset->second;
        require(packet[1] == 0 || firstVertex <= std::numeric_limits<std::uint32_t>::max() - (packet[1] - 1u), "auto draw vertex range overflow");
        return {0, packet[1], 0, queue.instanceCount, packet[2] & 0x20u, false, firstVertex, 0};
    }
    require(((packet[0] >> 8u) & 0xffu) == 0x35, "expected DRAW_INDEX_OFFSET_2 packet");
    require(queue.indexType <= 2, "unsupported index type");
    const std::uint32_t indexSize = queue.indexType == 0 ? 2 : queue.indexType == 1 ? 4 : 1;
    require(queue.indexBase != 0 && queue.indexBase % indexSize == 0, "null or misaligned index base");
    const auto offset = static_cast<std::uint64_t>(packet[2]) * indexSize;
    require(offset <= std::numeric_limits<std::uint64_t>::max() - queue.indexBase, "index address overflow");
    const auto address = queue.indexBase + offset;
    const auto bytes = static_cast<std::uint64_t>(packet[3]) * indexSize;
    require(bytes <= std::numeric_limits<std::size_t>::max(), "index range size overflow");
    GuestMemory::CheckRange(reinterpret_cast<const void*>(address), static_cast<std::size_t>(bytes), indexSize);
    return {address, packet[3], indexSize, queue.instanceCount, packet[4]};
}

void Execute(std::span<const std::uint32_t> packet, QueueState& queue) {
    const auto opcode = (packet[0] >> 8u) & 0xffu;
    switch (opcode) {
        case 0x10:
            switch ((packet[0] >> 2u) & 0x3fu) {
                case 0: return;
                case 0x09: queue = QueueState{}; return;
                case 0x0b: queue.markers.emplace_back(reinterpret_cast<const char*>(packet.data() + 1)); return;
                case 0x0c:
                    require(!queue.markers.empty(), "marker stack underflow");
                    queue.markers.pop_back();
                    return;
                case 0x1a:
                    switch (packet[1]) {
                        case 0: queue.ClearContext(); break;
                        case 1: case 3:
                            require(!queue.savedContext.has_value(), "context state is already pushed");
                            queue.savedContext = queue.context;
                            if (packet[1] == 3) queue.ClearContext();
                            break;
                        case 2:
                            require(queue.savedContext.has_value(), "context state has not been pushed");
                            queue.context = std::move(*queue.savedContext);
                            queue.savedContext.reset();
                            break;
                    }
                    return;
                default: throw std::runtime_error("custom packet requires driver execution");
            }
        case 0x11:
            ((packet[0] & 2u) == 0 ? queue.drawIndirectBase : queue.dispatchIndirectBase) = address(packet[2], packet[3]);
            return;
        case 0x12:
            queue.ClearContext(); return;
        case 0x13: queue.indexBufferSize = packet[1]; return;
        case 0x26: queue.indexBase = address(packet[1], packet[2]); return;
        case 0x2a: queue.indexType = packet[1]; return;
        case 0x2f: queue.instanceCount = packet[1]; return;
        case 0x63: case 0x64: case 0x9f: {
            std::vector<std::uint32_t> pairs(static_cast<std::size_t>(packet[4]) * 2);
            GuestMemory::Read(address(packet[1], packet[2]), std::as_writable_bytes(std::span(pairs)), 4);
            for (std::size_t i = 0; i < pairs.size(); i += 2) registerOffset(pairs[i]);
            for (std::size_t i = 0; i < pairs.size(); i += 2) writeRegister(queue, opcode, registerOffset(pairs[i]), pairs[i + 1]);
            return;
        }
        case 0x69: case 0x76: case 0x79: case 0x7a: {
            const auto offset = registerOffset(packet[1]);
            for (std::size_t i = 2; i < packet.size(); ++i) writeRegister(queue, opcode, offset + static_cast<std::uint32_t>(i - 2), packet[i]);
            return;
        }
        case 0x81:
            std::copy(packet.begin() + 2, packet.end(), queue.constantRam.begin() + packet[1] / 4);
            return;
        case 0x83:
            GuestMemory::Write(address(packet[3], packet[4]), std::as_bytes(std::span(queue.constantRam).subspan(packet[1] / 4, packet[2])), 4);
            return;
        case 0x37: {
            const auto destination = address(packet[2], packet[3]);
            if ((packet[1] & 0x10000u) != 0) {
                for (const auto& value : packet.subspan(4)) GuestMemory::Write(destination, std::as_bytes(std::span(&value, 1)), 4);
            } else GuestMemory::Write(destination, std::as_bytes(packet.subspan(4)), 4);
            return;
        }
        case 0x40: {
            const auto source = ((packet[1] & 0xfu) << 1u) | ((packet[1] >> 30u) & 1u);
            copyMemory(address(packet[2], packet[3]), address(packet[4], packet[5]), (packet[1] & 0x10000u) != 0 ? 8 : 4, source >= 10);
            return;
        }
        case 0x50:
            copyMemory(address(packet[2], packet[3]), address(packet[4], packet[5]), packet[6] & 0x3ffffffu, dmaSource(packet) == 2);
            return;
        default: throw std::runtime_error("packet requires driver execution");
    }
}

}
