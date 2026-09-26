#!/usr/bin/env bash
# fetch-moltenvk.sh — bundle a pinned MoltenVK runtime next to a target binary.
#
# Proven recipe from the KytyPS5 macOS CI: download the pinned MoltenVK release
# tarball, extract libMoltenVK.dylib, copy it next to the launcher binary, and
# ad-hoc codesign it so dyld accepts the (re-distributed) dylib.
#
# Usage:
#   scripts/fetch-moltenvk.sh <target-binary-dir>
#     e.g. scripts/fetch-moltenvk.sh build
#
# Env:
#   MOLTENVK_VERSION  pinned release tag (default: 1.2.11)

set -euo pipefail

MOLTENVK_VERSION="${MOLTENVK_VERSION:-1.2.11}"
TARGET_DIR="${1:?usage: scripts/fetch-moltenvk.sh <target-binary-dir>}"
URL="https://github.com/KhronosGroup/MoltenVK/releases/download/v${MOLTENVK_VERSION}/MoltenVK-macos.tar"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "error: fetch-moltenvk.sh targets macOS (codesign/dyld required)" >&2
    exit 1
fi
command -v codesign >/dev/null 2>&1 || { echo "error: codesign not found in PATH" >&2; exit 1; }

WORK_DIR="$(mktemp -d -t anyps5_moltenvk.XXXXXX)"
trap 'rm -rf "${WORK_DIR}"' EXIT

echo ">> Downloading MoltenVK v${MOLTENVK_VERSION}"
curl -fSL --retry 3 --retry-delay 2 -o "${WORK_DIR}/MoltenVK-macos.tar" "${URL}"

echo ">> Extracting"
tar -xf "${WORK_DIR}/MoltenVK-macos.tar" -C "${WORK_DIR}"

# The release tarball layout has drifted between versions
# (MoltenVK/dylib/macOS/...), so locate the dylib instead of assuming a path.
DYLIB="$(find "${WORK_DIR}" -type f -name 'libMoltenVK.dylib' | head -n 1)"
if [[ -z "${DYLIB}" ]]; then
    echo "error: libMoltenVK.dylib not found in MoltenVK-macos.tar (v${MOLTENVK_VERSION})" >&2
    exit 1
fi

mkdir -p "${TARGET_DIR}"
echo ">> Installing $(basename "${DYLIB}") -> ${TARGET_DIR}/"
cp -f "${DYLIB}" "${TARGET_DIR}/libMoltenVK.dylib"

echo ">> Ad-hoc codesigning ${TARGET_DIR}/libMoltenVK.dylib"
codesign --force --sign - "${TARGET_DIR}/libMoltenVK.dylib"

echo ">> Done: ${TARGET_DIR}/libMoltenVK.dylib (MoltenVK v${MOLTENVK_VERSION})"
