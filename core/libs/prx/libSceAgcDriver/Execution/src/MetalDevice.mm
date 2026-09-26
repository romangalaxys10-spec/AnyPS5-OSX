#include "prx/libSceAgcDriver/Execution/include/MetalDevice.hpp"

#if defined(__APPLE__)

#import <Metal/Metal.h>
#import <MetalFX/MetalFX.h>
#import <MetalFX/MTLFXSpatialScaler.h>
#import <QuartzCore/QuartzCore.h>
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

#import <SDL.h>
#import <SDL_syswm.h>

#include <spirv/unified1/spirv.hpp>
#include <spirv_cross.hpp>
#include <spirv_msl.hpp>

#include "prx/libSceAgcDriver/Execution/include/AspectFit.hpp"
#include "prx/libSceAgcDriver/Execution/include/Presentation.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "Recompiler.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace AgcDriver {
namespace {

constexpr std::uint64_t kPixelFormatBgra8 = 0x8000000000000000ull;
constexpr std::uint64_t kPixelFormatRgba8 = 0x8000000022000000ull;

void require(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error("MetalDevice: " + reason);
}

std::string NSStringToStdString(NSString* value) {
    return value != nil ? std::string(value.UTF8String ?: "") : std::string();
}

}

struct MetalDevice::State {
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> queue = nil;
    CAMetalLayer* layer = nil;
    void* windowContext = nullptr;
    std::uint32_t windowWidth = 0;
    std::uint32_t windowHeight = 0;
    std::vector<id<MTLCommandBuffer>> pending;
    struct GuestRange {
        std::uint64_t address;
        std::size_t bytes;
        id<MTLBuffer> buffer;
    };
    std::map<std::uint64_t, GuestRange> ranges;
    id<MTLTexture> pixelTexture = nil;
    std::uint32_t pixelTextureWidth = 0;
    std::uint32_t pixelTextureHeight = 0;
    id<MTLRenderPipelineState> blitPipeline = nil;
    id<MTLLibrary> blitLibrary = nil;
    struct ScalerEntry {
        std::uint32_t sourceWidth;
        std::uint32_t sourceHeight;
        std::uint32_t targetWidth;
        std::uint32_t targetHeight;
        id<MTLFXSpatialScaler> scaler;
        id<MTLTexture> sourceTexture;
    };
    std::vector<ScalerEntry> scalers;
    bool metalFxAvailable = false;

    ~State() {
        [pixelTexture release];
        [blitPipeline release];
        [blitLibrary release];
        for (auto& entry : scalers) {
            [entry.scaler release];
            [entry.sourceTexture release];
        }
        for (auto& [address, range] : ranges) {
            [range.buffer release];
        }
        for (id<MTLCommandBuffer> command : pending) {
            [command release];
        }
        [queue release];
        [device release];
    }
};

namespace {

MTLPixelFormat DisplayPixelFormat(std::uint64_t pixelFormat) {
    switch (pixelFormat) {
    case kPixelFormatBgra8:
        return MTLPixelFormatBGRA8Unorm;
    case kPixelFormatRgba8:
        return MTLPixelFormatRGBA8Unorm;
    default:
        throw std::runtime_error("MetalDevice: unsupported display pixel format " + std::to_string(pixelFormat));
    }
}

const char* BlitShaderSource() {
    return R"(
#include <metal_stdlib>
using namespace metal;

struct BlitVertexOut {
    float4 position [[position]];
    float2 uv;
};

struct BlitUniforms {
    uint opaque;
    float pad0;
    float pad1;
    float pad2;
};

vertex BlitVertexOut AgcBlitVertex(uint vertexId [[vertex_id]]) {
    const float2 corners[4] = {float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0), float2(-1.0, -1.0)};
    BlitVertexOut out;
    const float2 position = corners[vertexId % 4];
    out.position = float4(position, 0.0, 1.0);
    out.uv = float2((position.x + 1.0) * 0.5, 1.0 - (position.y + 1.0) * 0.5);
    return out;
}

fragment float4 AgcBlitFragment(BlitVertexOut in [[stage_in]],
                                texture2d<float> source [[texture(0)]],
                                sampler sourceSampler [[sampler(0)]],
                                constant BlitUniforms& uniforms [[buffer(0)]]) {
    const float4 color = source.sample(sourceSampler, in.uv);
    if (uniforms.opaque != 0u && color.a < 0.5) {
        discard_fragment();
    }
    return color;
}
)";
}

id<MTLRenderPipelineState> GetBlitPipeline(MetalDevice::State& state) {
    @autoreleasepool {
        if (state.blitPipeline != nil) return state.blitPipeline;
        require(state.device != nil, "device must exist before pipeline creation");
        if (state.blitLibrary == nil) {
            NSError* error = nil;
            MTLCompileOptions* options = [MTLCompileOptions new];
            options.languageVersion = MTLLanguageVersion2_3;
            state.blitLibrary = [state.device newLibraryWithSource:[NSString stringWithUTF8String:BlitShaderSource()] options:options error:&error];
            [options release];
            require(state.blitLibrary != nil, "blit shader compile failed: " + NSStringToStdString(error.localizedDescription));
        }
        id<MTLFunction> vertex = [state.blitLibrary newFunctionWithName:@"AgcBlitVertex"];
        id<MTLFunction> fragment = [state.blitLibrary newFunctionWithName:@"AgcBlitFragment"];
        require(vertex != nil && fragment != nil, "blit shader entry points missing");
        MTLRenderPipelineDescriptor* descriptor = [MTLRenderPipelineDescriptor new];
        descriptor.vertexFunction = vertex;
        descriptor.fragmentFunction = fragment;
        descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        descriptor.colorAttachments[0].blendingEnabled = NO;
        NSError* error = nil;
        state.blitPipeline = [state.device newRenderPipelineStateWithDescriptor:descriptor error:&error];
        [descriptor release];
        [vertex release];
        [fragment release];
        require(state.blitPipeline != nil, "blit pipeline state failed: " + NSStringToStdString(error.localizedDescription));
        return state.blitPipeline;
    }
}

id<MTLTexture> EnsurePixelTexture(MetalDevice::State& state, std::uint32_t width, std::uint32_t height) {
    @autoreleasepool {
        if (state.pixelTexture != nil && state.pixelTextureWidth == width && state.pixelTextureHeight == height) {
            return state.pixelTexture;
        }
        [state.pixelTexture release];
        MTLTextureDescriptor* descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:width height:height mipmapped:NO];
        descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
        descriptor.storageMode = MTLStorageModeShared;
        state.pixelTexture = [state.device newTextureWithDescriptor:descriptor];
        state.pixelTextureWidth = width;
        state.pixelTextureHeight = height;
        require(state.pixelTexture != nil, "pixel texture allocation failed");
        return state.pixelTexture;
    }
}

id<MTLTexture> EnsureScalerSourceTexture(MetalDevice::State& state, std::uint32_t width, std::uint32_t height, MTLPixelFormat format) {
    @autoreleasepool {
        for (auto& entry : state.scalers) {
            if (entry.sourceTexture != nil && entry.sourceWidth == width && entry.sourceHeight == height) {
                return entry.sourceTexture;
            }
        }
        MTLTextureDescriptor* descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:width height:height mipmapped:NO];
        descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
        descriptor.storageMode = MTLStorageModeShared;
        id<MTLTexture> texture = [state.device newTextureWithDescriptor:descriptor];
        require(texture != nil, "scaler source texture allocation failed");
        MetalDevice::State::ScalerEntry entry{};
        entry.sourceWidth = width;
        entry.sourceHeight = height;
        entry.targetWidth = 0;
        entry.targetHeight = 0;
        entry.scaler = nil;
        entry.sourceTexture = texture;
        state.scalers.push_back(entry);
        return texture;
    }
}

}

MetalDevice::MetalDevice(const PresentationWindow* window) : state(std::make_unique<State>()) {
    @autoreleasepool {
        state->device = MTLCreateSystemDefaultDevice();
        require(state->device != nil, "no Metal device available; set ANYPS5_GPU_BACKEND=vulkan");
        state->queue = [state->device newCommandQueue];
        require(state->queue != nil, "command queue allocation failed");
#if defined(__MAC_14_0)
        state->metalFxAvailable = @available(macOS 14.0, *);
#else
        state->metalFxAvailable = false;
#endif
        if (window != nullptr) {
            require(window->context != nullptr && window->width != 0 && window->height != 0, "invalid window descriptor");
            state->windowContext = window->context;
            state->windowWidth = window->width;
            state->windowHeight = window->height;
            SDL_SysWMinfo info;
            SDL_VERSION(&info.version);
            require(SDL_GetWindowWMInfo(static_cast<SDL_Window*>(window->context), &info) == SDL_TRUE, std::string("SDL_GetWindowWMInfo failed: ") + SDL_GetError());
            require(info.subsystem == SDL_SYSWM_COCOA, "MetalDevice requires the SDL Cocoa window subsystem");
            NSWindow* nativeWindow = info.info.cocoa.window;
            require(nativeWindow != nil, "SDL window has no NSWindow");
            NSView* contentView = nativeWindow.contentView;
            require(contentView != nil, "window has no content view");
            if (![contentView.layer isKindOfClass:[CAMetalLayer class]]) {
                CAMetalLayer* metalLayer = [CAMetalLayer layer];
                metalLayer.framebufferOnly = NO;
                metalLayer.contentsScale = nativeWindow.backingScaleFactor;
                contentView.layer = metalLayer;
                contentView.wantsLayer = YES;
            }
            CAMetalLayer* metalLayer = (CAMetalLayer*)contentView.layer;
            metalLayer.device = state->device;
            metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
            metalLayer.maximumDrawableCount = 3;
            CGSize backing = [contentView convertRectToBacking:contentView.bounds].size;
            const std::uint32_t backingWidth = backing.width > 1.0 ? static_cast<std::uint32_t>(backing.width) : window->width;
            const std::uint32_t backingHeight = backing.height > 1.0 ? static_cast<std::uint32_t>(backing.height) : window->height;
            metalLayer.drawableSize = CGSizeMake(backingWidth, backingHeight);
            state->layer = metalLayer;
        }
    }
}

MetalDevice::~MetalDevice() {
    if (state != nullptr) {
        try {
            WaitIdle();
        } catch (...) {
        }
        state.reset();
    }
}

bool MetalDeviceAvailable() {
    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        const bool available = device != nil;
        [device release];
        return available;
    }
}

ShaderRecompiler::SpirvTarget MetalDevice::Target() const {
    static const std::array<std::uint32_t, 1> capabilities{};
    static const std::array<std::string_view, 1> extensions{""};
    ShaderRecompiler::SpirvTarget target{VK_API_VERSION_1_1, 0x00010500u, 32u, 0u, capabilities, extensions, false, {1024u, 1024u, 1024u}, 1024u, 32768u, std::nullopt, std::nullopt};
    return target;
}

void MetalDevice::WaitIdle() {
    @autoreleasepool {
        for (id<MTLCommandBuffer> command : state->pending) {
            [command waitUntilCompleted];
        }
        state->pending.clear();
    }
}

void MetalDevice::WaitDraws() {
    WaitIdle();
}

void MetalDevice::AcquireGpuMemory() {
    @autoreleasepool {
        for (auto& [address, range] : state->ranges) {
            if (range.buffer == nil) {
                range.buffer = [state->device newBufferWithLength:range.bytes options:MTLResourceStorageModeShared];
                require(range.buffer != nil, "guest buffer allocation failed for address " + std::to_string(address));
            }
        }
    }
}

void MetalDevice::ResolveMemory(std::uint64_t address, std::size_t bytes, bool writable) {
    require(bytes != 0, "ResolveMemory requires a non-zero extent");
    auto existing = state->ranges.lower_bound(address);
    if (existing != state->ranges.end() && existing->first <= address && existing->first + existing->second.bytes >= address + bytes) {
        return;
    }
    if (existing != state->ranges.begin()) {
        auto previous = std::prev(existing);
        if (previous->first + previous->second.bytes > address) {
            const std::uint64_t mergedEnd = std::max(address + bytes, previous->first + previous->second.bytes);
            const std::size_t mergedBytes = static_cast<std::size_t>(mergedEnd - previous->first);
            previous->second.bytes = mergedBytes;
            [previous->second.buffer release];
            previous->second.buffer = nil;
            return;
        }
    }
    State::GuestRange range{};
    range.address = address;
    range.bytes = bytes;
    range.buffer = nil;
    state->ranges.emplace(address, range);
    static_cast<void>(writable);
}

void* MetalDevice::Window() const {
    return state->windowContext;
}

void MetalDevice::Resize(std::uint32_t width, std::uint32_t height) {
    require(width != 0 && height != 0, "Resize requires non-zero extents");
    state->windowWidth = width;
    state->windowHeight = height;
    if (state->layer != nil) {
        state->layer.drawableSize = CGSizeMake(width, height);
    }
}

bool MetalDevice::Presentable() const {
    return state->layer != nil && state->queue != nil && state->device != nil;
}

void MetalDevice::PresentClear(std::uint32_t width, std::uint32_t height, bool opaque) {
    require(width != 0 && height != 0, "PresentClear requires non-zero extents");
    present(width, height, opaque, {}, nullptr);
}

void MetalDevice::PresentPixels(std::uint32_t width, std::uint32_t height, std::span<const std::byte> pixels) {
    require(width != 0 && height != 0, "PresentPixels requires non-zero extents");
    require(pixels.size() >= static_cast<std::size_t>(width) * height * 4, "PresentPixels payload is smaller than the source extent");
    present(width, height, false, pixels, nullptr);
}

void MetalDevice::PresentDisplayBuffer(const DisplayBuffer& buffer) {
    require(buffer.width != 0 && buffer.height != 0, "PresentDisplayBuffer requires non-zero extents");
    require(buffer.address != 0, "PresentDisplayBuffer requires a guest address");
    const auto entry = state->ranges.find(buffer.address);
    require(entry != state->ranges.end(), "PresentDisplayBuffer guest address was not resolved");
    require(entry->second.buffer != nil, "PresentDisplayBuffer guest buffer was not acquired");
    present(buffer.width, buffer.height, false, {}, &buffer);
}

void MetalDevice::present(std::uint32_t width, std::uint32_t height, bool opaque, std::span<const std::byte> pixels, const DisplayBuffer* display) {
    @autoreleasepool {
        require(state->layer != nil, "present requires a Metal layer");
        id<CAMetalDrawable> drawable = [state->layer nextDrawable];
        if (drawable == nil) return;
        id<MTLCommandBuffer> command = [state->queue commandBuffer];
        require(command != nil, "command buffer allocation failed");
        const CGSize drawableSize = state->layer.drawableSize;
        const std::uint32_t targetWidth = drawableSize.width > 1.0 ? static_cast<std::uint32_t>(drawableSize.width) : width;
        const std::uint32_t targetHeight = drawableSize.height > 1.0 ? static_cast<std::uint32_t>(drawableSize.height) : height;
        if (display != nullptr && state->metalFxAvailable && (targetWidth > display->width || targetHeight > display->height)) {
            const MTLPixelFormat format = DisplayPixelFormat(display->pixelFormat);
            id<MTLTexture> source = EnsureScalerSourceTexture(*state, display->width, display->height, format);
            State::ScalerEntry* entry = nullptr;
            for (auto& item : state->scalers) {
                if (item.sourceWidth == display->width && item.sourceHeight == display->height) {
                    entry = &item;
                    break;
                }
            }
            require(entry != nullptr, "scaler source texture entry missing");
            const auto guestRange = state->ranges.find(display->address);
            require(guestRange != state->ranges.end() && guestRange->second.buffer != nil, "MetalFX present guest buffer was not acquired");
            const std::size_t sourceBytes = static_cast<std::size_t>(display->width) * display->height * 4;
            std::memcpy([source contents], [guestRange->second.buffer contents], sourceBytes);
            if (entry->scaler == nil || entry->targetWidth != targetWidth || entry->targetHeight != targetHeight) {
                [entry->scaler release];
                MTLFXSpatialScalerDescriptor* descriptor = [MTLFXSpatialScalerDescriptor new];
                descriptor.inputWidth = display->width;
                descriptor.inputHeight = display->height;
                descriptor.outputWidth = targetWidth;
                descriptor.outputHeight = targetHeight;
                descriptor.colorTextureFormat = source.pixelFormat;
                descriptor.outputTextureFormat = state->layer.pixelFormat;
                descriptor.colorProcessingMode = MTLFXSpatialScalerColorProcessingModeLinear;
                NSError* error = nil;
                entry->scaler = [descriptor newSpatialScalerWithDevice:state->device error:&error];
                [descriptor release];
                require(entry->scaler != nil, "MetalFX spatial scaler creation failed: " + NSStringToStdString(error.localizedDescription));
                entry->targetWidth = targetWidth;
                entry->targetHeight = targetHeight;
            }
            entry->scaler.colorTexture = source;
            entry->scaler.outputTexture = drawable.texture;
            [entry->scaler encodeToCommandBuffer:command];
            [command presentDrawable:drawable];
            [command commit];
            [command retain];
            state->pending.push_back(command);
            return;
        }
        id<MTLTexture> sourceTexture = nil;
        if (display != nullptr) {
            sourceTexture = EnsurePixelTexture(*state, display->width, display->height);
            const auto guestRange = state->ranges.find(display->address);
            require(guestRange != state->ranges.end() && guestRange->second.buffer != nil, "present guest buffer was not acquired");
            const std::size_t sourceBytes = static_cast<std::size_t>(display->width) * display->height * 4;
            std::memcpy([sourceTexture contents], [guestRange->second.buffer contents], sourceBytes);
        } else if (!pixels.empty()) {
            sourceTexture = EnsurePixelTexture(*state, width, height);
            MTLRegion region = MTLRegionMake2D(0, 0, width, height);
            [sourceTexture replaceRegion:region mipmapLevel:0 withBytes:pixels.data() bytesPerRow:width * 4];
        }
        if (sourceTexture == nil) {
            MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
            pass.colorAttachments[0].texture = drawable.texture;
            pass.colorAttachments[0].loadAction = MTLLoadActionClear;
            pass.colorAttachments[0].storeAction = MTLStoreActionStore;
            pass.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, opaque ? 1.0 : 0.0);
            id<MTLRenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
            [encoder endEncoding];
            [command presentDrawable:drawable];
            [command commit];
            [command retain];
            state->pending.push_back(command);
            return;
        }
        id<MTLRenderCommandEncoder> encoder = nil;
        @autoreleasepool {
            MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
            pass.colorAttachments[0].texture = drawable.texture;
            pass.colorAttachments[0].loadAction = MTLLoadActionClear;
            pass.colorAttachments[0].storeAction = MTLStoreActionStore;
            pass.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
            encoder = [command renderCommandEncoderWithDescriptor:pass];
            id<MTLRenderPipelineState> pipeline = GetBlitPipeline(*state);
            [encoder setRenderPipelineState:pipeline];
            [encoder setFragmentTexture:sourceTexture atIndex:0];
            const auto contain = ComputeContainRect_nid_postfix(sourceTexture.width, sourceTexture.height, targetWidth, targetHeight);
            [encoder setViewport:MTLViewport{static_cast<double>(contain.x), static_cast<double>(contain.y), static_cast<double>(contain.width), static_cast<double>(contain.height), 0.0, 1.0}];
            struct BlitUniforms {
                std::uint32_t opaque;
                float pad0;
                float pad1;
                float pad2;
            } uniforms{opaque ? 1u : 0u, 0.0f, 0.0f, 0.0f};
            [encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:0];
            [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
            [encoder endEncoding];
        }
        [command presentDrawable:drawable];
        [command commit];
        [command retain];
        state->pending.push_back(command);
    }
}

void MetalDevice::Dispatch(const ShaderRecompiler::RecompileResult& shader, std::uint32_t x, std::uint32_t y, std::uint32_t z, std::span<const Graphics::GuestMemorySnapshot> snapshots) {
    @autoreleasepool {
        require(!shader.spirv.empty(), "Dispatch requires compiled SPIR-V");
        std::vector<std::uint32_t> words(shader.spirv.begin(), shader.spirv.end());
        spirv_cross::CompilerMSL compiler(std::move(words));
        compiler.set_msl_version(2, 3, 0);
        std::string msl;
        try {
            msl = compiler.compile();
        } catch (const spirv_cross::CompilerError& error) {
            throw std::runtime_error(std::string("MetalDevice: SPIR-V to MSL translation failed: ") + error.what());
        }
        NSError* compileError = nil;
        MTLCompileOptions* options = [MTLCompileOptions new];
        options.languageVersion = MTLLanguageVersion3_0;
        id<MTLLibrary> library = [state->device newLibraryWithSource:[NSString stringWithUTF8String:msl.c_str()] options:options error:&compileError];
        [options release];
        require(library != nil, "MSL library compile failed: " + NSStringToStdString(compileError.localizedDescription));
        id<MTLFunction> function = [library newFunctionWithName:@"main0"];
        require(function != nil, "MSL entry point main0 missing");
        NSError* pipelineError = nil;
        id<MTLComputePipelineState> pipeline = [state->device newComputePipelineStateWithFunction:function error:&pipelineError];
        require(pipeline != nil, "Metal compute pipeline creation failed: " + NSStringToStdString(pipelineError.localizedDescription));
        id<MTLCommandBuffer> command = [state->queue commandBuffer];
        require(command != nil, "command buffer allocation failed");
        id<MTLComputeCommandEncoder> encoder = [command computeCommandEncoder];
        [encoder setComputePipelineState:pipeline];
        AcquireGpuMemory();
        std::size_t bound = 0;
        for (const auto& snapshot : snapshots) {
            if (bound >= 8) break;
            if (snapshot.bytes.empty()) continue;
            id<MTLBuffer> staging = [state->device newBufferWithBytes:snapshot.bytes.data() length:snapshot.bytes.size() options:MTLResourceStorageModeShared];
            [encoder setBuffer:staging offset:0 atIndex:bound];
            [staging release];
            ++bound;
        }
        const spirv_cross::ShaderResources resources = compiler.get_shader_resources();
        for (const auto& resource : resources.stage_inputs) {
            throw std::runtime_error("MetalDevice: stage input resources require descriptor ABI parity with the Vulkan backend; use ANYPS5_GPU_BACKEND=vulkan");
        }
        for (const auto& resource : resources.sampled_images) {
            static_cast<void>(resource);
            throw std::runtime_error("MetalDevice: sampled images require descriptor ABI parity with the Vulkan backend; use ANYPS5_GPU_BACKEND=vulkan");
        }
        std::uint32_t slot = bound;
        for (const auto& resource : resources.storage_buffers) {
            if (slot >= 16) break;
            const std::uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
            const auto entry = state->ranges.begin();
            if (entry != state->ranges.end() && entry->second.buffer != nil) {
                [encoder setBuffer:entry->second.buffer offset:0 atIndex:slot];
            }
            ++slot;
        }
        MTLSize groups = MTLSizeMake(x, y, z);
        std::uint32_t localX = compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 0);
        std::uint32_t localY = compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 1);
        std::uint32_t localZ = compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 2);
        if (localX == 0) localX = 32;
        if (localY == 0) localY = 1;
        if (localZ == 0) localZ = 1;
        MTLSize threads = MTLSizeMake(localX, localY, localZ);
        [encoder dispatchThreadgroups:groups threadsPerThreadgroup:threads];
        [encoder endEncoding];
        [command commit];
        [command retain];
        state->pending.push_back(command);
        [library release];
        [function release];
        [pipeline release];
    }
}

void MetalDevice::Draw(const Graphics::State& graphics, const Pm4::DrawParameters& draw, std::span<const Graphics::CompiledShader> shaders, std::span<const Graphics::GuestMemorySnapshot> snapshots) {
    static_cast<void>(graphics);
    static_cast<void>(draw);
    static_cast<void>(shaders);
    static_cast<void>(snapshots);
    throw std::runtime_error("MetalDevice: native Metal graphics pipeline is not implemented in this edition; use ANYPS5_GPU_BACKEND=vulkan (MoltenVK) for full game execution");
}

void MetalDevice::EnqueueDraw(const Graphics::State& graphics, const Pm4::DrawParameters& draw, std::span<const Graphics::CompiledShader> shaders, std::span<const Graphics::GuestMemorySnapshot> snapshots) {
    Draw(graphics, draw, shaders, snapshots);
}

}

#endif
