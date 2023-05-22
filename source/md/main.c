#include <stdint.h>
#include <string.h>

#include "../common/comm_ids.h"
#include "megasd.h"
#include "cd_init.h"

extern void do_main(void);

// Communication channels                                               32X SIDE
static volatile uint16_t* const MD_SYS_COMM0 = (uint16_t*) 0xA15120; // 0x20004020
static volatile uint16_t* const MD_SYS_COMM2 = (uint16_t*) 0xA15122; // 0x20004022

// MD VDP ports
static volatile uint16_t* const vdp_data_port = (uint16_t*) 0xC00000;
static volatile uint16_t* const vdp_ctrl_port = (uint16_t*) 0xC00004;
static volatile uint32_t* const vdp_ctrl_wide = (uint32_t*) 0xC00004;

#define GFX_WRITE_VRAM_ADDR(adr)    (((0x4000 + ((adr) & 0x3FFF)) << 16) + (((adr) >> 14) | 0x00))

void vdp_set_autoinc(unsigned char value)
{
    *vdp_ctrl_port = 0x8F00 | value;
}

void put_tile_xy(uint16_t tile, uint16_t x, uint16_t y)
{
    vdp_set_autoinc(2);

    uint16_t addr = 0xC000 + (((x & 63) + ((y & 31) << 6)) * 2);

    *vdp_ctrl_wide = GFX_WRITE_VRAM_ADDR((uint32_t) addr);
    *vdp_data_port = tile - 32;
}

void print_text(const char* str, int x, int y)
{
    while (*str)
        put_tile_xy(*str++, x++, y);
}


unsigned short crsr_y;
unsigned short crsr_x;

void md_update()
{
    unsigned short command = *MD_SYS_COMM0 & 0xff00;
    unsigned short command_value = *MD_SYS_COMM0 & 0x00ff;

    if (command == SH2MD_COMMAND_SET_CURSOR)
    {
        unsigned short data = *MD_SYS_COMM2;
    
        crsr_y = (data & 0x1F);
        crsr_x = (data >> 6);

        *MD_SYS_COMM0 = 0;
    }
    else if (command == SH2MD_COMMAND_PUT_CHAR)
    {
        uint16_t character = command_value;       // Extract the character

        put_tile_xy(character, crsr_x, crsr_y);

        crsr_x += 1;                             // Increment x cursor coordinate

        *MD_SYS_COMM0 = 0;
    }

    print_text("Hello World from MD side", 0, 2);
}

int main(void)
{
    cd_ok = InitCD();
    megasd_ok = InitMegaSD();

    /*
     * Main loop in ram - you need to have it in ram to avoid bus contention
     * for the rom with the SH2s.
     */
    do_main(); // never returns

    return 0;
}
