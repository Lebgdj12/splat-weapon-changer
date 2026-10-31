// kernel.hpp - Kernel-level memory write (from CodePatchPlugin by theawesomecoder61)
//
// This is the EXACT function from theawesomecoder61/CodePatchPlugin (src/kernel.hpp).
// CodePatchPlugin is a WORKING Aroma plugin that patches game code.
//
// The key differences from my previous attempts:
// 1. Uses KernelCopyData from <kernel/kernel.h> (libkernel), NOT inline asm SC0x25
// 2. Calls ICInvalidateRange on BOTH source and destination (flushes instruction cache)
// 3. Uses OSEffectiveToPhysical to translate virtual -> physical
// 4. DCFlushRange on both source and destination
//
// ICInvalidateRange is CRITICAL for code section writes - without it, the
// instruction cache holds stale instructions and the patched code doesn't run.

#pragma once
#include <coreinit/cache.h>
#include <coreinit/memorymap.h>
#include <wums.h>
#include <kernel/kernel.h>

// Write a 32-bit value to any virtual address via kernel copy.
// Returns true if the write succeeded, false if the address was invalid.
// Pattern from arGecko by quarky (https://git.sr.ht/~quarky/arGecko)
inline bool KernelWriteU32(uint32_t addr, uint32_t value) {
    // Check if the address maps to valid physical memory.
    // arGecko does this check and skips writes to invalid addresses.
    uint32_t phys_address = (uint32_t) OSEffectiveToPhysical(addr);
    if (!phys_address) {
        return false;  // bad address - skip the write
    }

    // Flush source caches before the kernel copy
    ICInvalidateRange(&value, 4);
    DCFlushRange(&value, 4);

    auto src = (uint32_t) OSEffectiveToPhysical((uint32_t) &value);

    // Kernel copy between physical addresses - bypasses ALL virtual protections
    KernelCopyData(phys_address, src, 4);

    // Flush destination caches after the copy
    DCFlushRange((void *) addr, 4);
    ICInvalidateRange((void *) addr, 4);
    return true;
}
