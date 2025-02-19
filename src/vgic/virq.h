/*
 * Copyright 2019, Data61, CSIRO (ABN 41 687 119 230)
 * Copyright 2022, UNSW (ABN 57 195 873 179)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../util/util.h"

/* The ARM GIC architecture defines 16 SGIs (0 - 7 is recommended for non-secure
 * state, 8 - 15 for secure state), 16 PPIs (interrupt 16 - 31) and 988 SPIs
 * (32 - 1019). The interrupt IDs 1020 - 1023 are used for special purposes.
 * GICv3.1 is not implemented here, it supports 64 additional PPIs (interrupt
 * 1056 - 1119) and 1024 SPIs (interrupt 4096 – 5119). LPIs starting at
 * interrupt 8192 are also not implemented here.
 */
#define NUM_SGI_VIRQS           16   // vCPU local SGI interrupts
#define NUM_PPI_VIRQS           16   // vCPU local PPI interrupts
#define NUM_VCPU_LOCAL_VIRQS    (NUM_SGI_VIRQS + NUM_PPI_VIRQS)

/* Usually, VMs do not use all SPIs. To reduce the memory footprint, our vGIC
 * implementation manages the SPIs in a fixed size slot list. 200 entries have
 * been good trade-off that is sufficient for most systems.
 */
#define NUM_SLOTS_SPI_VIRQ      200

#define VIRQ_INVALID -1

typedef void (*irq_ack_fn_t)(uint64_t vcpu_id, int irq, void *cookie);

struct virq_handle {
    int virq;
    irq_ack_fn_t ack_fn;
    void *ack_data;
};

// @ivanv: revisit
static inline void virq_ack(uint64_t vcpu_id, struct virq_handle *irq)
{
    // printf("VGIC|INFO: Acking for vIRQ %d\n", irq->virq);
    assert(irq->ack_fn);
    irq->ack_fn(vcpu_id, irq->virq, NULL);
}

/* TODO: A typical number of list registers supported by GIC is four, but not
 * always. One particular way to probe the number of registers is to inject a
 * dummy IRQ with seL4_ARM_VCPU_InjectIRQ(), using LR index high enough to be
 * not supported by any target; the kernel will reply with the supported range
 * of LR indexes.
 */
#define NUM_LIST_REGS 4
/* This is a rather arbitrary number, increase if needed. */
#define MAX_IRQ_QUEUE_LEN 64
#define IRQ_QUEUE_NEXT(_i) (((_i) + 1) & (MAX_IRQ_QUEUE_LEN - 1))

static_assert((MAX_IRQ_QUEUE_LEN & (MAX_IRQ_QUEUE_LEN - 1)) == 0,
              "IRQ ring buffer size must be power of two");

struct irq_queue {
    struct virq_handle *irqs[MAX_IRQ_QUEUE_LEN]; /* circular buffer */
    uint64_t head;
    uint64_t tail;
};

/* vCPU specific interrupt context */
typedef struct vgic_vcpu {
    /* Mirrors the GIC's vCPU list registers */
    struct virq_handle lr_shadow[NUM_LIST_REGS];
    /* Queue for IRQs that don't fit in the GIC's vCPU list registers */
    struct irq_queue irq_queue;
    /*  vCPU local interrupts (SGI, PPI) */
    struct virq_handle local_virqs[NUM_VCPU_LOCAL_VIRQS];
} vgic_vcpu_t;

/* GIC global interrupt context */
typedef struct vgic {
    /* virtual registers */
    void *registers;
    /* registered global interrupts (SPI) */
    struct virq_handle vspis[NUM_SLOTS_SPI_VIRQ];
    /* vCPU specific interrupt context */
    vgic_vcpu_t vgic_vcpu[NUM_VCPUS];
} vgic_t;

static inline vgic_vcpu_t *get_vgic_vcpu(vgic_t *vgic, int vcpu_id)
{
    assert(vgic);
    assert((vcpu_id >= 0) && (vcpu_id < ARRAY_SIZE(vgic->vgic_vcpu)));
    return &(vgic->vgic_vcpu[vcpu_id]);
}

static inline struct virq_handle *virq_get_sgi_ppi(vgic_t *vgic, uint64_t vcpu_id, int virq)
{
    vgic_vcpu_t *vgic_vcpu = get_vgic_vcpu(vgic, vcpu_id);
    assert(vgic_vcpu);
    assert((virq >= 0) && (virq < ARRAY_SIZE(vgic_vcpu->local_virqs)));
    return &vgic_vcpu->local_virqs[virq];
}

static inline struct virq_handle *virq_find_spi_irq_data(struct vgic *vgic, int virq)
{
    for (int i = 0; i < ARRAY_SIZE(vgic->vspis); i++) {
        if (vgic->vspis[i].virq != VIRQ_INVALID && vgic->vspis[i].virq == virq) {
            return &vgic->vspis[i];
        }
    }
    return NULL;
}

static inline struct virq_handle *virq_find_irq_data(struct vgic *vgic, uint64_t vcpu_id, int virq)
{
    if (virq < NUM_VCPU_LOCAL_VIRQS)  {
        return virq_get_sgi_ppi(vgic, vcpu_id, virq);
    }
    return virq_find_spi_irq_data(vgic, virq);
}

static inline bool virq_spi_add(vgic_t *vgic, struct virq_handle *virq_data)
{
    for (int i = 0; i < ARRAY_SIZE(vgic->vspis); i++) {
        if (vgic->vspis[i].virq == VIRQ_INVALID) {
            vgic->vspis[i] = *virq_data;
            return true;
        }
    }
    return false;
}

static inline bool virq_sgi_ppi_add(uint64_t vcpu_id, vgic_t *vgic, struct virq_handle *virq_data)
{
    // @ivanv: revisit
    vgic_vcpu_t *vgic_vcpu = get_vgic_vcpu(vgic, vcpu_id);
    assert(vgic_vcpu);
    int irq = virq_data->virq;
    assert((irq >= 0) && (irq < ARRAY_SIZE(vgic_vcpu->local_virqs)));
    struct virq_handle *slot = &vgic_vcpu->local_virqs[irq];
    if (slot->virq != VIRQ_INVALID) {
        printf("VMM|ERROR: IRQ %d already registered on VCPU %u", virq_data->virq, vcpu_id);
        return false;
    }
    *slot = *virq_data;
    return true;
}

static inline bool virq_add(uint64_t vcpu_id, vgic_t *vgic, struct virq_handle *virq_handle)
{
    if (virq_handle->virq < NUM_VCPU_LOCAL_VIRQS) {
        return virq_sgi_ppi_add(vcpu_id, vgic, virq_handle);
    }
    return virq_spi_add(vgic, virq_handle);
}

static inline int vgic_irq_enqueue(vgic_t *vgic, uint64_t vcpu_id, struct virq_handle *irq)
{
    vgic_vcpu_t *vgic_vcpu = get_vgic_vcpu(vgic, vcpu_id);
    assert(vgic_vcpu);
    struct irq_queue *q = &vgic_vcpu->irq_queue;

    // @ivanv: add "unlikely" call
    if (IRQ_QUEUE_NEXT(q->tail) == q->head) {
        return -1;
    }

    q->irqs[q->tail] = irq;
    q->tail = IRQ_QUEUE_NEXT(q->tail);

    return 0;
}

static inline struct virq_handle *vgic_irq_dequeue(vgic_t *vgic, uint64_t vcpu_id)
{
    vgic_vcpu_t *vgic_vcpu = get_vgic_vcpu(vgic, vcpu_id);
    assert(vgic_vcpu);
    struct irq_queue *q = &vgic_vcpu->irq_queue;
    struct virq_handle *virq = NULL;

    if (q->head != q->tail) {
        virq = q->irqs[q->head];
        q->head = IRQ_QUEUE_NEXT(q->head);
    }

    return virq;
}

static inline int vgic_find_empty_list_reg(vgic_t *vgic, uint64_t vcpu_id)
{
    vgic_vcpu_t *vgic_vcpu = get_vgic_vcpu(vgic, vcpu_id);
    assert(vgic_vcpu);
    for (int i = 0; i < ARRAY_SIZE(vgic_vcpu->lr_shadow); i++) {
        if (vgic_vcpu->lr_shadow[i].virq == VIRQ_INVALID) {
            return i;
        }
    }

    return -1;
}

static inline bool vgic_vcpu_load_list_reg(vgic_t *vgic, uint64_t vcpu_id, int idx, int group, struct virq_handle *virq)
{
    vgic_vcpu_t *vgic_vcpu = get_vgic_vcpu(vgic, vcpu_id);
    assert(vgic_vcpu);
    assert((idx >= 0) && (idx < ARRAY_SIZE(vgic_vcpu->lr_shadow)));

    // @ivanv: why is the priority 0?
    uint64_t err = sel4cp_arm_vcpu_inject_irq(VM_ID, virq->virq, 0, group, idx);
    assert(err == seL4_NoError);
    if (err) {
        printf("VMM|ERROR: Failure loading vGIC list register (error %d)", err);
        return false;
    }

    vgic_vcpu->lr_shadow[idx] = *virq;

    return true;
}
