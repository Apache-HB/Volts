#pragma once

#include "svl/types.h"

namespace volts::ppu
{
    union control
    {
        svl::u8 bytes[32];
        svl::u32 fields[8];
    };

    static_assert(sizeof(control) == 32);

    struct thread
    {
        svl::u64 gpr[32] = {};
        svl::f64 fpr[32] = {};

        svl::u64 link = 0;
        svl::u64 count = 0;
        svl::u64 xer = 0;

        control cr = {};
        // todo: fixed point exception

        svl::v128 vr[32] = {};

        // todo: vector status
        // vr save register
    };
}