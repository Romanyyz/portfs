#ifndef SHARED_STRUCTS_H
#define SHARED_STRUCTS_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <cstdint>
#endif // __KERNEL__

struct MyStruct {
    int id;
    __u64 kernel_specific_field;
};


struct MyStruct {
    int id;
    uint64_t user_specific_field;
};

#endif // SHARED_STRUCTS_H
