# AnyPS5 — OSX Edition (Hybrid Suite)

Native macOS port of [AnyPS5](https://github.com/boykopovar/AnyPS5) with **Metal + MetalFX acceleration**, bundled into a **hybrid PS5 + Nintendo Switch** suite together with [Ryujinx](https://github.com/alula/Ryujinx) and an optional [OptiScaler](https://github.com/optiscaler/OptiScaler) enhancement layer for Windows.

> Upstream: `boykopovar/AnyPS5` — this edition tracks upstream `main` and preserves GPL-2.0.
> See `NOTICE` for component provenance and `docs/OSX_EDITION_REPORT.md` for the full engineering report.

## What this edition adds

| Area | Upstream (Linux/Windows) | OSX Edition |
|---|---|---|
| Relinker output | ELF / PE | ELF / PE / **Mach-O x86_64** (`--macos`) |
| GPU execution | Vulkan | Vulkan (MoltenVK) **+ native Metal backend** (`ANYPS5_GPU_BACKEND=metal`) |
| Presentation | Vulkan swapchain | Vulkan swapchain or **CAMetalLayer + MSL composite pipeline** |
| Upscaling | Bilinear (`PresentationScaler`) | Bilinear or **MetalFX SpatialScaler** (macOS 14+) |
| Shader translation | GNM/RDNA → SPIR-V | GNM/RDNA → SPIR-V, plus **SPIR-V → MSL** (SPIRV-Cross) for native Metal compute |
| Windowing | SDL2 (X11/Win32) | SDL2 (Cocoa, `SDL_WINDOW_METAL`, high-DPI drawable sizing) |
| Exception tables | `.eh_frame` / `.ehmeta` | same relinker tables, resolved via `dyld` + `getsectiondata` |
| Engines | PS5 only | **PS5 + Switch** via unified `anyps5-launcher` |

## Repository layout

```
core/            AnyPS5 core (relinker, shader recompiler, prx system libraries)
  relinker/      + elfpatcher/macos/MacosMachoPatcher (ELF → Mach-O)
  libs/prx/libSceAgcDriver/Execution/  GpuDevice interface, VulkanDevice (MoltenVK),
                                       MetalDevice.mm (Metal + MetalFX), GpuDeviceFactory
launcher/        anyps5-launcher — hybrid PS5/Switch game orchestrator
switch/Ryujinx   Nintendo Switch engine (real Ryujinx C# source, MIT, pinned submodule)
scripts/         macOS build/probe/MoltenVK/Switch-engine helpers
docs/            OSX edition engineering report + upstream docs
```

## Build (macOS)

Requirements: Xcode 16+ (macOS 14/15 SDK for MetalFX), CMake 3.20+, Ninja, MoltenVK for the Vulkan fallback path.

```bash
./scripts/build-macos.sh                 # x86_64 default, ARCH=arm64 supported
brew install molten-vk                   # Vulkan-on-Metal fallback runtime
./scripts/fetch-moltenvk.sh build/libs   # or bundle libMoltenVK.dylib next to binaries
```

Backend selection at runtime:

```bash
export ANYPS5_GPU_BACKEND=metal    # native Metal + MetalFX presentation (default on macOS)
export ANYPS5_GPU_BACKEND=vulkan   # full-fidelity execution via MoltenVK
```

The Metal backend implements the complete presentation path (clear / pixel upload / display buffer composite) with MetalFX spatial upscaling and SPIR-V→MSL compute dispatch. Guest graphics pipelines route through the Vulkan/MoltenVK backend; the Metal backend throws a precise `std::runtime_error` for unsupported graphics states, matching upstream's strict-failure philosophy.

## Build (Linux / Windows)

Unchanged from upstream; see `docs/UPSTREAM_README.md`. Windows requires MinGW-w64 GCC 15.x (winlibs, `x86_64-ucrt-posix-seh`).

## Switch engine

The Nintendo Switch engine is the real Ryujinx source (MIT), pinned as `switch/Ryujinx`:

```bash
git submodule update --init switch/Ryujinx
dotnet publish switch/Ryujinx/src/Ryujinx.Ava -c Release -r osx-x64 --self-contained false -o publish/osx
```

`anyps5-launcher` auto-detects the title format (PS5: `.elf/.self` + `sce_sys/param.sfo`; Switch: `.nsp/.xci/.nca/.nro`) and routes to the right engine.

## OptiScaler (Windows, optional)

OptiScaler (GPL-3.0, kept license-isolated from this GPL-2.0 codebase — no source merge) can be dropped next to a relinked Windows title to bridge DLSS/XeSS/FSR frame generation. The launcher's `--optiscaler` flag stages `OptiScaler.ini` + `nvngx.dll` when an `optiscaler/` directory is present.

## Disclaimer

This project is intended for interoperability, research, preservation, and compatibility purposes. It does not include, distribute, or require copyrighted software, firmware, cryptographic keys, or proprietary libraries. Users are responsible for ensuring that any binaries used with this project are obtained and used in accordance with applicable laws and their respective license terms.

## License

GPL-2.0 (see LICENSE). Component licenses: Ryujinx MIT, OptiScaler GPL-3.0 (external component, not merged), MoltenVK Apache-2.0 (runtime dependency), SPIRV-Cross Apache-2.0/MIT/ISC/Khronos (used under MIT).
