#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_SPECIFICS_ITANIUMABI_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_SPECIFICS_ITANIUMABI_HPP

#include <cxxabi.h>
#include <cstddef>
#include <typeinfo>

// Apple's libc++abi implements the Itanium C++ ABI but hides the internal
// type_info subclasses from <cxxabi.h>. The declarations below are the
// canonical Itanium C++ ABI data layouts, identical to what libc++abi compiles
// with on Darwin. They are reinterpret views over real libc++abi objects:
// they must never be constructed, copied or destroyed here - only used to
// read the ABI fields of guest RTTI received as std::type_info pointers.

namespace __cxxabiv1 {

class __class_type_info;

struct __base_class_type_info {
    const __class_type_info* __base_type;
    long __offset_flags;

    enum __offset_flags_masks {
        __virtual_mask = 0x1,
        __public_mask = 0x2,
        __offset_shift = 8
    };
};

class __class_type_info : public std::type_info {};

class __si_class_type_info : public __class_type_info {
public:
    const __class_type_info* __base_type;
};

class __vmi_class_type_info : public __class_type_info {
public:
    unsigned int __base_count;
    const __base_class_type_info* __base_info;
};

class __pbase_type_info : public std::type_info {
public:
    unsigned int __flags;
    const std::type_info* __pointee;

    enum __masks {
        __const_mask = 0x1,
        __volatile_mask = 0x2,
        __restrict_mask = 0x4,
        __incomplete_mask = 0x8,
        __incomplete_class_mask = 0x10,
        __transaction_mask = 0x20
    };
};

class __pointer_type_info : public __pbase_type_info {};

class __pointer_to_member_type_info : public __pbase_type_info {
public:
    const __class_type_info* __context;
};

class __function_type_info : public __class_type_info {};

}

#endif
