/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 * Copyright 2022, UNSW (ABN 57 195 873 179)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "fault.h"
#include "util/util.h"

#define CPSR_THUMB                 (1 << 5)
#define CPSR_IS_THUMB(x)           ((x) & CPSR_THUMB)
#define HSR_SYNDROME_VALID         (1 << 24)
#define HSR_IS_SYNDROME_VALID(hsr) ((hsr) & HSR_SYNDROME_VALID)
#define HSR_SYNDROME_WIDTH(x)      (((x) >> 22) & 0x3)
#define HSR_SYNDROME_RT(x)         (((x) >> 16) & 0x1f)

// int fault_is_32bit_instruction(seL4_UserContext *regs)
// {
//     // @ivanv: assuming VCPU fault
//     return !CPSR_IS_THUMB(regs->spsr);
// }

bool fault_advance_vcpu(seL4_UserContext *regs) {
    // For now we just ignore it and continue
    // Assume 64-bit instruction
    regs->pc += 4;
    // @ivanv: should this be true or false?
    int err = seL4_TCB_WriteRegisters(BASE_VM_TCB_CAP + VM_ID, true, 0, SEL4_USER_CONTEXT_SIZE, regs);
    assert(err == seL4_NoError);
    /* Reply to thread */
    reply_to_fault();

    return (err != 0);
}

enum fault_width {
    WIDTH_DOUBLEWORD,
    WIDTH_WORD,
    WIDTH_HALFWORD,
    WIDTH_BYTE
};

enum fault_width fault_get_width(uint64_t fsr)
{
    // @ivanv: what is HSR syndrome??
    if (HSR_IS_SYNDROME_VALID(fsr)) {
        switch (HSR_SYNDROME_WIDTH(fsr)) {
        case 0: return WIDTH_BYTE;
        case 1: return WIDTH_HALFWORD;
        case 2: return WIDTH_WORD;
        case 3: return WIDTH_DOUBLEWORD;
        default:
            // @ivanv: reviist
            // print_fault(f);
            assert(0);
            return 0;
        }
    } else {
        // @ivanv: reviist
        // int rt;
        // rt = decode_instruction(f);
        // assert(rt >= 0);
    }

    assert(0);
    // @ivanv: come back to
    return 0;
}

uint64_t fault_get_data_mask(uint64_t addr, uint64_t fsr)
{
    uint64_t mask = 0;
    switch (fault_get_width(fsr)) {
        case WIDTH_BYTE:
            mask = 0x000000ff;
            assert(!(addr & 0x0));
            break;
        case WIDTH_HALFWORD:
            mask = 0x0000ffff;
            assert(!(addr & 0x1));
            break;
        case WIDTH_WORD:
            mask = 0xffffffff;
            assert(!(addr & 0x3));
            break;
        case WIDTH_DOUBLEWORD:
            mask = ~mask;
            break;
        default:
            /* Should never get here... Keep the compiler happy */
            printf("VMM|ERROR: unknown width: 0x%lx, from FSR: 0x%lx\n", fault_get_width(fsr), fsr);
            assert(0);
            return 0;
    }
    mask <<= (addr & 0x3) * 8;
    return mask;
}

static uint64_t wzr = 0;
uint64_t *decode_rt(uint64_t reg, seL4_UserContext *regs)
{
    switch (reg) {
        case 0: return &regs->x0;
        case 1: return &regs->x1;
        case 2: return &regs->x2;
        case 3: return &regs->x3;
        case 4: return &regs->x4;
        case 5: return &regs->x5;
        case 6: return &regs->x6;
        case 7: return &regs->x7;
        case 8: return &regs->x8;
        case 9: return &regs->x9;
        case 10: return &regs->x10;
        case 11: return &regs->x11;
        case 12: return &regs->x12;
        case 13: return &regs->x13;
        case 14: return &regs->x14;
        case 15: return &regs->x15;
        case 16: return &regs->x16;
        case 17: return &regs->x17;
        case 18: return &regs->x18;
        case 19: return &regs->x19;
        case 20: return &regs->x20;
        case 21: return &regs->x21;
        case 22: return &regs->x22;
        case 23: return &regs->x23;
        case 24: return &regs->x24;
        case 25: return &regs->x25;
        case 26: return &regs->x26;
        case 27: return &regs->x27;
        case 28: return &regs->x28;
        case 29: return &regs->x29;
        case 30: return &regs->x30;
        case 31: return &wzr;
        default:
            printf("invalid reg %d\n", reg);
            assert(!"Invalid register");
            return NULL;
    }
}

bool fault_is_write(uint64_t fsr)
{
    return (fsr & (1U << 6)) != 0;
}

bool fault_is_read(uint64_t fsr)
{
    return !fault_is_write(fsr);
}

static int get_rt(uint64_t fsr)
{

    int rt = -1;
    if (HSR_IS_SYNDROME_VALID(fsr)) {
        rt = HSR_SYNDROME_RT(fsr);
    } else {
        printf("decode_insturction for arm64 not implemented\n");
        // @ivanv: implement decode instruction for aarch64
        // rt = decode_instruction(f);
    }
    assert(rt >= 0);
    return rt;
}

uint64_t fault_get_data(seL4_UserContext *regs, uint64_t fsr)
{
    /* Get register opearand */
    int rt  = get_rt(fsr);

    uint64_t data = *decode_rt(rt, regs);

    return data;
}

uint64_t fault_emulate(seL4_UserContext *regs, uint64_t reg, uint64_t addr, uint64_t fsr, uint64_t reg_val)
{
    uint64_t m, s;
    s = (addr & 0x3) * 8;
    m = fault_get_data_mask(addr, fsr);
    if (fault_is_read(fsr)) {
        /* Read data must be shifted to lsb */
        return (reg & ~(m >> s)) | ((reg_val & m) >> s);
    } else {
        /* Data to write must be shifted left to compensate for alignment */
        return (reg & ~m) | ((reg_val << s) & m);
    }
}

bool fault_advance(seL4_UserContext *regs, uint64_t addr, uint64_t fsr, uint64_t reg_val)
{
    /* Get register opearand */
    int rt = get_rt(fsr);

    uint64_t *reg_ctx = decode_rt(rt, regs);
    *reg_ctx = fault_emulate(regs, *reg_ctx, addr, fsr, reg_val);
    // DFAULT("%s: Emulate fault @ 0x%x from PC 0x%x\n",
    //        fault->vcpu->vm->vm_name, fault->addr, fault->ip);

    return fault_advance_vcpu(regs);
}
