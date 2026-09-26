#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_RENDERCACHE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_RENDERCACHE_HPP

#include "prx/libSceAgcDriver/Graphics/include/GpuColorTransfer.hpp"
#include "prx/libc/include/GuestMemoryTracking.hpp"
#include <map>

namespace AgcDriver::Graphics {

class ResidentColor {
public:
    ResidentColor(const Context& context, const ColorTarget& color);
    ~ResidentColor();
    void Begin(VkCommandBuffer commands);
    void Download(VkCommandBuffer commands);
    void Commit();
    void Transition(VkCommandBuffer commands, VkImageLayout layout);
    RenderTarget& Target() { return transfer.Target(color, false); }
    const ColorTarget& Description() const { return color; }
    bool Valid() const { return valid; }
    bool Dirty() const { return dirty; }
    std::uint64_t Generation() const { return generation; }
    void Invalidate();
    void ReleaseMemory();
    bool SharesPages(const ColorTarget& other) const;

private:
    void resolveCpuAccess(GuestMemoryTracking::Access access);
    Context context;
    ColorTarget color;
    GpuColorTransfer transfer;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    bool valid = false;
    bool dirty = false;
    std::uint64_t generation = 0;
    std::unique_ptr<GuestMemoryTracking::Watch> memoryWatch;
};

class RenderCache {
public:
    explicit RenderCache(const Context& context) : context(context) {}
    ~RenderCache();
    std::shared_ptr<ResidentColor> Get(const ColorTarget& color, bool blending);
    std::shared_ptr<ResidentColor> Find(std::uint64_t address) const;
    void Resolve(std::uint64_t address, std::size_t bytes, bool writable);
    void Flush();

private:
    Context context;
    std::map<std::uint64_t, std::shared_ptr<ResidentColor>> entries;
};

}

#endif
