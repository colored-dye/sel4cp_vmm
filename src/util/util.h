/*
 * Copyright 2021, Breakaway Consulting Pty. Ltd.
 * Copyright 2022, UNSW (ABN 57 195 873 179)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once // @ivanv: actually understand if this is having an effect

#include <stdint.h>
#include <sel4cp.h>
#include "printf.h"

// @ivanv: these are here for convience, should not be here though
#define VM_ID 1
#define VCPU_ID 0
#define NUM_VCPUS 1
// Note that this is AArch64 specific
#define SEL4_USER_CONTEXT_SIZE 0x24

#define PAGE_SIZE_4K 4096

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

#define CTZ(x) __builtin_ctz(x)

#if __STDC_VERSION__ >= 201112L && !defined(__cplusplus)
#define static_assert _Static_assert
#endif

//  __attribute__ ((__noreturn__))
// void __assert_func(const char *file, int line, const char *function, const char *str);

void _putchar(char character);

#define DEBUG_VMM

#if defined(DEBUG_VMM)
#define LOG_VMM(...) do{ printf("%s|INFO: ", sel4cp_name); printf(__VA_ARGS__); }while(0)
#else
#define LOG_VMM(...) do{}while(0)
#endif

#if defined(BOARD_imx8mm_evk)
#define GIC_V3
#else
#define GIC_V2
#endif

static char
decchar(unsigned int v) {
    return '0' + v;
}

static void
put8(uint8_t x)
{
    char tmp[4];
    unsigned i = 3;
    tmp[3] = 0;
    do {
        uint8_t c = x % 10;
        tmp[--i] = decchar(c);
        x /= 10;
    } while (x);
    sel4cp_dbg_puts(&tmp[i]);
}

// @ivanv: sort this out...
static void
reply_to_fault()
{
    sel4cp_msginfo msg = sel4cp_msginfo_new(0, 0);
    seL4_Send(4, msg);
}

static uint64_t get_vmm_id(char *sel4cp_name)
{
    // @ivanv: Absolute hack
    return sel4cp_name[4] - '0';
}

static void
dump_ctx(seL4_UserContext *ctx) {
#if defined(ARCH_aarch64)
    // I don't know if it's the best idea, but here we'll just dump the
    // registers in the same order they are defined in seL4_UserContext
    printf("TCB registers:\n");
    // Frame registers
    printf("    pc:   0x%016lx\n", ctx->pc);
    printf("    sp:   0x%016lx\n", ctx->sp);
    printf("    spsr: 0x%016lx\n", ctx->spsr);
    printf("    x0:   0x%016lx\n", ctx->x0);
    printf("    x1:   0x%016lx\n", ctx->x1);
    printf("    x2:   0x%016lx\n", ctx->x2);
    printf("    x3:   0x%016lx\n", ctx->x3);
    printf("    x4:   0x%016lx\n", ctx->x4);
    printf("    x5:   0x%016lx\n", ctx->x5);
    printf("    x6:   0x%016lx\n", ctx->x6);
    printf("    x7:   0x%016lx\n", ctx->x7);
    printf("    x8:   0x%016lx\n", ctx->x8);
    printf("    x16:  0x%016lx\n", ctx->x16);
    printf("    x17:  0x%016lx\n", ctx->x17);
    printf("    x18:  0x%016lx\n", ctx->x18);
    printf("    x29:  0x%016lx\n", ctx->x29);
    printf("    x30:  0x%016lx\n", ctx->x30);
    // Other integer registers
    printf("    x9:   0x%016lx\n", ctx->x9);
    printf("    x10:  0x%016lx\n", ctx->x10);
    printf("    x11:  0x%016lx\n", ctx->x11);
    printf("    x12:  0x%016lx\n", ctx->x12);
    printf("    x13:  0x%016lx\n", ctx->x13);
    printf("    x14:  0x%016lx\n", ctx->x14);
    printf("    x15:  0x%016lx\n", ctx->x15);
    printf("    x19:  0x%016lx\n", ctx->x19);
    printf("    x20:  0x%016lx\n", ctx->x20);
    printf("    x21:  0x%016lx\n", ctx->x21);
    printf("    x22:  0x%016lx\n", ctx->x22);
    printf("    x23:  0x%016lx\n", ctx->x23);
    printf("    x24:  0x%016lx\n", ctx->x24);
    printf("    x25:  0x%016lx\n", ctx->x25);
    printf("    x26:  0x%016lx\n", ctx->x26);
    printf("    x27:  0x%016lx\n", ctx->x27);
    printf("    x28:  0x%016lx\n", ctx->x28);
    // TODO(ivanv): print out thread ID registers?
#endif
    // @ivanv: riscv64 print regs
}

static void assert_fail(
    const char  *assertion,
    const char  *file,
    unsigned int line,
    const char  *function)
{
    printf("Failed assertion '");
    printf(assertion);
    printf("' at ");
    printf(file);
    printf(":");
    put8(line);
    printf(" in function ");
    printf(function);
    printf("\n");
    while (1) {}
}

#define assert(expr) \
    do { \
        if (!(expr)) { \
            assert_fail(#expr, __FILE__, __LINE__, __FUNCTION__); \
        } \
    } while(0)

