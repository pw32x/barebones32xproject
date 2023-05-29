#include <stdint.h>
#include <string.h>

#include "../common/comm_ids.h"
#include "megasd.h"
#include "cd_init.h"
#include "font.h"

#include "data/genesis_palette.h"
#include "data/genesis_tileset.h"
#include "data/genesis_tilemap.h"
#include "data/32x_palette.h"
#include "data/32x_tileset.h"
#include "data/32x_tilemap.h"

extern void main_loop();
extern void init_main();
extern void bump_fm();
extern void enable_ints();
extern void disable_ints();
extern void chk_hotplug();

// Communication channels                                               32X SIDE
static volatile uint16_t* const MD_SYS_COMM0 = (uint16_t*) 0xA15120; // 0x20004020
static volatile uint16_t* const MD_SYS_COMM2 = (uint16_t*) 0xA15122; // 0x20004022

// MD VDP ports
static volatile uint16_t* const vdp_data_port = (uint16_t*) 0xC00000;
static volatile uint32_t* const vdp_data_wide = (uint32_t*) 0xC00000;
static volatile uint16_t* const vdp_ctrl_port = (uint16_t*) 0xC00004;
static volatile uint32_t* const vdp_ctrl_wide = (uint32_t*) 0xC00004;

#define PLANE_A_ADDR        0xC000
#define PLANE_W_ADDR        0xB000
#define PLANE_B_ADDR        0xE000
#define SPRITE_ATTR_ADDR    0xA800
#define HSCROLL_ADDR        0xAC00

#define PLANE_WIDTH     64
#define PLANE_HEIGHT    32

#define VDP_VBLANK_FLAG         (1 << 3)

#define GFX_WRITE_CRAM_ADDR(adr)    (((0xC000 + ((adr) & 0x7F)) << 16) + 0x00)
#define GFX_WRITE_VRAM_ADDR(adr)    (((0x4000 + ((adr) & 0x3FFF)) << 16) + (((adr) >> 14) | 0x00))
#define GFX_WRITE_VSRAM_ADDR(adr)   (((0x4000 + ((adr) & 0x3F)) << 16) + 0x10)


void vdp_set_autoinc(unsigned char value)
{
    *vdp_ctrl_port = 0x8F00 | value;
}

void vpd_load_palette(const unsigned short* palette, unsigned short paletteIndex)
{
    vdp_set_autoinc(2);

    *vdp_ctrl_wide = GFX_WRITE_CRAM_ADDR((uint32_t)paletteIndex * (16 * 2)); // 16 colors, 2 bytes per color

    uint32_t* paletteWide = (uint32_t*)palette;

    *vdp_data_wide = *paletteWide++;
    *vdp_data_wide = *paletteWide++;
    *vdp_data_wide = *paletteWide++;
    *vdp_data_wide = *paletteWide++;
    *vdp_data_wide = *paletteWide++;
    *vdp_data_wide = *paletteWide++;
    *vdp_data_wide = *paletteWide++;
    *vdp_data_wide = *paletteWide++;

}

unsigned int vdp_load_tileset(const unsigned char* tileset, unsigned short tilesetSize, unsigned int vdpAddress)
{
    vdp_set_autoinc(2);
    *vdp_ctrl_wide = GFX_WRITE_VRAM_ADDR((uint32_t) vdpAddress);

    uint32_t* tilesetWide = (uint32_t*)tileset;

    unsigned short tilesetSizeWide = tilesetSize >> 2;

    for (int loop = 0; loop < tilesetSizeWide; loop++)
    {
        *vdp_data_wide = *tilesetWide;
        tilesetWide++;
    }

    return vdpAddress;
}

void vdp_draw_tileset(const unsigned short* tilemap, 
                      short x, 
                      short y, 
                      unsigned short width, 
                      unsigned short height, 
                      unsigned short tilesetStart,
                      unsigned short paletteIndex,
                      unsigned short planeAddr)
{
    // no clipping
    if (x < 0)
        return;
    if (y < 0)
        return;
    if (x + width > PLANE_WIDTH - 1)
        return;
    if (y + height> PLANE_HEIGHT - 1)
        return;

    vdp_set_autoinc(2);

    for (int loopy = 0; loopy < height; loopy++)
    {
        uint16_t addr = planeAddr + (((x & 63) + (((y + loopy) & 31) << 6)) * 2);
        *vdp_ctrl_wide = GFX_WRITE_VRAM_ADDR((uint32_t) addr);

        for (int loopx = 0; loopx < width; loopx++)
        {
            *vdp_data_port = (*tilemap + (tilesetStart >> 5)) | (paletteIndex << 13);
            tilemap++;
        }
    }
}

unsigned int fg_color = 0x11111111; /* default to color 1 for fg color */
unsigned int bg_color = 0x00000000; /* default to color 0 for bg color */

void vdp_load_font()
{
    unsigned short fontTilesetSize = sizeof(font_data);
    unsigned int vdpAddress = 0; // start of vdp tile area

    vdp_set_autoinc(2);
    *vdp_ctrl_wide = GFX_WRITE_VRAM_ADDR((uint32_t) vdpAddress);

    uint32_t* fontTilesetWide = (uint32_t*)font_data;

    fontTilesetSize >>= 2;

    for (int loop = 0; loop < fontTilesetSize; loop++)
    {
        unsigned int value = *fontTilesetWide;
        unsigned int invValue = ~value;

        value &= fg_color;
        invValue &= bg_color;

        *vdp_data_wide = value | invValue;
        fontTilesetWide++;
    }
}

void put_tile_xy(uint16_t tile, uint16_t x, uint16_t y)
{
    vdp_set_autoinc(2);

    uint16_t addr = PLANE_A_ADDR + (((x & 63) + ((y & 31) << 6)) * 2);

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

void process_commands()
{
    if (!*MD_SYS_COMM0)
        return;

    unsigned short command = *MD_SYS_COMM0 & 0xff00;
    unsigned short command_value = *MD_SYS_COMM0 & 0x00ff;

    /*
    if (command)
    {
        char buf[100];
        sprintf(buf, "%04X", command);
        print_text(buf, messageX, messageY);
        messageY++;

        if (messageY == 28)
        {
            messageY = 0;
            messageX += 5;
        }

        if (messageX > 40)
        {
            while (1) {};
        }
    }
    */



    if (command == SH2MD_COMMAND_SET_CURSOR)
    {
        unsigned short data = *MD_SYS_COMM2;
    
        crsr_y = (data & 0x1F);
        crsr_x = (data >> 6);
    }
    else if (command == SH2MD_COMMAND_PUT_CHAR)
    {
        uint16_t character = command_value;       // Extract the character
    
        put_tile_xy(character, crsr_x, crsr_y);
    
        crsr_x += 1;                             // Increment x cursor coordinate
    }
}

void init_graphics()
{
    vpd_load_palette(genesis_palette, 1);
    int genesis_tileset_start = vdp_load_tileset(genesis_tileset, sizeof(genesis_tileset), sizeof(font_data));
    vdp_draw_tileset(genesis_tilemap, 0, 0, 40, 28, genesis_tileset_start, 1, PLANE_A_ADDR);

    vpd_load_palette(_32x_palette, 2);
    int _32x_tileset_start = vdp_load_tileset(_32x_tileset, sizeof(_32x_tileset), genesis_tileset_start + sizeof(genesis_tileset));
    vdp_draw_tileset(_32x_tilemap, 0, 0, 40, 28, _32x_tileset_start, 2, PLANE_B_ADDR);
}



void vdp_wait_vsync()
{
    while (*vdp_ctrl_port & VDP_VBLANK_FLAG);
    while (!(*vdp_ctrl_port & VDP_VBLANK_FLAG));
}

void vdp_set_horizontal_scroll(unsigned short planeAddr, short scrollAmount)
{
    unsigned short vdpDestinationAddress = HSCROLL_ADDR;
    if (planeAddr == PLANE_B_ADDR) 
        vdpDestinationAddress += 2;

    *vdp_ctrl_wide = GFX_WRITE_VRAM_ADDR(vdpDestinationAddress);
    *vdp_data_port = scrollAmount;
}

void vdp_set_vertical_scroll(unsigned short planeAddr, short scrollAmount)
{
    unsigned short destinationAddress = (planeAddr == PLANE_B_ADDR) ? 2 : 0;

    *vdp_ctrl_wide = GFX_WRITE_VSRAM_ADDR((unsigned int) destinationAddress);
    *vdp_data_port = scrollAmount;
}

int planeAScrollX = 0;
int planeAScrollY = 0;
int planeBScrollX = 0;
int planeBScrollY = 0;

int planeAScrollXDirection = 1;
int planeAScrollYDirection = 1;
int planeBScrollXDirection = -1;
int planeBScrollYDirection = -1;


void update_scene()
{
    planeBScrollX += planeBScrollXDirection;
    if (planeBScrollX > 100 || planeBScrollX < 0)
        planeBScrollXDirection = -planeBScrollXDirection;

    planeAScrollY += planeAScrollYDirection;
    if (planeAScrollY > 100 || planeAScrollY < 0)
        planeAScrollYDirection = -planeAScrollYDirection;

    vdp_set_vertical_scroll(PLANE_A_ADDR, planeAScrollY);
    vdp_set_horizontal_scroll(PLANE_B_ADDR, planeBScrollX);
    
}

int main(void)
{
    cd_ok = InitCD();
    megasd_ok = InitMegaSD();

    /*
     * Main loop in ram - you need to have it in ram to avoid bus contention
     * for the rom with the SH2s.
     */

    init_main();
    init_graphics();

    print_text("Hello World from MD side", 0, 2);

    while (1)
    {
        // disabling and enabling ints screws up the MD_SYS_COMM0 messages in process_commands
        // for some reason. it doesn't occur in assembly.
        //disable_ints();
        //bump_fm();
        //enable_ints();

        process_commands();
        *MD_SYS_COMM0 = 0;

        chk_hotplug();

        update_scene();

        vdp_wait_vsync();
    }

    return 0;
}
