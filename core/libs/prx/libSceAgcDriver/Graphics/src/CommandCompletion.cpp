#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"

namespace AgcDriver::Graphics {

bool CommandBatch::IsComplete() {
    Require(submitted, "cannot query an unsubmitted command batch");
    if (!pending) return true;
    const auto result = context.Function<PFN_vkGetFenceStatus>("vkGetFenceStatus")(context.device, fence);
    if (result == VK_NOT_READY) return false;
    Check(result, "vkGetFenceStatus graphics");
    pending = false;
    return true;
}

}
