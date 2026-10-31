// syscalls.h - Kernel-level memory access for WUPS plugins
//
// Copied from PinkDiamondTeam/TCPGecko-Plugin (src/tcpgecko/libs/kernel/syscalls.h)
// These functions issue PPC syscalls that run with kernel privileges, allowing
// reads/writes to ANY memory address - including the game's read-only code
// section. Without these, userspace writes to addresses like 0x105EBE40
// (inside the game's RPX) silently fail or crash the plugin.
//
// This is THE reason cheats weren't applying: my regular pointer writes
// worked for the player struct (in RW data section) but the absolute-
// address cheats (Speedhack @ 0x105EBE40 etc.) were hitting read-only
// memory and being dropped.
//
// Usage:
//   kern_write((void*)0x105EBE40, 0x3F100000u);  // write 0.5f speed mult
//   uint32_t v = kern_read((void*)0x106E46E8);   // read player base ptr

#ifndef WUPS_SYSCALLS_H
#define WUPS_SYSCALLS_H

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// Kernel copy of arbitrary data between physical addresses.
// Used by TCPGecko to install the code handler. We don't use this directly
// in our plugin (kern_write is enough for 4-byte writes).
void SC0x25_KernelCopyData(unsigned int phys_dst, unsigned int phys_src, unsigned int length);

// Read a 32-bit word from any address with kernel permissions.
// Bypasses userspace read protections (useful if the address is in a
// protected page, though for game data this is rarely needed).
uint32_t __attribute__((noinline)) kern_read(const void *addr);

// Write a 32-bit word to any address with kernel permissions.
// CRITICAL: this is what lets us write to the game's read-only code section
// (e.g. 0x105EBE40 for Speedhack, 0x1054BE1C for Extended Charger Range).
void __attribute__((noinline)) kern_write(void *addr, uint32_t value);

#ifdef __cplusplus
}
#endif

#endif // WUPS_SYSCALLS_H
