#define PPUCTRL                         0x2000
#define PPUMASK                         0x2001
#define PPUSTATUS                       0x2002
#define PPUOAMADDR                      0x2003
#define PPUOAMDATA                      0x2004
#define PPUSCROLL                       0x2005
#define PPUADDR                         0x2006
#define PPUDATA                         0x2007
#define PPUCTRL_NAMETABLE               (memory[PPUCTRL] & 0x00)
#define PPUCTRL_INCREMENT               (memory[PPUCTRL] & 0x04)
#define PPUCTRL_SPRITEHI                (memory[PPUCTRL] & 0x08)
#define PPUCTRL_BACKGROUNDHI            (memory[PPUCTRL] & 0x10)
#define PPUCTRL_SPRITE16                (memory[PPUCTRL] & 0x20)
#define PPUCTRL_VBLANKNMI               (memory[PPUCTRL] & 0x80)
#define PPUMASK_MONOCHROME              (memory[PPUMASK] & 0x01)
#define PPUMASK_BACKGROUNDCLIP          (memory[PPUMASK] & 0x02)
#define PPUMASK_SPRITECLIP              (memory[PPUMASK] & 0x04)
#define PPUMASK_BACKGROUNDSHOW          (memory[PPUMASK] & 0x08)
#define PPUMASK_SPRITESHOW              (memory[PPUMASK] & 0x10)
#define PPUSTATUS_VRAMWRITEFLAG         (memory[PPUSTATUS] & 0x10)
#define PPUSTATUS_SPRITEOVERFLOW        (memory[PPUSTATUS] & 0x20)
#define PPUSTATUS_SPRITE0HIT            (memory[PPUSTATUS] & 0x40)
#define PPUSTATUS_VBLANKON              (memory[PPUSTATUS] & 0x80)

static unsigned char ppu_memory[16384];
static unsigned int ppu_addr_latch = 0;
static unsigned int ppu_addr = 0x2000;
static unsigned int ppu_lastdata = 0;
static unsigned int ppu_scroll_latch = 0;
static unsigned int ppu_background_loopyT = 0x00;
static unsigned int ppu_background_loopyV = 0x00;
static unsigned int ppu_background_loopyX = 0x00;
static unsigned char ppu_background_cache[256 + 8][256 + 8];
static unsigned int ppu_sprite_addr = 0x00;
static unsigned char ppu_sprite_memory[256];
static unsigned char ppu_sprite_cache[256 + 8][256 + 8];

static unsigned char ppu_memread(unsigned int address)
{

    if (address == PPUSTATUS)
    {

        unsigned int old = memory[PPUSTATUS];

        memory[PPUSTATUS] &= 0x7F;
        ppu_scroll_latch = 0;
        ppu_addr_latch = 0;

        return (old & 0xE0) | (ppu_lastdata & 0x1F);

    }

    if (address == PPUDATA)
    {

        unsigned int old = ppu_lastdata;

        ppu_lastdata = ppu_addr;
        ppu_addr += (PPUCTRL_INCREMENT) ? 32 : 1;

        return ppu_memory[old];

    }

    return memory[address];

}

static unsigned char ppu_memwrite(unsigned int address, unsigned char data)
{

    ppu_lastdata = data;

    if (address == PPUCTRL)
    {

        ppu_background_loopyT &= 0xf3ff;
        ppu_background_loopyT |= (data & 3) << 10;

        return data;

    }

    if (address == PPUOAMADDR)
    {

        ppu_sprite_addr = data;

        return data;

    }

    if (address == PPUOAMDATA)
    {

        ppu_sprite_memory[ppu_sprite_addr] = data;
        ppu_sprite_addr++;

        return data;

    }

    if (address == PPUSCROLL)
    {

        if (ppu_scroll_latch)
        {

            ppu_background_loopyT &= 0xFC1F;
            ppu_background_loopyT |= (data & 0xF8) << 2;
            ppu_background_loopyT &= 0x8FFF;
            ppu_background_loopyT |= (data & 0x07) << 12;
            ppu_scroll_latch = 0;

        }

        else
        {

            ppu_background_loopyT &= 0xFFE0;
            ppu_background_loopyT |= (data & 0xF8) >> 3;
            ppu_background_loopyX = data & 0x07;
            ppu_scroll_latch = 1;

        }

        return data;

    }

    if (address == PPUADDR)
    {

        if (ppu_addr_latch)
        {

            ppu_addr |= data;
            ppu_background_loopyT &= 0xFF00;
            ppu_background_loopyT |= data;
            ppu_background_loopyV = ppu_background_loopyT;
            ppu_addr_latch = 0;

        }

        else
        {

            ppu_addr = (data << 8);
            ppu_background_loopyT &= 0x00FF;
            ppu_background_loopyT |= (data & 0x3F);
            ppu_addr_latch = 1;

        }

        return data;

    }

    if (address == PPUDATA)
    {

        if (PPUSTATUS_VRAMWRITEFLAG)
            return data;

        ppu_memory[ppu_addr] = data;

        if ((ppu_addr > 0x1999) && (ppu_addr < 0x3000))
        {

            if (OS_MIRROR == 1)
            {

                ppu_memory[ppu_addr + 0x400] = data;
                ppu_memory[ppu_addr + 0x800] = data;
                ppu_memory[ppu_addr + 0x1200] = data;

            }

            else if (FS_MIRROR == 1)
            {

            }

            else
            {

                if (MIRRORING == 0)
                    ppu_memory[ppu_addr + 0x400] = data;
                else
                    ppu_memory[ppu_addr + 0x800] = data;

            }

        }

        if (ppu_addr == 0x3f10)
            ppu_memory[0x3f00] = data;

        ppu_addr += (PPUCTRL_INCREMENT) ? 32 : 1;

        return data;

    }

    if (address == 0x4014)
    {

        unsigned int i;

        for (i = 0; i < 256; i++)
            ppu_sprite_memory[i] = memory[0x100 * data + i];

        return data;

    }

    return data;

}

static void ppu_checkspritehit(int width, int scanline)
{

    int i;

    for (i = 0; i < width; i++)
    {

        if ((ppu_background_cache[i][scanline - 1] > 0x00) && (ppu_sprite_cache[i][scanline - 1] > 0x00))
            memory[PPUSTATUS] |= 0x40;

    }

}

static void ppu_renderbackground(int scanline)
{

    int tile_count;
    int i;
    int nt_addr;
    int at_addr;
    int x_scroll;
    int y_scroll;
    int attribs;
    unsigned char tile[8];

    ppu_background_loopyV &= 0xfbe0;
    ppu_background_loopyV |= (ppu_background_loopyT & 0x041f);
    x_scroll = (ppu_background_loopyV & 0x1f);
    y_scroll = (ppu_background_loopyV & 0x03e0) >> 5;
    nt_addr = 0x2000 + (ppu_background_loopyV & 0x0fff);
    at_addr = 0x2000 + (ppu_background_loopyV & 0x0c00) + 0x03c0 + ((y_scroll & 0xfffc) << 1) + (x_scroll >> 2);

    if ((y_scroll & 0x0002) == 0)
    {

        if ((x_scroll & 0x0002) == 0)
            attribs = (ppu_memory[at_addr] & 0x03) << 2;
        else
            attribs = (ppu_memory[at_addr] & 0x0C);

    }

    else
    {

        if ((x_scroll & 0x0002) == 0)
            attribs = (ppu_memory[at_addr] & 0x30) >> 2;
        else
            attribs = (ppu_memory[at_addr] & 0xC0) >> 4;

    }

    for (tile_count = 0; tile_count < 33; tile_count++)
    {

        int ttc = tile_count << 3;
        int pt_addr = (ppu_memory[nt_addr] << 4) + ((ppu_background_loopyV & 0x7000) >> 12);
        char a1, a2;

        if (PPUCTRL_BACKGROUNDHI)
            pt_addr += 0x1000;

        a1 = ppu_memory[pt_addr];
        a2 = ppu_memory[pt_addr + 8];

        for (i = 0; i < 8; i++)
        {

            tile[i] = 0;

            if (a1 & (0x80 >> i))
                tile[i] += 1;

            if (a2 & (0x80 >> i))
                tile[i] += 2;

            if (tile[i])
                tile[i] += attribs;

        }

        if ((tile_count == 0) && (ppu_background_loopyX != 0))
        {

            for (i = 0; i < 8 - ppu_background_loopyX; i++)
            {

                ppu_background_cache[ttc + i][scanline] = tile[ppu_background_loopyX + i];

                if (PPUMASK_BACKGROUNDSHOW)
                    backend_drawpixel(ttc + i, scanline, ppu_memory[0x3f00 + (tile[ppu_background_loopyX + i])]);

            }

        }

        else if ((tile_count == 32) && (ppu_background_loopyX != 0))
        {

            for (i = 0; i < ppu_background_loopyX; i++)
            {

                ppu_background_cache[ttc + i - ppu_background_loopyX][scanline] = tile[i];

                if (PPUMASK_BACKGROUNDSHOW)
                    backend_drawpixel(ttc + i - ppu_background_loopyX, scanline, ppu_memory[0x3f00 + (tile[i])]);

            }

        }

        else
        {

            for (i = 0; i < 8; i++)
            {

                ppu_background_cache[ttc + i - ppu_background_loopyX][scanline] = tile[i];

                if (PPUMASK_BACKGROUNDSHOW)
                    backend_drawpixel(ttc + i - ppu_background_loopyX, scanline, ppu_memory[0x3f00 + (tile[i])]);

            }

        }

        nt_addr++;
        x_scroll++;

        if ((x_scroll & 0x0001) == 0)
        {

            if ((x_scroll & 0x0003) == 0)
            {

                if ((x_scroll & 0x1F) == 0)
                {

                    nt_addr ^= 0x0400;
                    at_addr ^= 0x0400;
                    nt_addr -= 0x0020;
                    at_addr -= 0x0008;
                    x_scroll -= 0x0020;

                }

                at_addr++;

            }

            if ((y_scroll & 0x0002) == 0)
            {

                if ((x_scroll & 0x0002) == 0)
                    attribs = ((ppu_memory[at_addr]) & 0x03) << 2;
                else
                    attribs = ((ppu_memory[at_addr]) & 0x0C);

            }

            else
            {

                if ((x_scroll & 0x0002) == 0)
                    attribs = ((ppu_memory[at_addr]) & 0x30) >> 2;
                else
                    attribs = ((ppu_memory[at_addr]) & 0xC0) >> 4;

            }

        }

    }

    if ((ppu_background_loopyV & 0x7000) == 0x7000)
    {

        ppu_background_loopyV &= 0x8fff;

        if ((ppu_background_loopyV & 0x03e0) == 0x03a0)
        {

            ppu_background_loopyV ^= 0x0800;
            ppu_background_loopyV &= 0xfc1f;

        }

        else
        {

            if ((ppu_background_loopyV & 0x03e0) == 0x03e0)
                ppu_background_loopyV &= 0xfc1f;
            else
                ppu_background_loopyV += 0x0020;

        }

    }

    else
    {

        ppu_background_loopyV += 0x1000;

    }

}

static void ppu_rendersprite(int y, int x, int pattern_number, int attribs, int spr_nr)
{

    int disp_spr_back = attribs & 0x20;
    int flip_spr_hor = attribs & 0x40;
    int flip_spr_ver = attribs & 0x80;
    int i, imax;
    int j, jmax;
    int sprite_start;
    int sprite_pattern_table;
    unsigned char bit1[8][16];
    unsigned char bit2[8][16];
    unsigned char sprite[8][16];

    sprite_pattern_table = (!PPUCTRL_SPRITE16 && PPUCTRL_SPRITEHI) ? 0x1000 : 0x0000;
    sprite_start = sprite_pattern_table + ((pattern_number << 3) << 1);
    imax = 8;
    jmax = (PPUCTRL_SPRITE16) ? 16 : 8;

    if ((!flip_spr_hor) && (!flip_spr_ver))
    {

        for (i = imax - 1; i >= 0; i--)
        {

            for (j = 0; j < jmax; j++)
            {

                bit1[(imax - 1) - i][j] = ((ppu_memory[sprite_start + j] >> i) & 0x01) ? 1 : 0;
                bit2[(imax - 1) - i][j] = ((ppu_memory[sprite_start + 8 + j] >> i) & 0x01) ? 1 : 0;

            }

        }

    }

    else if ((flip_spr_hor) && (!flip_spr_ver))
    {

        for (i = 0; i < imax; i++)
        {

            for (j = 0; j < jmax; j++)
            {

                bit1[i][j] = ((ppu_memory[sprite_start + j] >> i) & 0x01) ? 1 : 0;
                bit2[i][j] = ((ppu_memory[sprite_start + 8 + j] >> i) & 0x01) ? 1 : 0;

            }

        }

    }

    else if ((!flip_spr_hor) && (flip_spr_ver))
    {

        for (i = imax - 1; i >= 0; i--)
        {

            for (j = jmax - 1; j >= 0; j--)
            {

                bit1[(imax - 1) - i][(jmax - 1) - j] = ((ppu_memory[sprite_start + j] >> i) & 0x01) ? 1 : 0;
                bit2[(imax - 1) - i][(jmax - 1) - j] = ((ppu_memory[sprite_start + 8 + j] >> i) & 0x01) ? 1 : 0;

            }

        }

    }

    else if ((flip_spr_hor) && (flip_spr_ver))
    {

        for (i = 0; i < imax; i++)
        {

            for (j = (jmax - 1); j >= 0; j--)
            {

                bit1[i][(jmax - 1) - j] = ((ppu_memory[sprite_start + j] >> i) & 0x01) ? 1 : 0;
                bit2[i][(jmax - 1) - j] = ((ppu_memory[sprite_start + 8 + j] >> i) & 0x01) ? 1 : 0;

            }

        }

    }

    for (i = 0; i < imax; i++)
    {

        for (j = 0; j < jmax; j++)
        {

            if ((bit1[i][j] == 0) && (bit2[i][j] == 0))
                sprite[i][j] = 0;
            else if ((bit1[i][j] == 1) && (bit2[i][j] == 0))
                sprite[i][j] = 1 + ((attribs & 0x03) << 0x02);
            else if ((bit1[i][j] == 0) && (bit2[i][j] == 1))
                sprite[i][j] = 2 + ((attribs & 0x03) << 0x02);
            else if ((bit1[i][j] == 1) && (bit2[i][j] == 1))
                sprite[i][j] = 3 + ((attribs & 0x03) << 0x02);

            if (spr_nr == 0)
                ppu_sprite_cache[x + i][y + j] = sprite[i][j];

            if (sprite[i][j] != 0)
            {

                if (!disp_spr_back)
                {

                    if (PPUMASK_BACKGROUNDSHOW)
                        backend_drawpixel(x + i, y + j, ppu_memory[0x3f10 + (sprite[i][j])]);

                }

                else
                {

                    if (ppu_background_cache[x + i][y + j] == 0)
                        if (PPUMASK_BACKGROUNDSHOW)
                            backend_drawpixel(x + i, y + j, ppu_memory[0x3f10 + (sprite[i][j])]);

                }

            }

        }

    }

}

static void ppu_rendersprites()
{

    int i = 0;

    memset(ppu_sprite_cache, 0x00, sizeof (ppu_sprite_cache));

    for (i = 63; i >= 0; i--)
        ppu_rendersprite(ppu_sprite_memory[i * 4], ppu_sprite_memory[i * 4 + 3], ppu_sprite_memory[i * 4 + 1], ppu_sprite_memory[i * 4 + 2], i);

}

