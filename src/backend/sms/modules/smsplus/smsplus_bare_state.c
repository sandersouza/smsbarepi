#include "../../../../../third_party/smsplus/shared.h"

void viewport_check(void);

int get_save_state_size(void)
{
    int size = 0;
    size += sizeof(vdp_t);
    size += sizeof(sms_t);
    size += 4;
    size += 0x8000;
    size += sizeof(Z80_Regs);
    size += sizeof(int);
    size += FM_GetContextSize();
    size += SN76489_GetContextSize();
    return size;
}

int save_state_to_mem(void *storage)
{
    uint8 *dst = (uint8 *) storage;
    if (dst == 0)
    {
        return 0;
    }

    memcpy(dst, &vdp, sizeof(vdp_t));
    dst += sizeof(vdp_t);
    memcpy(dst, &sms, sizeof(sms_t));
    dst += sizeof(sms_t);
    memcpy(dst, &cart.fcr[0], 1);
    dst += 1;
    memcpy(dst, &cart.fcr[1], 1);
    dst += 1;
    memcpy(dst, &cart.fcr[2], 1);
    dst += 1;
    memcpy(dst, &cart.fcr[3], 1);
    dst += 1;
    memcpy(dst, &cart.sram, 0x8000);
    dst += 0x8000;
    memcpy(dst, Z80_Context, sizeof(Z80_Regs));
    dst += sizeof(Z80_Regs);
    memcpy(dst, &after_EI, sizeof(int));
    dst += sizeof(int);
    memcpy(dst, FM_GetContextPtr(), FM_GetContextSize());
    dst += FM_GetContextSize();
    memcpy(dst, SN76489_GetContextPtr(0), SN76489_GetContextSize());
    return 1;
}

int load_state_from_mem(void *storage)
{
    int i;
    uint8 *src = (uint8 *) storage;
    if (src == 0)
    {
        return 0;
    }

    z80_reset(0);
    z80_set_irq_callback(sms_irq_callback);
    system_reset();
    if (snd.enabled)
    {
        sound_reset();
    }

    memcpy(&vdp, src, sizeof(vdp_t));
    src += sizeof(vdp_t);
    memcpy(&sms, src, sizeof(sms_t));
    src += sizeof(sms_t);
    memcpy(&cart.fcr[0], src, 1);
    src += 1;
    memcpy(&cart.fcr[1], src, 1);
    src += 1;
    memcpy(&cart.fcr[2], src, 1);
    src += 1;
    memcpy(&cart.fcr[3], src, 1);
    src += 1;
    memcpy(&cart.sram, src, 0x8000);
    src += 0x8000;
    memcpy(Z80_Context, src, sizeof(Z80_Regs));
    src += sizeof(Z80_Regs);
    memcpy(&after_EI, src, sizeof(int));
    src += sizeof(int);
    FM_SetContext(src);
    src += FM_GetContextSize();
    SN76489_SetContext(0, src);

    z80_set_irq_callback(sms_irq_callback);
    if (sms.console == CONSOLE_SMSJ)
    {
        cpu_writeport16 = smsj_port_w;
        cpu_readport16 = smsj_port_r;
    }
    else
    {
        cpu_writeport16 = sms_port_w;
        cpu_readport16 = sms_port_r;
    }

    for (i = 0x00; i <= 0x2F; i++)
    {
        cpu_readmap[i] = &cart.rom[(i & 0x1F) << 10];
        cpu_writemap[i] = dummy_write;
    }

    for (i = 0x30; i <= 0x3F; i++)
    {
        cpu_readmap[i] = &sms.wram[(i & 0x07) << 10];
        cpu_writemap[i] = &sms.wram[(i & 0x07) << 10];
    }

    sms_mapper_w(3, cart.fcr[3]);
    sms_mapper_w(2, cart.fcr[2]);
    sms_mapper_w(1, cart.fcr[1]);
    sms_mapper_w(0, cart.fcr[0]);

    bg_list_index = 0x200;
    for (i = 0; i < 0x200; i++)
    {
        bg_name_list[i] = i;
        bg_name_dirty[i] = -1;
    }

    for (i = 0; i < PALETTE_SIZE; i++)
    {
        palette_sync(i, 1);
    }

    viewport_check();
    return 1;
}
