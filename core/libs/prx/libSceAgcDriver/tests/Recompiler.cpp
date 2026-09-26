#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    try {
        alignas(256) const std::array<std::uint32_t, 1> code{0xbf810000};
        Shader shader{};
        shader.file_header = 0x34333231;
        shader.version = 0x18;
        shader.header_size = sizeof(Shader);
        shader.shader_size = sizeof(code);
        shader.code = code.data();
        AgcDriverRegisterShader_nid_postfix(&shader);
        const auto address = reinterpret_cast<std::uintptr_t>(code.data());
        std::vector<std::uint32_t> commands{
            0xc0027600, 0x20c, static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u),
            0xc0017600, 0x213, 0,
            0xc0031500, 1, 1, 1, 0x8041
        };
        Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
        const std::array<std::uint32_t, 3> arguments{1, 1, 1};
        alignas(256) const std::array<std::uint32_t, 65> vertexCode{0xbf810000};
        Shader vertexShader = shader;
        const std::array<std::uint16_t, 4> indices{0, 1, 2, 0};
        std::uint32_t destination = 0;
        const bool graphics = argc == 2 && (std::string(argv[1]) == "draw" || std::string(argv[1]) == "vertex");
        if (graphics) {
            shader.type = 1;
            AgcDriverRegisterShader_nid_postfix(&shader);
            vertexShader.type = 2;
            vertexShader.shader_size = sizeof(vertexCode);
            vertexShader.code = vertexCode.data();
            AgcDriverRegisterShader_nid_postfix(&vertexShader);
            const auto vertexAddress = reinterpret_cast<std::uintptr_t>(vertexCode.data()) + 256;
            const auto indexAddress = reinterpret_cast<std::uintptr_t>(indices.data());
            const auto pixelAddress = std::string(argv[1]) == "vertex" ? 0 : address;
            commands = {
                0xc0027600, 0x0c8, static_cast<std::uint32_t>(vertexAddress >> 8u), static_cast<std::uint32_t>(vertexAddress >> 40u),
                0xc0017600, 0x08b, 0,
                0xc0027600, 0x008, static_cast<std::uint32_t>(pixelAddress >> 8u), static_cast<std::uint32_t>(pixelAddress >> 40u),
                0xc0017600, 0x00b, 0x08000002,
                0xc0217600, 0x00c
            };
            for (std::uint32_t i = 0; i < 33; ++i) commands.push_back(100 + i);
            const auto destinationAddress = reinterpret_cast<std::uintptr_t>(&destination);
            const std::array<std::uint32_t, 13> draw{
                0xc0012600, static_cast<std::uint32_t>(indexAddress), static_cast<std::uint32_t>(indexAddress >> 32u),
                0xc0033500, 3, 1, 3, 0x20,
                0xc0033700, 0x100, static_cast<std::uint32_t>(destinationAddress), static_cast<std::uint32_t>(destinationAddress >> 32u), 7
            };
            commands.insert(commands.end(), draw.begin(), draw.end());
            packet = Packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
        }
        if (argc == 2 && std::string(argv[1]) == "indirect") {
            const auto argumentAddress = reinterpret_cast<std::uintptr_t>(arguments.data());
            commands[7] = 0xc0021600;
            commands[8] = static_cast<std::uint32_t>(argumentAddress);
            commands[9] = static_cast<std::uint32_t>(argumentAddress >> 32u);
            commands[10] = 0x8041;
            packet.dw_num = 11;
        }
        if (graphics) sceAgcDriverSubmitDcb(&packet);
        else sceAgcDriverSubmitAcb(0x20, &packet);
        try {
            AgcDriverWaitIdle_nid_postfix();
        } catch (const std::runtime_error& error) {
            if (std::string(error.what()) != "ShaderRecompiler::Recompile not implemented") {
                throw;
            }
            if (destination != 0) throw std::runtime_error("failed draw executed a later memory write");
            std::puts("Vulkan device initialized; real recompiler exception propagated");
            try {
                AgcDriverWaitIdle_nid_postfix();
                throw std::runtime_error("idle lost recompiler failure");
            } catch (const std::runtime_error& idle) {
                if (std::string(idle.what()) != "ShaderRecompiler::Recompile not implemented") throw;
            }
            try {
                LibcRunShutdown_nid_postfix();
                throw std::runtime_error("shutdown lost recompiler failure");
            } catch (const std::runtime_error& shutdown) {
                if (std::string(shutdown.what()) != "ShaderRecompiler::Recompile not implemented") throw;
            }
            return 0;
        }
        throw std::runtime_error("dispatch unexpectedly completed without a recompiler");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { LibcRunShutdown_nid_postfix(); }
        catch (const std::exception& shutdown) { std::fprintf(stderr, "shutdown: %s\n", shutdown.what()); }
        return 1;
    }
}
