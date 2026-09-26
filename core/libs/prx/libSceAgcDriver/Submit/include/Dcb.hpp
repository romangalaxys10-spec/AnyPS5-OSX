#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_SUBMIT_INCLUDE_DCB_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_SUBMIT_INCLUDE_DCB_HPP

#include "SceTypes.hpp"

extern "C" int APS5_VABI sceAgcDriverSubmitDcb(const Packet* packet);
extern "C" int APS5_VABI sceAgcDriverAgrSubmitDcb(const Packet* packet);

#endif
