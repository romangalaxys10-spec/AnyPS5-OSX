#!/usr/bin/env bash
# run-switch-engine.sh — launch the dotnet-published Ryujinx engine used by
# AnyPS5 OSX Edition as an external "switch engine" execution backend.
#
# Search order:
#   1. $ANYPS5_SWITCH_ENGINE          (explicit path to the engine executable)
#   2. switch/publish/<detected-os>/  (osx on macOS, linux on Linux)
#   3. switch/publish/osx/ then switch/publish/linux/ (cross-launch fallback)
#
# --------------------------------------------------------------- macOS -----
# CI publishes linux-x64 only (single ubuntu job keeps the CI matrix fast).
# The exact macOS publish command is documented here by design:
#
#     cd switch/Ryujinx
#     dotnet publish src/Ryujinx.Ava -c Release -r osx-x64 --self-contained false -o publish/osx
#     # fallback if this Ryujinx revision has no Avalonia frontend project:
#     dotnet publish src/Ryujinx -c Release -r osx-x64 --self-contained false -o publish/osx
#
# App Translocation: Ryujinx (an unsigned dotnet apphost) that arrives via
# Finder/browser download gets the com.apple.quarantine attribute; macOS then
# relocates the app to a read-only translocation path and its relative paths
# break. Ryujinx needs a translocation bypass:
#
#     xattr -cr publish/osx
#
# (the script below only warns when it detects the attribute — it never
# mutates it silently)
# ----------------------------------------------------------------------------

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
PUBLISH_ROOT="${REPO_ROOT}/switch/publish"

case "$(uname -s)" in
    Darwin) OS_DIR="osx" ;;
    Linux)  OS_DIR="linux" ;;
    *)      OS_DIR="unknown" ;;
esac

ENGINE="${ANYPS5_SWITCH_ENGINE:-}"
if [[ -z "${ENGINE}" ]]; then
    for dir in "${PUBLISH_ROOT}/${OS_DIR}" "${PUBLISH_ROOT}/osx" "${PUBLISH_ROOT}/linux"; do
        for name in Ryujinx Ryujinx.Ava Ryujinx.Headless.SDL2; do
            if [[ -x "${dir}/${name}" ]]; then
                ENGINE="${dir}/${name}"
                break 2
            fi
        done
    done
fi

if [[ -z "${ENGINE}" || ! -e "${ENGINE}" ]]; then
    cat >&2 <<EOF
error: switch engine (Ryujinx) not found.
Looked at:
  \$ANYPS5_SWITCH_ENGINE -> ${ANYPS5_SWITCH_ENGINE:-<unset>}
  ${PUBLISH_ROOT}/${OS_DIR}/Ryujinx{,.Ava,.Headless.SDL2}
  ${PUBLISH_ROOT}/osx/Ryujinx{,.Ava,.Headless.SDL2}
  ${PUBLISH_ROOT}/linux/Ryujinx{,.Ava,.Headless.SDL2}

Publish it first (Linux, same as CI):
  cd switch/Ryujinx && dotnet publish src/Ryujinx.Ava -c Release -r linux-x64 --self-contained false -o ../../publish/linux

Publish it on macOS (see the comment block at the top of this script):
  cd switch/Ryujinx && dotnet publish src/Ryujinx.Ava -c Release -r osx-x64 --self-contained false -o publish/osx
  xattr -cr publish/osx
EOF
    exit 1
fi

# macOS: warn about quarantine (App Translocation) without mutating attributes.
if [[ "$(uname -s)" == "Darwin" ]] && command -v xattr >/dev/null 2>&1; then
    if xattr -p com.apple.quarantine "${ENGINE}" >/dev/null 2>&1; then
        echo "warning: '${ENGINE}' is quarantined (com.apple.quarantine attribute present)." >&2
        echo "         macOS may relocate it via App Translocation and break relative paths." >&2
        echo "         Fix with: xattr -cr '$(dirname "${ENGINE}")'" >&2
    fi
fi

echo ">> Launching switch engine: ${ENGINE} $*"
exec "${ENGINE}" "$@"
