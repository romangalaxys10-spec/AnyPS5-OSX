#include "Optimization/SharedMemoryBarrierInserter.hpp"
#include <stdexcept>

namespace ShaderRecompiler {

SharedMemoryBarrierStats SharedMemoryBarrierInserter::Insert(IrProgram& program, std::uint32_t waveSize) const {
    throw std::runtime_error("SharedMemoryBarrierInserter::Insert not implemented");
}

}
