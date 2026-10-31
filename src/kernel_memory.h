// kernel_memory.h - Safe kernel-level memory access
//
// This is the approach TCPGecko-Plugin ACTUALLY uses (src/tcpgecko/kernel.h).
// They do NOT call kern_write/kern_read directly for game memory. Instead they
// use SC0x25_KernelCopyData (syscall 0x25) which copies between PHYSICAL
// addresses, combined with OSEffectiveToPhysical to convert virtual->physical.
//
// This is safer than kern_write (syscall 0x35) because:
// - SC0x25 is the official kernel data copy syscall, always available under Aroma
// - Physical addresses bypass ALL virtual memory protections
// - The staging buffer ensures the source is in stable, cache-aligned memory
//
// Without this approach, direct kern_write to certain addresses crashes the
// plugin (DSI exception) and takes the game down with it.

#ifndef WUPS_KERNEL_MEMORY_H
#define WUPS_KERNEL_MEMORY_H

#include <cstdint>
#include <cstring>
#include <coreinit/cache.h>
#include <coreinit/memorymap.h>
#include "syscalls.h"

// Staging buffer - must be cache-aligned (0x40 = 64 bytes).
// TCPGecko uses the same alignment for their kernelCopyStagingBuffer.
static alignas(0x40) unsigned char g_kernelStagingBuffer[64];

// Copy `length` bytes from sourceBuffer to destinationBuffer using kernel
// syscall 0x25 (SC0x25_KernelCopyData). Both addresses are virtual; they get
// translated to physical via OSEffectiveToPhysical before the syscall.
//
// The source is first copied to the staging buffer to ensure it's in stable
// memory (the original source might be in a page that gets remapped).
static inline void kernelCopyData(void *destinationBuffer, const void *sourceBuffer, uint32_t length) {
    if (length > sizeof(g_kernelStagingBuffer)) length = sizeof(g_kernelStagingBuffer);

    // Stage source data in our aligned buffer
    memcpy(g_kernelStagingBuffer, sourceBuffer, length);
    DCFlushRange(g_kernelStagingBuffer, length);

    // Translate virtual -> physical for both addresses
    uint32_t physDst = (uint32_t) OSEffectiveToPhysical((uint32_t) destinationBuffer);
    uint32_t physSrc = (uint32_t) OSEffectiveToPhysical((uint32_t) g_kernelStagingBuffer);

    // Kernel copy between physical addresses - bypasses all virtual protections
    SC0x25_KernelCopyData(physDst, physSrc, length);

    // Flush destination cache so the write is visible to other CPUs
    DCFlushRange(destinationBuffer, length);
}

// Write a 32-bit value to any virtual address (including read-only game code).
static inline void writeKernelMemory(uintptr_t addr, uint32_t value) {
    kernelCopyData(reinterpret_cast<void*>(addr), &value, sizeof(uint32_t));
}

// Read a 32-bit value from any virtual address.
// For addresses > 0xF0000000 (kernel space), use kern_read directly.
// For game memory, use the copy approach (read INTO staging buffer).
static inline uint32_t readKernelMemory(uintptr_t addr) {
    if (addr > 0xF0000000u) {
        return kern_read(reinterpret_cast<const void*>(addr));
    }
    uint32_t result = 0;
    kernelCopyData(&result, reinterpret_cast<const void*>(addr), sizeof(uint32_t));
    return result;
}

#endif // WUPS_KERNEL_MEMORY_H
