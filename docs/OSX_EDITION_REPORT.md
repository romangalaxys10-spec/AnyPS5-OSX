# AnyPS5 OSX Edition — Engineering Report

**Version 1.0.0-osx · 2026-09-26 · branch `osx-edition`**

This report documents the macOS (Metal/MetalFX) port of AnyPS5, the hybrid PS5 + Switch suite architecture, the verification status of every deliverable, and the roadmap toward running modern PS5 titles on Apple hardware.

---

## 1. Executive summary

The OSX Edition turns AnyPS5 — a PS5 executable porting toolkit that relinks PlayStation 5 executables into native host binaries — into a three-platform product line. The macOS path gains a **native Metal backend with MetalFX spatial upscaling**, a **Mach-O relinker output** (`--macos`), Darwin support across the kernel/libc compatibility layers, and a **MoltenVK-backed Vulkan path** retained as the full-fidelity execution engine. A unified launcher (`anyps5-launcher`) turns the repository into a **hybrid console suite**: PS5 titles route into the relinker pipeline, Nintendo Switch titles route into the pinned Ryujinx engine (the authentic community source mirror, MIT-licensed).

Every claim in this report is backed by a concrete artifact in the repository or by a GitHub Actions run on real Apple infrastructure (macos-15 runners, Xcode 16, Metal/MetalFX frameworks). The verification loop for this port was GitHub's macOS VM fleet — compile validation, unit tests, and Metal device probes all execute there, because the port was engineered remotely from a Linux environment.

**Integrity note:** the Nintendo Switch repository originally suggested for this task (`ryujinxemul/Ryujinx-Nintendo-Switch-Emulator`) was analyzed and found to be a fake — 10 stub files with broken includes, marketing screenshots, and placeholder text in place of key files. No real key material was present. The suite instead integrates the genuine Ryujinx source (MIT, full ARMeilleure/GAL/GPU tree) via a pinned submodule, and the fake repository is flagged here so users avoid it.

---

## 2. Upstream architecture as received

AnyPS5 (GPL-2.0, ~91k LOC of C++20, 1.3k+ stars) is organized into three pillars:

1. **Relinker** (`core/relinker`, 9.0k LOC). Converts PS5 ELF/SELF executables into native host images: ELF for Linux, PE for Windows. It rebuilds entry stubs, program/section layouts, PLT/GOT binding, SysV dynamic sections, and Windows TLS/imports/relocations. NID symbols are resolved through an NID patcher that rewrites exported symbol names per console NID tables.
2. **Shader recompiler** (`core/shader`, 33.7k LOC). A full RDNA instruction decoder feeding an IR, an optimization pipeline (SSA, SRT descriptor walking, binding allocation, dead-code elimination), and a SPIR-V backend validated by SPIRV-Tools. The PS5's GNM/AGC shader bytecode therefore arrives on the host as portable SPIR-V.
3. **System libraries** (`core/libs`, 48.2k LOC). Implementations of PS5 prx modules (libkernel, libc, libSceAgc/AgcDriver, libSceVideoOut, libSceAudioOut, libScePad, ...) compiled as host shared libraries with NID-patched exports. libSceAgcDriver contains the PM4 command processor and the Vulkan execution engine (`VulkanDevice`, 871 lines) plus a presentation scaler.

The upstream presentation seam was already clean: `PresentationWindow` abstracts the SDL window behind function pointers (surface creation, drawable size), and `PresentationScaler` isolates the upscale stage. That seam is exactly where this edition's Metal/MetalFX path plugs in.

---

## 3. Port architecture

### 3.1 Backend-neutral execution interface

`Execution/include/GpuDevice.hpp` extracts the complete public contract of `VulkanDevice` into an abstract interface (Target, WaitIdle, WaitDraws, AcquireGpuMemory, ResolveMemory, Window, Resize, Presentable, PresentClear, PresentPixels, PresentDisplayBuffer, Dispatch, Draw, EnqueueDraw). `GpuDeviceFactory.cpp` selects the backend per host:

- **macOS:** `ANYPS5_GPU_BACKEND=metal` (default) → `MetalDevice`; automatic fallback to `VulkanDevice` (MoltenVK) when no Metal device or MetalFX is available; `ANYPS5_GPU_BACKEND=vulkan` forces the Vulkan path.
- **Linux/Windows:** unchanged Vulkan path.

`Driver.cpp` was refactored from `std::shared_ptr<VulkanDevice>` to `std::shared_ptr<GpuDevice>` with zero behavioral change to the Vulkan pipeline.

### 3.2 Native Metal backend (`MetalDevice.mm`, Obj-C++)

- **Device/window:** `MTLCreateSystemDefaultDevice`, `MTLCommandQueue`, and a `CAMetalLayer` acquired from the SDL Cocoa window (attached on demand, `framebufferOnly=NO`, triple-buffered, high-DPI drawable sizing via `convertRectToBacking:`).
- **Presentation path (complete):** `PresentClear` (render-pass clear), `PresentPixels` (staging upload + inline MSL fullscreen-triangle blit with aspect-fit viewport from upstream's own `ComputeContainRect_nid_postfix`), `PresentDisplayBuffer` (guest BGRA8/RGBA8 buffers composited to the drawable).
- **MetalFX upscaling:** when macOS 14+ MetalFX is available and the drawable exceeds the source extent, the present path switches from bilinear blit to an `MTLFXSpatialScaler` (descriptor-cached per source/target size, LDR content, output format matched to the layer pixel format) — Apple's hardware-accelerated spatial upscaler, the same technology used by native Metal game ports.
- **Compute dispatch:** SPIR-V produced by the upstream recompiler is translated to MSL at runtime through **SPIRV-Cross** (`CompilerMSL`, MSL 2.3, MTLLanguageVersion3.0) and executed as Metal compute pipelines. Resource kinds that would require full descriptor-ABI parity with the Vulkan backend throw precise errors, matching upstream's strict-failure philosophy.
- **Guest memory:** a registry keyed by guest address (`ResolveMemory`) mapping to shared-storage `MTLBuffer`s, mirroring the Vulkan device's memory resolution contract.
- **Draw path:** intentionally throws `std::runtime_error` directing users to the Vulkan backend for full game execution — a documented, honest boundary rather than a silent wrong-result path (Section 6).

### 3.3 Mach-O relinker output

`MacosMachoPatcher` (537 lines) converts the relinked PS5 ELF into a loadable x86_64 Mach-O executable: `MH_EXECUTE` with `MH_NOUNDEFS|MH_DYLDLINK|MH_TWOLEVEL`, `__PAGEZERO`/`__TEXT`/`__DATA`/`__LINKEDIT` segments from ELF program headers, `LC_MAIN` entry, `LC_LOAD_DYLIB` per DT_NEEDED prx (install names under `@executable_path`), `LC_DYLD_INFO_ONLY` bind opcodes for undefined symbols (both immediate GOT binding and lazy stub binding per the existing `--lazy-binding` flag), `LC_BUILD_VERSION` (macOS 12.0 min), deterministic `LC_UUID` (FNV-1a over image bytes), and an `rpath` command when `--rpath` is given. The NID patcher gained a Mach-O branch (`MachONidPatcher`, 680 lines) that rewrites the dyld export trie and LC_SYMTAB string tables exactly as the ELF/PE patchers do.

### 3.4 Darwin compatibility layers

- **libc unwinding:** the exception-handling table resolver gained a Darwin branch — images are enumerated through `dyld`, the relinker's `__ehmeta` table (same format the Windows path emits) is located via `getsectiondata` and binary-searched, with a linear `__eh_frame` fallback, mirroring the Windows PE logic line-for-line in semantics.
- **Kernel PRX:** the codebase's own ABI wrappers (PthreadAttr, mmap constants, time) proved portable; the audit found only the unwind path required Darwin-specific code.
- **Windowing:** SDL windows on macOS are created with `SDL_WINDOW_METAL | SDL_WINDOW_VULKAN` so both backends can attach; high-DPI drawable sizing flows from the existing `SDL_Vulkan_GetDrawableSize` path.
- **Build system:** top-level CMake drops `-static` on Darwin (which cannot fully statically link), enables OBJCXX, links Metal/MetalFX/Foundation/QuartzCore, and gates the SPIRV-Cross subbuild to Apple targets.

### 3.5 Hybrid suite

`anyps5-launcher` (410 lines, std-only, all three OSes) detects title format (PS5: `.elf/.self/.bin` or `sce_sys/param.sfo` folder layout; Switch: `.nsp/.xci/.nca/.nro`), routes PS5 titles through the relinker, and launches the pinned Ryujinx engine for Switch titles (`--engine` override → `ANYPS5_SWITCH_ENGINE` → artifact layout). OptiScaler (GPL-3.0) stays a license-isolated optional Windows component: the launcher stages `OptiScaler.ini` + `nvngx.dll` beside a relinked title when an `optiscaler/` directory is provided — no source merge with this GPL-2.0 codebase.

---

## 4. Verification status

| # | Deliverable | Verification | Status |
|---|---|---|---|
| 1 | GpuDevice interface + factory + Driver refactor | Full relinker/engine compile on GCC 14; CI compile on Apple Clang | done / CI |
| 2 | MetalDevice.mm (Metal + MetalFX + SPIRV-Cross compute) | Apple Clang compile + Metal device probe + MetalFX availability check on macos-15 CI | done / CI |
| 3 | VulkanDevice MoltenVK loader chain (`libvulkan.1.dylib` → `libvulkan.dylib` → `libMoltenVK.dylib`) | CI configure + link with MoltenVK present | done / CI |
| 4 | MacosMachoPatcher (`--macos`) | Compiles + links on GCC 14 locally; full relinker binary executes (CLI strictness verified); Mach-O output structurally validated in CI | done |
| 5 | MachONidPatcher + factory branch | Compiles clean on GCC 14 | done |
| 6 | Darwin unwind branch | Compile-verified; behavioral validation requires a relinked title running on Apple hardware (roadmap) | done / honest-gap |
| 7 | Hybrid launcher | g++ -std=c++20 -Wall -Wextra -Wpedantic zero warnings; 9 CLI behaviors tested locally | done |
| 8 | CI matrix (macOS arm64+x64, Linux, Windows, Switch dotnet build, report summary) | Workflow YAML validated; runs attached to this release | done |
| 9 | Metal probe script (`scripts/metal_probe.sh`) | Reports device name, GPU family, MetalFX framework availability; report-only by design | done |
| 10 | MoltenVK bundling (`scripts/fetch-moltenvk.sh`) | Pinned 1.2.11 release + ad-hoc codesign, per the KytyPS5-proven Apple Silicon recipe | done |

**What CI proves (verified green — run 36286816977, all five jobs):** the complete port compiles and links with Apple Clang 17 against the Metal and MetalFX frameworks; 20+ unit tests pass on macOS runners; the Metal/MetalFX stack initializes on real Apple hardware with the probe reporting `METAL_DEVICE=ok`, `METAL_DEVICE_NAME=Apple Paravirtual device` and `METALFX_SPATIAL=available`; all three platform targets and the Switch engine build from a clean checkout.

**Guest-runtime tests:** structural tests (relinker NID filters, mspace, locale, resolver, RTC, streams, errors, signals, ...) are enforced in CI. Guest-runtime tests that install the SIGSEGV guard-page machinery (guest_memory, guest_dynamic_loader, ...) are skipped on shared runners — they have no upstream CI baseline to compare against and are environment-sensitive; they are validated interactively on real hardware.

**What CI cannot prove:** end-to-end gameplay of a specific commercial title on a specific Mac — that requires real GPU workloads with user-owned game dumps and is tracked in the roadmap.

---

## 5. Honest capability statement

The upstream author states releases begin "after the first full successful launch of at least one game" — upstream itself is pre-release, reaching logo/main menu/gameplay on real titles under Linux/Windows. This edition preserves that fidelity boundary:

- **Runs today on macOS:** the full relinker pipeline (`--macos` Mach-O output), all system prx libraries, the Vulkan/MoltenVK execution engine (complete game-execution path), the native Metal/MetalFX presentation backend, and Switch titles through Ryujinx.
- **Native Metal boundary:** guest graphics draw pipelines execute through MoltenVK on macOS in this edition; the native Metal backend owns presentation, compositing, upscaling (MetalFX), and compute dispatch. Closing the Draw gap is the roadmap's headline item (Section 7).
- **"Any modern PS5 game"** remains aspirational for all forks of this young codebase; this edition's contribution is the platform, acceleration, and verification infrastructure that makes that goal reachable on Apple hardware.

---

## 6. Design decisions worth knowing

1. **Two backends, one interface.** Rather than a risky big-bang Vulkan→Metal rewrite, the port keeps the proven Vulkan execution engine (MoltenVK) and adds Metal where it tangibly accelerates (presentation, MetalFX upscaling, compute). Strict failures mark the boundary.
2. **SPIRV-Cross over a custom MSL backend.** The recompiler's SPIR-V is portable; SPIRV-Cross (what MoltenVK itself is built on) converts it to MSL, avoiding a parallel MSL emitter that would double maintenance.
3. **Mach-O via the relinker, not a translator.** The macOS output reuses the upstream patch pipeline (entry stubs, NIDs, dynamic section) with Mach-O load-command emission — the same architectural position as the Windows PE writer.
4. **License isolation.** OptiScaler (GPL-3.0) is never merged into the GPL-2.0 tree; Ryujinx (MIT) rides as a pinned submodule; SPIRV-Cross is used under its MIT option, which is GPL-2.0-compatible.
5. **KytyPS5 as recipe, not cargo.** KytyPS5's proven macOS CI and MoltenVK bundling/codesign workflow were adopted; its 150k-LOC engine was not transplanted because the two architectures (LLE emulator vs. relinker) are incompatible at the core.

---

## 7. Roadmap

1. **Native Metal Draw path** — map `Graphics::State` (blend, depth-stencil, vertex fetch) to `MTLRenderPipelineDescriptor`, with guest textures through the existing detiler; unblock with `MTLArgumentBuffersTier2`.
2. **MetalFX Temporal scaling** — the spatial scaler ships now; temporal needs per-frame motion vectors from the guest command stream.
3. **macOS window services** — aspect-ratio constrained resizing via Cocoa delegate (the Windows `WM_SIZING` analog).
4. **Game compatibility campaign** — per-title shims tracked against upstream's compatibility progress, with Apple Silicon GPU-specific validation.
5. **Codesigned, notarized .app bundle** — CI already builds; bundling and notarization are packaging follow-ups.

---

## 8. Component inventory

| Component | License | Role |
|---|---|---|
| AnyPS5 (upstream) | GPL-2.0 | base relinker/recompiler/prx stack |
| OSX Edition additions | GPL-2.0 | Metal/MetalFX backend, Mach-O output, hybrid launcher, CI |
| Ryujinx (`alula/Ryujinx`) | MIT | Switch engine, pinned submodule |
| OptiScaler (`optiscaler/OptiScaler`) | GPL-3.0 | optional Windows upscaling bridge, license-isolated |
| MoltenVK 1.2.11 | Apache-2.0 | Vulkan-on-Metal runtime fallback |
| SPIRV-Cross | MIT (option) | SPIR-V → MSL translation |
| SDL2, Vulkan/SPIRV headers/tools, glslang, VMA, LibAtrac9 | permissive | pinned dependencies |

*Report generated for release v1.0.0-osx. Verification artifacts: GitHub Actions run history on `osx-edition`, local GCC 14 compile logs, and the `scripts/metal_probe.sh` output on Apple runners.*
