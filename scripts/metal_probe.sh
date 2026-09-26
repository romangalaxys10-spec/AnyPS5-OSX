#!/usr/bin/env bash
# metal_probe.sh — Metal / MetalFX availability probe for AnyPS5 OSX Edition.
#
# Report-only: this script NEVER fails a build. It compiles a tiny ObjC++
# probe with `xcrun clang++` against the Metal and MetalFX frameworks and
# prints machine-readable markers:
#
#   METAL_DEVICE=ok|none                    MTLCreateSystemDefaultDevice result
#   METALFX_SPATIAL=available|unavailable   MTLFXSpatialScalerDescriptor resolvable
#
# plus informational lines (device name, Apple7/Metal3 family support, MetalFX
# weak-link signal count, Temporal scaler scaffold check). Always exits 0 —
# even if compilation fails (reported as METAL_DEVICE=none / METALFX_SPATIAL=
# unavailable with a reason line).
#
# CI usage (macos job in .github/workflows/osx-edition.yml), gated to arm64:
#   if [ "$(uname -m)" = "arm64" ]; then bash scripts/metal_probe.sh; fi

set -uo pipefail   # deliberately NO -e: the probe must never hard-fail

echo ">> AnyPS5 OSX Edition — Metal/MetalFX probe ($(uname -s) $(uname -m)$(sw_vers -productVersion >/dev/null 2>&1 && echo " $(sw_vers -productVersion)"))"

PROBE_SRC="$(mktemp -t anyps5_metal_probe.XXXXXX)"
PROBE_BIN="$(mktemp -t anyps5_metal_probe_bin.XXXXXX)"
trap 'rm -f "${PROBE_SRC}" "${PROBE_BIN}" "${PROBE_BIN}.dSYM" 2>/dev/null || true' EXIT

cat > "${PROBE_SRC}" <<'EOF'
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <MetalFX/MetalFX.h>
#include <cstdio>

int main() {
    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (device == nil) {
            std::printf("METAL_DEVICE=none\n");
            std::printf("METALFX_SPATIAL=unavailable\n");
            return 0;
        }
        std::printf("METAL_DEVICE=ok\n");
        NSString* deviceName = device.name;
        std::printf("METAL_DEVICE_NAME=%s\n",
                    deviceName ? [deviceName UTF8String] : "(unknown)");
        std::printf("METAL_FAMILY_APPLE7=%s\n",
                    [device supportsFamily:MTLGPUFamilyApple7] ? "yes" : "no");
        std::printf("METAL_FAMILY_METAL3=%s\n",
                    [device supportsFamily:MTLGPUFamilyMetal3] ? "yes" : "no");

        // MetalFX is weak-linked (-weak_framework MetalFX), so count how many
        // independent runtime signals resolve: >=1 means the framework is
        // present at runtime; 2 means the spatial scaler descriptor class is
        // fully usable by the port's PresentationScaler/MetalFX path.
        int signals = 0;
        BOOL weakLinkedClass = ([MTLFXSpatialScalerDescriptor class] != Nil);
        BOOL nsClassLookup   = (NSClassFromString(@"MTLFXSpatialScalerDescriptor") != nil);
        if (weakLinkedClass) signals++;
        if (nsClassLookup)   signals++;
        std::printf("METALFX_WEAK_LINK_SIGNALS=%d\n", signals);

        BOOL spatial = (weakLinkedClass && nsClassLookup);
        std::printf("METALFX_SPATIAL=%s\n", spatial ? "available" : "unavailable");

        // Temporal scaler scaffold check (informational for the port roadmap).
        BOOL temporal = (NSClassFromString(@"MTLFXTemporalScalerDescriptor") != nil);
        std::printf("METALFX_TEMPORAL=%s\n", temporal ? "available" : "unavailable");
    }
    return 0;
}
EOF

if ! xcrun clang++ -x objective-c++ -std=c++20 -fobjc-arc \
        -framework Foundation -framework Metal \
        -weak_framework MetalFX \
        "${PROBE_SRC}" -o "${PROBE_BIN}" 2>&1; then
    echo "METAL_PROBE=compile-failed (Metal/MetalFX headers or SDK unavailable)"
    echo "METAL_DEVICE=none"
    echo "METALFX_SPATIAL=unavailable"
    exit 0
fi

"${PROBE_BIN}" || true   # report-only: never propagate the probe exit status
exit 0
