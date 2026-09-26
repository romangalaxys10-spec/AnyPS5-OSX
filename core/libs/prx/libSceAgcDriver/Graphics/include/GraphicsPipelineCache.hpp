#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GRAPHICSPIPELINECACHE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GRAPHICSPIPELINECACHE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RenderCache.hpp"
#include <list>
#include <map>
#include <string>

namespace AgcDriver::Graphics {

class GraphicsPipelineCache {
public:
    explicit GraphicsPipelineCache(const Context& context) : context(context) {}
    std::shared_ptr<Pipeline> Get(const State& state, const std::shared_ptr<ResidentColor>& target, const ShaderResources& resources, std::span<const CompiledShader> shaders);

private:
    struct Entry {
        std::string key;
        std::shared_ptr<ResidentColor> target;
        std::shared_ptr<Pipeline> pipeline;
    };
    Context context;
    std::list<Entry> entries;
    std::map<std::string, std::list<Entry>::iterator> lookup;
};

}

#endif
