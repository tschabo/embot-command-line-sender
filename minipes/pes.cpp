/*
 * PES file parsing.
 *
 * All format credit goes to Robert Heel and 
 *
 *	https://bugs.launchpad.net/inkscape/+bug/247463
 *
 * which has a php script to do so. He in turn seems to have
 * gotten it from NJ Crawford's C# PES viewer. Linus just turned
 * it into C. And I turned some parts into C++.
 */
#include <string.h>

#include <fstream>
#include <iterator>

#include "pes.h"

static color color_def[256] = {
    {"NULL", 0, 0, 0},
    {"Color1", 14, 31, 124},
    {"Color2", 10, 85, 163},
    {"Color3", 48, 135, 119},
    {"Color4", 75, 107, 175},
    {"Color5", 237, 23, 31},
    {"Color6", 209, 92, 0},
    {"Color7", 145, 54, 151},
    {"Color8", 228, 154, 203},
    {"Color9", 145, 95, 172},
    {"Color10", 157, 214, 125},
    {"Color11", 232, 169, 0},
    {"Color12", 254, 186, 53},
    {"Color13", 255, 255, 0},
    {"Color14", 112, 188, 31},
    {"Color15", 192, 148, 0},
    {"Color16", 168, 168, 168},
    {"Color17", 123, 111, 0},
    {"Color18", 255, 255, 179},
    {"Color19", 79, 85, 86},
    {"Black", 0, 0, 0},
    {"Color21", 11, 61, 145},
    {"Color22", 119, 1, 118},
    {"Color23", 41, 49, 51},
    {"Color24", 42, 19, 1},
    {"Color25", 246, 74, 138},
    {"Color26", 178, 118, 36},
    {"Color27", 252, 187, 196},
    {"Color28", 254, 55, 15},
    {"White", 240, 240, 240},
    {"Color30", 106, 28, 138},
    {"Color31", 168, 221, 196},
    {"Color32", 37, 132, 187},
    {"Color33", 254, 179, 67},
    {"Color34", 255, 240, 141},
    {"Color35", 208, 166, 96},
    {"Color36", 209, 84, 0},
    {"Color37", 102, 186, 73},
    {"Color38", 19, 74, 70},
    {"Color39", 135, 135, 135},
    {"Color40", 216, 202, 198},
    {"Color41", 67, 86, 7},
    {"Color42", 254, 227, 197},
    {"Color43", 249, 147, 188},
    {"Color44", 0, 56, 34},
    {"Color45", 178, 175, 212},
    {"Color46", 104, 106, 176},
    {"Color47", 239, 227, 185},
    {"Color48", 247, 56, 102},
    {"Color49", 181, 76, 100},
    {"Color50", 19, 43, 26},
    {"Color51", 199, 1, 85},
    {"Color52", 254, 158, 50},
    {"Color53", 168, 222, 235},
    {"Color54", 0, 103, 26},
    {"Color55", 78, 41, 144},
    {"Color56", 47, 126, 32},
    {"Color57", 253, 217, 222},
    {"Color58", 255, 217, 17},
    {"Color59", 9, 91, 166},
    {"Color60", 240, 249, 112},
    {"Color61", 227, 243, 91},
    {"Color62", 255, 200, 100},
    {"Color63", 255, 200, 150},
    {"Color64", 255, 200, 200},
};

std::vector<unsigned char> read_file(const std::filesystem::path& path)
{
    std::ifstream input( path, std::ios::binary );
    auto buff = std::vector<unsigned char>(std::istreambuf_iterator<char>(input), {});
    for(int i = 0; i<8;i++)
    {
        buff.push_back(0);
    }
    return buff;
}

#define get_u8(buf, offset) (*(unsigned char *)((offset) + (const char *)(buf)))
#define get_le32(buf, offset) (*(unsigned int *)((offset) + (const char *)(buf)))

static std::vector<std::reference_wrapper<color>> parse_pes_colors(const std::vector<unsigned char> fileBuffer, unsigned int pec)
{
    const void *buf = fileBuffer.data();
    int nr_colors = get_u8(buf, pec + 48) + 1;
    int i;
    std::vector<std::reference_wrapper<color>> colors;
    for (i = 0; i < nr_colors; i++)
    {
        colors.push_back(color_def[get_u8(buf, pec + 49 + i)]);
    }
    return colors;
}

static int parse_pes_stitches(const std::vector<unsigned char>& fileBuffer, unsigned int pec, pes& pes)
{
    int oldx, oldy;
    const unsigned char *buf = fileBuffer.data(), *p, *end;

    p = buf + pec + 532;
    end = buf + fileBuffer.size()-8;

    oldx = oldy = 0;

    int color_idx{0};

    pes.blocks.push_back({pes.colors[color_idx++]});

    while (p < end)
    {
        int val1 = p[0], val2 = p[1], jumpstitch = 0;
        p += 2;
        if (val1 == 255 && !val2)
            return 0;
        if (val1 == 254 && val2 == 176)
        {
            if (pes.blocks.back().stitches.size())
            {
                pes.blocks.push_back({pes.colors[color_idx++]});
            }
            p++; /* Skip byte */
            continue;
        }

        /* High bit set means 12-bit offset, otherwise 7-bit signed delta */
        if (val1 & 0x80)
        {
            val1 = ((val1 & 15) << 8) + val2;
            /* Signed 12-bit arithmetic */
            if (val1 & 2048)
                val1 -= 4096;
            val2 = *p++;
            jumpstitch = 1;
        }
        else
        {
            if (val1 & 64)
                val1 -= 128;
        }

        if (val2 & 0x80)
        {
            val2 = ((val2 & 15) << 8) + *p++;
            /* Signed 12-bit arithmetic */
            if (val2 & 2048)
                val2 -= 4096;
            jumpstitch = 1;
        }
        else
        {
            if (val2 & 64)
                val2 -= 128;
        }

        val1 += oldx;
        val2 += oldy;

        oldx = val1;
        oldy = val2;

        if (val1 < pes.min_x)
            pes.min_x = val1;
        if (val1 > pes.max_x)
            pes.max_x = val1;
        if (val2 < pes.min_y)
            pes.min_y = val2;
        if (val2 > pes.max_y)
            pes.max_y = val2;

        pes.blocks.back().stitches.push_back({val1,val2,jumpstitch});
    }
    return 0;
}

pes parse_pes(const std::vector<unsigned char>& pesBin)
{
    const void *buf = pesBin.data();
    const unsigned int size = pesBin.size();
    unsigned int pec{};

    pes thePes{};

    if (size < 48)
        throw "File to small";
    if (memcmp(buf, "#PES", 4))
        throw "Not a pes file";
    pec = get_le32(buf, 8);
    if (pec + 532 >= size)
        throw "File to small";
    thePes.colors = parse_pes_colors(pesBin, pec);
    parse_pes_stitches(pesBin, pec, thePes);
    return thePes;
}

