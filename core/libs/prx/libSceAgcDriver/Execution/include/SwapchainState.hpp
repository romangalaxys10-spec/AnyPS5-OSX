#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SWAPCHAINSTATE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SWAPCHAINSTATE_HPP

#include <vulkan/vulkan.h>

namespace AgcDriver {

class SwapchainState {
public:
    bool NeedsRecreation() const;
    void Recreated();
    bool ProcessResult(VkResult result, const char* operation);

private:
    bool outdated = false;
};

}

#endif
