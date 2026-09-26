#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DRAWQUEUE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DRAWQUEUE_HPP

#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"

namespace AgcDriver::Graphics {

class DrawQueue {
public:
    ~DrawQueue();
    VkCommandBuffer Begin(const Context& context);
    void Enqueue(std::shared_ptr<ShaderResources> resources, std::shared_ptr<void> storage);
    void Flush();
    void Resolve(std::uint64_t address, std::size_t bytes);
    void Wait();
    void WaitGpu();
    void Collect();
    void RecordMemoryBarrier(const Context& context);

private:
    struct Entry {
        std::shared_ptr<void> storage;
        std::shared_ptr<ShaderResources> resources;
    };
    struct Batch {
        std::vector<Entry> entries;
        std::unique_ptr<CommandBatch> commands;
        bool hasBarrier = false;
    };
    void retire(Batch batch);
    Batch recording;
    std::vector<Batch> pending;
    std::vector<std::unique_ptr<CommandBatch>> available;
    std::size_t drawCount = 0;
};

}

#endif
