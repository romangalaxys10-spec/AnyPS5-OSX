#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PIPELINECACHE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PIPELINECACHE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"

namespace AgcDriver::Graphics {

class PipelineCache {
public:
    explicit PipelineCache(const Context& context) : context(context) {
        const VkPipelineCacheCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
        Check(context.Function<PFN_vkCreatePipelineCache>("vkCreatePipelineCache")(context.device, &info, nullptr, &cache), "vkCreatePipelineCache");
    }

    ~PipelineCache() {
        context.Function<PFN_vkDestroyPipelineCache>("vkDestroyPipelineCache")(context.device, cache, nullptr);
    }

    PipelineCache(const PipelineCache&) = delete;
    PipelineCache& operator=(const PipelineCache&) = delete;
    VkPipelineCache Handle() const { return cache; }

private:
    Context context;
    VkPipelineCache cache = VK_NULL_HANDLE;
};

}

#endif
