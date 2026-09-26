#!/usr/bin/env bash
# build-macos.sh — local developer build for AnyPS5 OSX Edition.
#
# Mirrors the GitHub Actions `macos` job in .github/workflows/osx-edition.yml:
#   1. configure (Ninja, Release, AUTO GPU backend, MoltenVK rpath, tests on)
#   2. build relinker / nid_patcher / libs / anyps5-launcher
#   3. build remaining CTest binaries
#   4. ctest
#   5. Metal/MetalFX probe (arm64 hosts only, report-only)
# Ends by printing artifact locations.
#
# Env overrides:
#   ARCH      target architecture: x86_64 (default) or arm64
#   BUILD_DIR build directory (default: build)
#   JOBS      parallel jobs for cmake --build (default: 3, same as CI)

set -euo pipefail

ARCH="${ARCH:-x86_64}"
BUILD_DIR="${BUILD_DIR:-build}"
JOBS="${JOBS:-3}"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "error: scripts/build-macos.sh must run on macOS" >&2
    exit 1
fi

# --- Homebrew prefix detection (rpath for the brew-installed MoltenVK loader) --
if command -v brew >/dev/null 2>&1; then
    BREW_PREFIX="$(brew --prefix)"
else
    BREW_PREFIX="/opt/homebrew"
    echo "warning: Homebrew not found; assuming prefix ${BREW_PREFIX}" >&2
fi
echo ">> Homebrew prefix: ${BREW_PREFIX}"

# --- dependencies --------------------------------------------------------------
if ! command -v cmake >/dev/null 2>&1 || ! command -v ninja >/dev/null 2>&1; then
    echo ">> installing cmake + ninja via brew"
    brew install cmake ninja
fi
if [[ ! -f "${BREW_PREFIX}/lib/libMoltenVK.dylib" ]]; then
    echo ">> installing molten-vk via brew (Vulkan loader for compile-time)"
    brew install molten-vk
fi

# --- configure (same flags as the CI macos job) --------------------------------
cmake -S . -B "${BUILD_DIR}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="${ARCH}" \
    -DANYPS5_GPU_BACKEND=AUTO \
    -DBUILD_TESTING=ON \
    -DCMAKE_EXE_LINKER_FLAGS="-Wl,-rpath,${BREW_PREFIX}/lib"

# --- build (same target list as CI) --------------------------------------------
cmake --build "${BUILD_DIR}" --target relinker nid_patcher libs anyps5-launcher --parallel "${JOBS}"

# --- remaining CTest binaries ----------------------------------------------------
cmake --build "${BUILD_DIR}" --parallel "${JOBS}"

# --- test --------------------------------------------------------------------------
ctest --test-dir "${BUILD_DIR}" --output-on-failure

# --- Metal probe (arm64 hosts only, report-only) ------------------------------------
if [[ "$(uname -m)" == "arm64" ]]; then
    bash scripts/metal_probe.sh
fi

echo
echo ">> Build complete. Artifacts:"
echo "   ${BUILD_DIR}/relinker           (PS5 ELF/SELF relinker)"
echo "   ${BUILD_DIR}/nid_patcher        (NID patcher CLI)"
echo "   ${BUILD_DIR}/libs/*.prx         (patched system prx libraries)"
echo "   ${BUILD_DIR}/anyps5-launcher    (OSX Edition launcher)"
echo
echo "Optional runtime bundle for the Vulkan fallback backend:"
echo "   scripts/fetch-moltenvk.sh ${BUILD_DIR}   # pinned MoltenVK dylib + ad-hoc codesign"
