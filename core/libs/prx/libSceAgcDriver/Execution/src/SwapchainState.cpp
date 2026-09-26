#include "prx/libSceAgcDriver/Execution/include/SwapchainState.hpp"
#include <stdexcept>
#include <string>

namespace AgcDriver {

bool SwapchainState::NeedsRecreation() const {
    return outdated;
}

void SwapchainState::Recreated() {
    outdated = false;
}

bool SwapchainState::ProcessResult(VkResult result, const char* operation) {
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        outdated = true;
        return false;
    }
    if (result == VK_SUBOPTIMAL_KHR) {
        outdated = true;
        return true;
    }
    if (result != VK_SUCCESS) throw std::runtime_error(std::string(operation) + ": Vulkan result " + std::to_string(result));
    return true;
}

}
