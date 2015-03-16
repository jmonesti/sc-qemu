#include "qemu-common.h"
#include "qemu/main-loop.h"
#include "hw/sysbus.h"
#include "exec/address-spaces.h"
#include "sysemu/sysemu.h"
#include "hw/arm/arm.h"

#include "sc_qemu.h"
#include "sc_machine.h"

qemu_context* SC_QEMU_INIT_SYM(qemu_import *q, systemc_import *s, void *opaque);

typedef struct mmio_ctx mmio_ctx;

struct mmio_ctx {
    qemu_context *qemu_ctx;
    uint32_t base;
};

static uint64_t sc_mmio_read(void *opaque, hwaddr offset,
                           unsigned size)
{
    mmio_ctx *ctx = (mmio_ctx*) opaque;

    return ctx->qemu_ctx->sysc.read(ctx->qemu_ctx->opaque,
                                    ctx->base + offset, size);
}

static void sc_mmio_write(void *opaque, hwaddr offset,
                        uint64_t value, unsigned size)
{
    mmio_ctx *ctx = (mmio_ctx*) opaque;

    ctx->qemu_ctx->sysc.write(ctx->qemu_ctx->opaque,
                              ctx->base + offset, value, size);
}

static const MemoryRegionOps sc_mmio_ops = {
    .read = sc_mmio_read,
    .write = sc_mmio_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};


/* vl.c */
bool main_loop_should_exit(void);
/* ---- */

static bool sc_qemu_cpu_loop(qemu_context *ctx)
{
    main_loop_wait(true);

    return main_loop_should_exit();
}

static void sc_qemu_irq_update(qemu_context *ctx, int cpu_mask,
                               int level)
{
    qemu_irq i;

    i = qdev_get_gpio_in(DEVICE(ctx->cpu), ARM_CPU_IRQ);

    qemu_set_irq(i, level);
}

static void sc_qemu_map_io(qemu_context *ctx, uint32_t base_address,
                           uint32_t size)
{
    MemoryRegion *sysmem = get_system_memory();
    MemoryRegion *mmio = g_new(MemoryRegion, 1);
    mmio_ctx *m_ctx = g_new(mmio_ctx, 1);

    m_ctx->base = base_address;
    m_ctx->qemu_ctx = ctx;

    memory_region_init_io(mmio, NULL, &sc_mmio_ops, m_ctx, "sc-mmio", size);
    memory_region_add_subregion(sysmem, base_address, mmio);
}

static void sc_qemu_map_dmi(qemu_context *ctx, uint32_t base_address,
                            uint32_t size, void* data)
{
    MemoryRegion *sysmem = get_system_memory();
    MemoryRegion *dmi = g_new(MemoryRegion, 1);

    memory_region_init_ram_ptr(dmi, NULL, "sc-dmi", size, data);
    vmstate_register_ram_global(dmi);
    memory_region_add_subregion(sysmem, base_address, dmi);
}


int qemu_main(int argc, char const * argv[], char **envp);

qemu_context* SC_QEMU_INIT_SYM(qemu_import *q, systemc_import *s, void *opaque)
{
    char const * qemu_argv[] = {
        "qemu",
        "-M", "sc-qemu",
        "-s", "-S",
        /*
        "-d", "in_asm,exec",
        "-D", "qemu.log",
        */
    };
    qemu_context *ctx;
    int qemu_argc = ARRAY_SIZE(qemu_argv);

    qemu_main(qemu_argc, qemu_argv, NULL);

    /* SystemC to QEMU fonctions */
    q->cpu_loop = sc_qemu_cpu_loop;
    q->irq_update = sc_qemu_irq_update;
    q->map_io = sc_qemu_map_io;
    q->map_dmi = sc_qemu_map_dmi;

    ctx = sc_qemu_machine_get_context();

    ctx->opaque = opaque;

    /* QEMU to SystemC functions */
    memcpy(&(ctx->sysc), s, sizeof(systemc_import));


    return ctx;
}

