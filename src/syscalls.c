// syscalls.c - Kernel syscall implementations (PPC assembly)
//
// Copied from PinkDiamondTeam/TCPGecko-Plugin (src/tcpgecko/libs/kernel/syscalls.c)
// Simplified: removed logger dependencies, kept only the raw asm.
//
// These issue PPC `sc` (system call) instructions with specific syscall
// numbers (0x25, 0x34, 0x35) that the Cafe OS kernel handles with
// kernel-level privileges. This bypasses userspace memory protections.

#include "syscalls.h"

// SC0x25: Kernel copy data between physical addresses.
// Args: r3=dest_phys, r4=src_phys, r5=length
void SC0x25_KernelCopyData(unsigned int phys_dst, unsigned int phys_src, unsigned int length)
{
    (void)phys_dst; (void)phys_src; (void)length;
    __asm__ volatile (
        "li 0, 0x2500\n"
        "sc\n"
        "blr\n"
    );
}

// SC0x34: Read 32-bit from any virtual address with kernel perms.
// Returns the value in r3.
uint32_t __attribute__((noinline)) kern_read(const void *addr) {
    uint32_t result;
    __asm__ volatile (
        "li 3,1\n"
        "li 4,0\n"
        "li 5,0\n"
        "li 6,0\n"
        "li 7,0\n"
        "lis 8,1\n"
        "mr 9,%1\n"      // addr -> r9
        "li 0,0x3400\n"  // syscall 0x34
        "mr %0,1\n"      // save r1
        "sc\n"
        "nop\n"
        "mr 1,%0\n"      // restore r1
        "mr %0,3\n"      // r3 -> result
        : "=r"(result)
        : "b"(addr)
        : "memory", "ctr", "lr", "0", "3", "4", "5", "6", "7", "8", "9", "10",
          "11", "12"
    );
    return result;
}

// SC0x35: Write 32-bit to any virtual address with kernel perms.
// Args: addr in r9, value in r5.
void __attribute__((noinline)) kern_write(void *addr, uint32_t value) {
    __asm__ volatile (
        "li 3,1\n"
        "li 4,0\n"
        "mr 5,%1\n"      // value -> r5
        "li 6,0\n"
        "li 7,0\n"
        "lis 8,1\n"
        "mr 9,%0\n"      // addr -> r9
        "mr %1,1\n"      // save r1
        "li 0,0x3500\n"  // syscall 0x35
        "sc\n"
        "nop\n"
        "mr 1,%1\n"      // restore r1
        :
        : "r"(addr), "r"(value)
        : "memory", "ctr", "lr", "0", "3", "4", "5", "6", "7", "8", "9", "10",
          "11", "12"
    );
}
