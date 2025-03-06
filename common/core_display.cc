/*****************************************************************************
 * Plus42 -- an enhanced HP-42S calculator simulator
 * Copyright (C) 2004-2025  Thomas Okken
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see http://www.gnu.org/licenses/.
 *****************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <set>

#include "core_display.h"
#include "core_commands2.h"
#include "core_commands8.h"
#include "core_equations.h"
#include "core_globals.h"
#include "core_helpers.h"
#include "core_main.h"
#include "core_parser.h"
#include "core_tables.h"
#include "core_variables.h"
#include "shell.h"
#include "shell_spool.h"


/********************/
/* HP-42S font data */
/********************/

#if defined(WINDOWS) && !defined(__GNUC__)
/* Disable warnings:
 * C4838: conversion from 'int' to 'const char' requires a narrowing conversion
 */
#pragma warning(push)
#pragma warning(disable:4838)
#endif


static const char bigchars[138][5] =
    {
        { 0x08, 0x08, 0x2a, 0x08, 0x08 },
        { 0x22, 0x14, 0x08, 0x14, 0x22 },
        { 0x10, 0x20, 0x7f, 0x01, 0x01 },
        { 0x20, 0x40, 0x3e, 0x01, 0x02 },
        { 0x55, 0x2a, 0x55, 0x2a, 0x55 },
        { 0x41, 0x63, 0x55, 0x49, 0x63 },
        { 0x7f, 0x7f, 0x3e, 0x1c, 0x08 },
        { 0x04, 0x7c, 0x04, 0x7c, 0x04 },
        { 0x30, 0x48, 0x45, 0x40, 0x20 },
        { 0x50, 0x58, 0x54, 0x52, 0x51 },
        { 0x0f, 0x08, 0x00, 0x78, 0x28 },
        { 0x51, 0x52, 0x54, 0x58, 0x50 },
        { 0x14, 0x34, 0x1c, 0x16, 0x14 },
        { 0x20, 0x70, 0xa8, 0x20, 0x3f },
        { 0x10, 0x20, 0x7f, 0x20, 0x10 },
        { 0x08, 0x08, 0x2a, 0x1c, 0x08 },
        { 0x08, 0x1c, 0x2a, 0x08, 0x08 },
        { 0x7e, 0x20, 0x20, 0x1e, 0x20 },
        { 0x48, 0x7e, 0x49, 0x41, 0x02 },
        { 0x00, 0x0e, 0x0a, 0x0e, 0x00 },
        { 0x78, 0x16, 0x15, 0x16, 0x78 },
        { 0x7c, 0x0a, 0x11, 0x22, 0x7d },
        { 0x7c, 0x13, 0x12, 0x13, 0x7c },
        { 0x60, 0x50, 0x58, 0x64, 0x42 },
        { 0x3e, 0x2a, 0x2a, 0x22, 0x00 },
        { 0x7e, 0x09, 0x7f, 0x49, 0x41 },
        { 0x60, 0x00, 0x60, 0x00, 0x60 },
        { 0x1f, 0x15, 0x71, 0x50, 0x50 },
        { 0x3c, 0x43, 0x42, 0x43, 0x3c },
        { 0x3c, 0x41, 0x40, 0x41, 0x3c },
        { 0x04, 0x02, 0x01, 0x02, 0x04 },
        { 0x3c, 0x3c, 0x3c, 0x3c, 0x3c },
        { 0x00, 0x00, 0x00, 0x00, 0x00 },
        { 0x00, 0x00, 0x5f, 0x00, 0x00 },
        { 0x00, 0x07, 0x00, 0x07, 0x00 },
        { 0x14, 0x7f, 0x14, 0x7f, 0x14 },
        { 0x24, 0x2a, 0x7f, 0x2a, 0x12 },
        { 0x23, 0x13, 0x08, 0x64, 0x62 },
        { 0x36, 0x49, 0x56, 0x20, 0x50 },
        { 0x00, 0x00, 0x07, 0x00, 0x00 },
        { 0x00, 0x1c, 0x22, 0x41, 0x00 },
        { 0x00, 0x41, 0x22, 0x1c, 0x00 },
        { 0x08, 0x2a, 0x1c, 0x2a, 0x08 },
        { 0x08, 0x08, 0x3e, 0x08, 0x08 },
        { 0x00, 0xb0, 0x70, 0x00, 0x00 },
        { 0x08, 0x08, 0x08, 0x08, 0x00 },
        { 0x00, 0x60, 0x60, 0x00, 0x00 },
        { 0x20, 0x10, 0x08, 0x04, 0x02 },
        { 0x3e, 0x51, 0x49, 0x45, 0x3e },
        { 0x00, 0x42, 0x7f, 0x40, 0x00 },
        { 0x62, 0x51, 0x49, 0x49, 0x46 },
        { 0x22, 0x49, 0x49, 0x49, 0x36 },
        { 0x18, 0x14, 0x12, 0x7f, 0x10 },
        { 0x27, 0x45, 0x45, 0x45, 0x39 },
        { 0x3c, 0x4a, 0x49, 0x49, 0x30 },
        { 0x01, 0x71, 0x09, 0x05, 0x03 },
        { 0x36, 0x49, 0x49, 0x49, 0x36 },
        { 0x06, 0x49, 0x49, 0x29, 0x1e },
        { 0x00, 0x36, 0x36, 0x00, 0x00 },
        { 0x00, 0xb6, 0x76, 0x00, 0x00 },
        { 0x08, 0x14, 0x22, 0x41, 0x00 },
        { 0x14, 0x14, 0x14, 0x14, 0x14 },
        { 0x41, 0x22, 0x14, 0x08, 0x00 },
        { 0x02, 0x01, 0x51, 0x09, 0x06 },
        { 0x3e, 0x41, 0x5d, 0x55, 0x5e },
        { 0x7e, 0x09, 0x09, 0x09, 0x7e },
        { 0x7f, 0x49, 0x49, 0x49, 0x36 },
        { 0x3e, 0x41, 0x41, 0x41, 0x22 },
        { 0x7f, 0x41, 0x41, 0x22, 0x1c },
        { 0x7f, 0x49, 0x49, 0x49, 0x41 },
        { 0x7f, 0x09, 0x09, 0x09, 0x01 },
        { 0x3e, 0x41, 0x41, 0x51, 0x72 },
        { 0x7f, 0x08, 0x08, 0x08, 0x7f },
        { 0x00, 0x41, 0x7f, 0x41, 0x00 },
        { 0x30, 0x40, 0x40, 0x40, 0x3f },
        { 0x7f, 0x08, 0x14, 0x22, 0x41 },
        { 0x7f, 0x40, 0x40, 0x40, 0x40 },
        { 0x7f, 0x02, 0x0c, 0x02, 0x7f },
        { 0x7f, 0x04, 0x08, 0x10, 0x7f },
        { 0x3e, 0x41, 0x41, 0x41, 0x3e },
        { 0x7f, 0x09, 0x09, 0x09, 0x06 },
        { 0x3e, 0x41, 0x51, 0x21, 0x5e },
        { 0x7f, 0x09, 0x19, 0x29, 0x46 },
        { 0x26, 0x49, 0x49, 0x49, 0x32 },
        { 0x01, 0x01, 0x7f, 0x01, 0x01 },
        { 0x3f, 0x40, 0x40, 0x40, 0x3f },
        { 0x07, 0x18, 0x60, 0x18, 0x07 },
        { 0x7f, 0x20, 0x18, 0x20, 0x7f },
        { 0x63, 0x14, 0x08, 0x14, 0x63 },
        { 0x03, 0x04, 0x78, 0x04, 0x03 },
        { 0x61, 0x51, 0x49, 0x45, 0x43 },
        { 0x00, 0x7f, 0x41, 0x41, 0x00 },
        { 0x02, 0x04, 0x08, 0x10, 0x20 },
        { 0x00, 0x41, 0x41, 0x7f, 0x00 },
        { 0x04, 0x02, 0x7f, 0x02, 0x04 },
        { 0x80, 0x80, 0x80, 0x80, 0x80 },
        { 0x00, 0x03, 0x04, 0x00, 0x00 },
        { 0x20, 0x54, 0x54, 0x54, 0x78 },
        { 0x7f, 0x44, 0x44, 0x44, 0x38 },
        { 0x38, 0x44, 0x44, 0x44, 0x44 },
        { 0x38, 0x44, 0x44, 0x44, 0x7f },
        { 0x38, 0x54, 0x54, 0x54, 0x58 },
        { 0x00, 0x08, 0x7e, 0x09, 0x02 },
        { 0x18, 0xa4, 0xa4, 0xa4, 0x78 },
        { 0x7f, 0x04, 0x04, 0x04, 0x78 },
        { 0x00, 0x44, 0x7d, 0x40, 0x00 },
        { 0x00, 0x40, 0x80, 0x84, 0x7d },
        { 0x7f, 0x10, 0x28, 0x44, 0x00 },
        { 0x00, 0x41, 0x7f, 0x40, 0x00 },
        { 0x7c, 0x04, 0x38, 0x04, 0x7c },
        { 0x7c, 0x04, 0x04, 0x04, 0x78 },
        { 0x38, 0x44, 0x44, 0x44, 0x38 },
        { 0xfc, 0x24, 0x24, 0x24, 0x18 },
        { 0x18, 0x24, 0x24, 0x24, 0xfc },
        { 0x7c, 0x08, 0x04, 0x04, 0x04 },
        { 0x48, 0x54, 0x54, 0x54, 0x24 },
        { 0x00, 0x04, 0x3f, 0x44, 0x20 },
        { 0x3c, 0x40, 0x40, 0x40, 0x7c },
        { 0x1c, 0x20, 0x40, 0x20, 0x1c },
        { 0x3c, 0x40, 0x30, 0x40, 0x3c },
        { 0x44, 0x28, 0x10, 0x28, 0x44 },
        { 0x1c, 0xa0, 0xa0, 0xa0, 0x7c },
        { 0x44, 0x64, 0x54, 0x4c, 0x44 },
        { 0x08, 0x36, 0x41, 0x41, 0x00 },
        { 0x00, 0x00, 0x7f, 0x00, 0x00 },
        { 0x00, 0x41, 0x41, 0x36, 0x08 },
        { 0x08, 0x04, 0x08, 0x10, 0x08 },
        { 0x7f, 0x08, 0x08, 0x08, 0x08 },
        { 0x28, 0x00, 0x00, 0x00, 0x00 },
        { 0x04, 0x08, 0x70, 0x08, 0x04 },
        { 0x5e, 0x61, 0x01, 0x61, 0x5e },
        { 0x04, 0x04, 0x7c, 0x04, 0x04 },
        { 0x7c, 0x40, 0x40, 0x40, 0x40 },
        { 0x78, 0x14, 0x14, 0x14, 0x78 },
        { 0x7f, 0x41, 0x22, 0x14, 0x08 },
        { 0x2a, 0x55, 0x2a, 0x14, 0x08 },
        { 0x08, 0x14, 0x2a, 0x14, 0x22 },
        { 0x22, 0x14, 0x2a, 0x14, 0x08 },
    };

static const char smallchars[454] =
    {
        0x00, 0x00, 0x00,
        0x5c,
        0x06, 0x00, 0x06,
        0x28, 0x7c, 0x28, 0x7c, 0x28,
        0x08, 0x54, 0x7c, 0x54, 0x20,
        0x24, 0x10, 0x48,
        0x30, 0x4c, 0x50, 0x20, 0x50,
        0x08, 0x04,
        0x38, 0x44,
        0x44, 0x38,
        0x54, 0x38, 0x54,
        0x10, 0x38, 0x10,
        0x40, 0x20,
        0x10, 0x10, 0x10,
        0x40,
        0x60, 0x10, 0x0c,
        0x38, 0x44, 0x38,
        0x48, 0x7c, 0x40,
        0x74, 0x54, 0x5c,
        0x44, 0x54, 0x7c,
        0x1c, 0x10, 0x7c,
        0x5c, 0x54, 0x74,
        0x7c, 0x54, 0x74,
        0x64, 0x14, 0x0c,
        0x7c, 0x54, 0x7c,
        0x5c, 0x54, 0x7c,
        0x28,
        0x40, 0x28,
        0x10, 0x28, 0x44,
        0x28, 0x28, 0x28,
        0x44, 0x28, 0x10,
        0x08, 0x04, 0x54, 0x08,
        0x38, 0x44, 0x54, 0x58,
        0x78, 0x14, 0x78,
        0x7c, 0x54, 0x28,
        0x38, 0x44, 0x44,
        0x7c, 0x44, 0x38,
        0x7c, 0x54, 0x44,
        0x7c, 0x14, 0x04,
        0x7c, 0x44, 0x54, 0x74,
        0x7c, 0x10, 0x7c,
        0x7c,
        0x60, 0x40, 0x7c,
        0x7c, 0x10, 0x28, 0x44,
        0x7c, 0x40, 0x40,
        0x7c, 0x08, 0x10, 0x08, 0x7c,
        0x7c, 0x18, 0x30, 0x7c,
        0x7c, 0x44, 0x7c,
        0x7c, 0x14, 0x1c,
        0x38, 0x44, 0x24, 0x58,
        0x7c, 0x14, 0x6c,
        0x48, 0x54, 0x24,
        0x04, 0x7c, 0x04,
        0x7c, 0x40, 0x7c,
        0x1c, 0x60, 0x1c,
        0x7c, 0x20, 0x10, 0x20, 0x7c,
        0x6c, 0x10, 0x6c,
        0x0c, 0x70, 0x0c,
        0x64, 0x54, 0x4c,
        0x7c, 0x44,
        0x0c, 0x10, 0x60,
        0x44, 0x7c,
        0x10, 0x08, 0x7c, 0x08, 0x10,
        0x40, 0x40, 0x40,
        0x04, 0x08,
        0x10, 0x6c, 0x44,
        0x6c,
        0x44, 0x6c, 0x10,
        0x10, 0x08, 0x10, 0x20, 0x10,
        0x54, 0x28, 0x54, 0x28, 0x54,
        0x10, 0x54, 0x10,
        0x28, 0x10, 0x28,
        0x10, 0x20, 0x7c, 0x04, 0x04, 0x04,
        0x20, 0x40, 0x38, 0x04, 0x08,
        0x44, 0x6c, 0x54, 0x44,
        0x08, 0x78, 0x08, 0x78, 0x08,
        0x50, 0x58, 0x54,
        0x3c, 0x20, 0x00, 0x78, 0x28,
        0x54, 0x58, 0x50,
        0x28, 0x68, 0x38, 0x2c, 0x28,
        0x10, 0x20, 0x7c, 0x20, 0x10,
        0x10, 0x10, 0x54, 0x38, 0x10,
        0x10, 0x38, 0x54, 0x10, 0x10,
        0x78, 0x20, 0x38, 0x20,
        0x1c, 0x14, 0x1c,
        0x1c, 0x08, 0x08,
        0x60, 0x00, 0x60, 0x00, 0x60,
        0x60, 0x50, 0x58, 0x64, 0x40,
        0x74, 0x28, 0x28, 0x74,
        0x34, 0x48, 0x48, 0x34,
        0x34, 0x40, 0x40, 0x34,
        0x7c, 0x12, 0x24, 0x7a,
        0x50, 0x78, 0x54, 0x04,
        0x20, 0x54, 0x40, 0x20,
        0x78, 0x14, 0x7c, 0x54,
        0x38, 0x38, 0x38,
        0x70, 0x2c, 0x70,
        0x7c, 0x7c, 0x38, 0x10,
        0x30, 0x48, 0x78,
        0x7c, 0x50, 0x70,
        0x30, 0x48, 0x48,
        0x70, 0x50, 0x7c,
        0x30, 0x68, 0x58,
        0x10, 0x7c, 0x14,
        0xb0, 0xa8, 0x78,
        0x7c, 0x10, 0x70,
        0x74,
        0x80, 0xf4,
        0x7c, 0x10, 0x68,
        0x7c, 0x40,
        0x78, 0x08, 0x78, 0x08, 0x70,
        0x78, 0x08, 0x70,
        0x38, 0x48, 0x70,
        0xf8, 0x28, 0x38,
        0x38, 0x28, 0xf8,
        0x70, 0x08, 0x08,
        0x58, 0x58, 0x68,
        0x08, 0x7c, 0x48,
        0x38, 0x40, 0x78,
        0x38, 0x60, 0x38,
        0x38, 0x40, 0x30, 0x40, 0x38,
        0x48, 0x30, 0x48,
        0x98, 0xa0, 0x78,
        0x68, 0x58, 0x58,
        0x08, 0x04, 0x08,
        0x18, 0x60, 0x18,
        0x58, 0x64, 0x04, 0x64, 0x58,
        0x7c, 0x44, 0x28, 0x10,
        0x08, 0x78, 0x08,
        0x20, 0x70, 0x20, 0x3c,
        0x7c, 0x54, 0x00, 0x78, 0x48,
        0x78, 0x40, 0x40,
        0x70, 0x28, 0x70,
        0x28, 0x54, 0x28, 0x10,
        0x10, 0x28, 0x54, 0x28, 0x44,
        0x44, 0x28, 0x54, 0x28, 0x10,
    };

static short smallchars_offset[137] =
    {
          0,
          3,
          4,
          7,
         12,
         17,
         20,
         25,
         27,
         29,
         31,
         34,
         37,
         39,
         42,
         43,
         46,
         49,
         52,
         55,
         58,
         61,
         64,
         67,
         70,
         73,
         76,
         77,
         79,
         82,
         85,
         88,
         92,
         96,
         99,
        102,
        105,
        108,
        111,
        114,
        118,
        121,
        122,
        125,
        129,
        132,
        137,
        141,
        144,
        147,
        151,
        154,
        157,
        160,
        163,
        166,
        171,
        174,
        177,
        180,
        182,
        185,
        187,
        192,
        195,
        197,
        200,
        201,
        204,
        209,
        214,
        217,
        220,
        226,
        231,
        235,
        240,
        243,
        248,
        251,
        256,
        261,
        266,
        271,
        275,
        278,
        281,
        286,
        291,
        295,
        299,
        303,
        307,
        311,
        315,
        319,
        322,
        325,
        329,
        332,
        335,
        338,
        341,
        344,
        347,
        350,
        353,
        354,
        356,
        359,
        361,
        366,
        369,
        372,
        375,
        378,
        381,
        384,
        387,
        390,
        393,
        398,
        401,
        404,
        407,
        410,
        413,
        418,
        422,
        425,
        429,
        434,
        437,
        440,
        444,
        449,
        454,
    };

static unsigned char smallchars_map[138] =
    {
        /*   0 */  70,
        /*   1 */  71,
        /*   2 */  72,
        /*   3 */  73,
        /*   4 */  69,
        /*   5 */  74,
        /*   6 */  97,
        /*   7 */  75,
        /*   8 */  93,
        /*   9 */  76,
        /*  10 */  77,
        /*  11 */  78,
        /*  12 */  79,
        /*  13 */ 129,
        /*  14 */  80,
        /*  15 */  81,
        /*  16 */  82,
        /*  17 */  83,
        /*  18 */  92,
        /*  19 */  84,
        /*  20 */  96,
        /*  21 */  91,
        /*  22 */  88,
        /*  23 */  87,
        /*  24 */  37,
        /*  25 */  94,
        /*  26 */  86,
        /*  27 */ 130,
        /*  28 */  89,
        /*  29 */  90,
        /*  30 */ 124,
        /*  31 */  95,
        /*  32 */   0,
        /*  33 */   1,
        /*  34 */   2,
        /*  35 */   3,
        /*  36 */   4,
        /*  37 */   5,
        /*  38 */   6,
        /*  39 */   7,
        /*  40 */   8,
        /*  41 */   9,
        /*  42 */  10,
        /*  43 */  11,
        /*  44 */  12,
        /*  45 */  13,
        /*  46 */  14,
        /*  47 */  15,
        /*  48 */  16,
        /*  49 */  17,
        /*  50 */  18,
        /*  51 */  19,
        /*  52 */  20,
        /*  53 */  21,
        /*  54 */  22,
        /*  55 */  23,
        /*  56 */  24,
        /*  57 */  25,
        /*  58 */  26,
        /*  59 */  27,
        /*  60 */  28,
        /*  61 */  29,
        /*  62 */  30,
        /*  63 */  31,
        /*  64 */  32,
        /*  65 */  33,
        /*  66 */  34,
        /*  67 */  35,
        /*  68 */  36,
        /*  69 */  37,
        /*  70 */  38,
        /*  71 */  39,
        /*  72 */  40,
        /*  73 */  41,
        /*  74 */  42,
        /*  75 */  43,
        /*  76 */  44,
        /*  77 */  45,
        /*  78 */  46,
        /*  79 */  47,
        /*  80 */  48,
        /*  81 */  49,
        /*  82 */  50,
        /*  83 */  51,
        /*  84 */  52,
        /*  85 */  53,
        /*  86 */  54,
        /*  87 */  55,
        /*  88 */  56,
        /*  89 */  57,
        /*  90 */  58,
        /*  91 */  59,
        /*  92 */  60,
        /*  93 */  61,
        /*  94 */  62,
        /*  95 */  63,
        /*  96 */  64,
        /*  97 */  98,
        /*  98 */  99,
        /*  99 */ 100,
        /* 100 */ 101,
        /* 101 */ 102,
        /* 102 */ 103,
        /* 103 */ 104,
        /* 104 */ 105,
        /* 105 */ 106,
        /* 106 */ 107,
        /* 107 */ 108,
        /* 108 */ 109,
        /* 109 */ 110,
        /* 110 */ 111,
        /* 111 */ 112,
        /* 112 */ 113,
        /* 113 */ 114,
        /* 114 */ 115,
        /* 115 */ 116,
        /* 116 */ 117,
        /* 117 */ 118,
        /* 118 */ 119,
        /* 119 */ 120,
        /* 120 */ 121,
        /* 121 */ 122,
        /* 122 */ 123,
        /* 123 */  65,
        /* 124 */  66,
        /* 125 */  67,
        /* 126 */  68,
        /* 127 */  85,
        /* 128 */  26,
        /* 129 */ 125,
        /* 130 */ 126,
        /* 131 */ 128,
        /* 132 */ 131,
        /* 133 */ 132,
        /* 134 */ 127,
        /* 135 */ 133,
        /* 136 */ 134,
        /* 137 */ 135,
    };

#if defined(WINDOWS) && !defined(__GNUC__)
#pragma warning(pop)
#endif



static char *display = NULL;
static int disp_bpl;
int disp_r, disp_c, disp_w, disp_h;
int requested_disp_r, requested_disp_c;

static bool is_dirty = false;
static int dirty_top, dirty_left, dirty_bottom, dirty_right;

static std::vector<std::string> messages;

static int catalogmenu_section[6];
static int catalogmenu_rows[6];
static int catalogmenu_row[6];
static int4 catalogmenu_dir[6][6];
static int catalogmenu_item[6][6];
static bool catalog_no_top;
static int catsect_when_units_key_was_pressed = -1;

static int custommenu_length[3][6];
static char custommenu_label[3][6][7];

static arg_struct progmenu_arg[9];
static bool progmenu_is_gto[9];
static int progmenu_length[6];
static char progmenu_label[6][7];

static int appmenu_exitcallback;

/* Menu keys that should respond to certain hardware
 * keyboard keys, in addition to the keymap:
 * 0:none 1:left 2:shift-left 3:right 4:shift-right 5:del
 */
static char special_key[6] = { 0, 0, 0, 0, 0, 0 };

static int2 crosshair_x, crosshair_y;
static int2 crosshair_back;
static bool crosshair_visible;


/*******************************/
/* Private function prototypes */
/*******************************/

static void mark_dirty(int top, int left, int bottom, int right);
static int get_cat_index();

bool display_alloc(int rows, int cols) {
    if (display != NULL && disp_r == rows && disp_c == cols)
        return false;
    if (rows < 2)
        rows = 2;
    if (cols < 22)
        cols = 22;
    disp_r = rows;
    disp_c = cols;
    disp_w = cols * 6 - 1;
    disp_h = rows * 8;
    disp_bpl = (disp_w + 7) / 8;
    free(display);
    display = (char *) malloc(disp_h * disp_bpl);
    if (mode_message_lines == ALL_LINES)
        mode_message_lines = 0;
    return true;
}

bool display_exists() {
    return display != NULL;
}

bool persist_display() {
    for (int i = 0; i < 6; i++) {
        if (!write_int(catalogmenu_section[i])) return false;
        if (!write_int(catalogmenu_rows[i])) return false;
        if (!write_int(catalogmenu_row[i])) return false;
        for (int j = 0; j < 6; j++) {
            if (!write_int4(catalogmenu_dir[i][j])) return false;
            if (!write_int(catalogmenu_item[i][j])) return false;
        }
    }
    write_bool(catalog_no_top);
    write_int(catsect_when_units_key_was_pressed);
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 6; j++) {
            if (!write_int(custommenu_length[i][j])) return false;
            if (fwrite(custommenu_label[i][j], 1, 7, gfile) != 7) return false;
        }
    }
    for (int i = 0; i < 9; i++)
        if (!write_arg(progmenu_arg + i))
            return false;
    for (int i = 0; i < 9; i++)
        if (!write_bool(progmenu_is_gto[i])) return false;
    for (int i = 0; i < 6; i++) {
        if (!write_int(progmenu_length[i])) return false;
        if (fwrite(progmenu_label[i], 1, 7, gfile) != 7) return false;
    }
    if (!write_int(disp_r)) return false;
    if (!write_int(disp_c)) return false;
    if (!write_int(requested_disp_r)) return false;
    if (!write_int(requested_disp_c)) return false;
    int sz = disp_h * disp_bpl;
    if (fwrite(display, 1, sz, gfile) != sz)
        return false;
    if (!write_int(skin_flags)) return false;
    if (!write_int(appmenu_exitcallback)) return false;
    if (fwrite(special_key, 1, 6, gfile) != 6)
        return false;
    int mcount = messages.size();
    if (!write_int(mcount))
        return false;
    for (int i = 0; i < mcount; i++) {
        std::string m = messages[i];
        int ml = m.length();
        if (!write_int2(ml))
            return false;
        if (fwrite(m.c_str(), 1, ml, gfile) != ml)
            return false;
    }
    if (!write_int2(crosshair_x)) return false;
    if (!write_int2(crosshair_y)) return false;
    if (!write_int2(crosshair_back)) return false;
    if (!write_bool(crosshair_visible)) return false;
    return true;
}

bool unpersist_display(int ver) {
    int levels = ver < 16 ? 5 : 6;
    for (int i = 0; i < levels; i++) {
        if (!read_int(&catalogmenu_section[i])) return false;
        if (!read_int(&catalogmenu_rows[i])) return false;
        if (!read_int(&catalogmenu_row[i])) return false;
        if (ver < 11) {
            for (int j = 0; j < 6; j++) {
                catalogmenu_dir[i][j] = 2;
                if (!read_int(&catalogmenu_item[i][j])) return false;
            }
        } else {
            for (int j = 0; j < 6; j++) {
                if (!read_int4(&catalogmenu_dir[i][j])) return false;
                if (!read_int(&catalogmenu_item[i][j])) return false;
            }
        }
    }
    if (ver < 16) {
        catalogmenu_section[5] = catalogmenu_section[4];
        catalogmenu_rows[5] = catalogmenu_rows[4];
        catalogmenu_row[5] = catalogmenu_row[4];
        for (int i = 0; i < 6; i++) {
            catalogmenu_dir[5][i] = catalogmenu_dir[4][i];
            catalogmenu_item[5][i] = catalogmenu_item[4][i];
        }
    }
    if (ver >= 14) {
        if (!read_bool(&catalog_no_top))
            return false;
    } else {
        catalog_no_top = false;
    }
    if (ver >= 42) {
        if (!read_int(&catsect_when_units_key_was_pressed))
            return false;
    } else {
        catsect_when_units_key_was_pressed = -1;
    }
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 6; j++) {
            if (!read_int(&custommenu_length[i][j])) return false;
            if (fread(custommenu_label[i][j], 1, 7, gfile) != 7) return false;
            if (ver < 44)
                switch_30_and_94(custommenu_label[i][j], custommenu_length[i][j]);
        }
    }
    for (int i = 0; i < 9; i++)
        if (!read_arg(progmenu_arg + i, false))
            return false;
    for (int i = 0; i < 9; i++)
        if (!read_bool(&progmenu_is_gto[i])) return false;
    for (int i = 0; i < 6; i++) {
        if (!read_int(&progmenu_length[i])) return false;
        if (fread(progmenu_label[i], 1, 7, gfile) != 7) return false;
        if (ver < 44)
            switch_30_and_94(progmenu_label[i], progmenu_length[i]);
    }
    int r, c;
    if (ver < 13) {
        r = 2;
        c = 22;
    } else {
        if (!read_int(&r)) return false;
        if (!read_int(&c)) return false;
    }
    if (ver < 17) {
        requested_disp_r = r;
        requested_disp_c = c;
    } else {
        if (!read_int(&requested_disp_r)) return false;
        if (!read_int(&requested_disp_c)) return false;
    }
    display_alloc(r, c);
    int sz = disp_h * disp_bpl;
    if (fread(display, 1, sz, gfile) != sz)
        return false;
    if (ver >= 15) {
        int sf;
        if (!read_int(&sf)) return false;
        if (skin_flags == -1)
            skin_flags = sf;
        else if (sf != skin_flags)
            force_redisplay = true;
    }
    if (!read_int(&appmenu_exitcallback)) return false;
    if (fread(special_key, 1, 6, gfile) != 6)
        return false;
    messages.clear();
    if (ver >= 13) {
        int mcount;
        if (!read_int(&mcount))
            return false;
        messages.resize(mcount);
        for (int i = 0; i < mcount; i++) {
            int2 ml;
            if (!read_int2(&ml))
                return false;
            char *buf = (char *) malloc(ml);
            if (fread(buf, 1, ml, gfile) != ml) {
                free(buf);
                return false;
            }
            messages[i] = std::string(buf, ml);
            free(buf);
        }
    }
    if (ver >= 20) {
        if (!read_int2(&crosshair_x)) return false;
        if (!read_int2(&crosshair_y)) return false;
        if (!read_int2(&crosshair_back)) return false;
        if (!read_bool(&crosshair_visible)) return false;
    } else {
        crosshair_visible = false;
        crosshair_x = -1;
        crosshair_y = -1;
    }
    return true;
}

void clear_display() {
    int sz = disp_h * disp_bpl;
    for (int i = 0; i < sz; i++)
        display[i] = 0;
    mark_dirty(0, 0, disp_h, disp_w);
    memset(special_key, 0, 6);
    crosshair_visible = false;
}

void flush_display() {
    if (!is_dirty)
        return;
    shell_blitter(display, disp_bpl, dirty_left, dirty_top,
                    dirty_right - dirty_left, dirty_bottom - dirty_top);
    is_dirty = false;
}

void repaint_display() {
    shell_blitter(display, disp_bpl, 0, 0, disp_w, disp_h);
}

void draw_pixel(int x, int y) {
    if (x < 0 || x >= disp_w || y < 0 || y >= disp_h)
        return;
    display[y * disp_bpl + (x >> 3)] |= 1 << (x & 7);
    mark_dirty(y, x, y + 1, x + 1);
}

void draw_line(int x1, int y1, int x2, int y2) {
    int dx = x1 - x2;
    if (dx < 0)
        dx = -dx;
    int dy = y1 - y2;
    if (dy < 0)
        dy = -dy;
    bool swap = dy > dx;
    if (swap) {
        int t = x1;
        x1 = y1;
        y1 = t;
        t = x2;
        x2 = y2;
        y2 = t;
    }
    int w = swap ? disp_h : disp_w;
    int h = swap ? disp_w : disp_h;
    if (x1 > x2) {
        int t = x1;
        x1 = x2;
        x2 = t;
        t = y1;
        y1 = y2;
        y2 = t;
    }
    if (x1 >= w || x2 < 0)
        return;
    if (x1 >= 0 && y1 >= 0)
        if (swap)
            draw_pixel(y1, x1);
        else
            draw_pixel(x1, y1);
    if (x1 == x2)
        return;
    int x = x1;
    double sy = ((double) (y2 - y1)) / (x2 - x1);
    double y = y1;
    if (x < -1) {
        y -= (x + 1) * sy;
        x = -1;
    }
    if (x2 > w - 1)
        x2 = w - 1;
    while (x < x2) {
        x++;
        y += sy;
        int iy = y < 0 ? -(int) (-y + 0.5) : (int) (y + 0.5);
        if (iy >= h) {
            if (sy >= 0)
                return;
        } else if (iy < 0) {
            if (sy <= 0)
                return;
        } else {
            if (swap)
                draw_pixel(iy, x);
            else
                draw_pixel(x, iy);
        }
    }
}

void draw_pattern(phloat dx, phloat dy, const char *pattern, int pattern_width){
    int x, y, h, v, hmin, hmax, vmin, vmax;
    x = dx < 0 ? to_int(-floor(-dx + 0.5)) : to_int(floor(dx + 0.5));
    y = dy < 0 ? to_int(-floor(-dy + 0.5)) : to_int(floor(dy + 0.5));
    if (x + pattern_width < 1 || x > disp_w || y + 8 < 1 || y > disp_h)
        return;
    hmin = x < 1 ? 1 - x : 0;
    hmax = x + pattern_width > disp_w + 1 ? disp_w + 1 - x : pattern_width;
    vmin = y < 1 ? 1 - y : 0;
    vmax = y + 8 > disp_h + 1 ? disp_h + 1 - y : 8;
    x--;
    y--;
    if (flags.f.agraph_control1) {
        if (flags.f.agraph_control0)
            /* dst = dst ^ src */
            for (h = hmin; h < hmax; h++) {
                char c = pattern[h] >> vmin;
                for (v = vmin; v < vmax; v++) {
                    if (c & 1) {
                        int X = h + x;
                        int Y = v + y;
                        display[Y * disp_bpl + (X >> 3)] ^= 1 << (X & 7);
                    }
                    c >>= 1;
                }
            }
        else
            /* dst = dst & ~src */
            for (h = hmin; h < hmax; h++) {
                char c = pattern[h] >> vmin;
                for (v = vmin; v < vmax; v++) {
                    if (c & 1) {
                        int X = h + x;
                        int Y = v + y;
                        display[Y * disp_bpl + (X >> 3)] &= ~(1 << (X & 7));
                    }
                    c >>= 1;
                }
            }
    } else {
        if (flags.f.agraph_control0)
            /* dst = src */
            for (h = hmin; h < hmax; h++) {
                char c = pattern[h] >> vmin;
                for (v = vmin; v < vmax; v++) {
                    int X = h + x;
                    int Y = v + y;
                    if (c & 1)
                        display[Y * disp_bpl + (X >> 3)] |= 1 << (X & 7);
                    else
                        display[Y * disp_bpl + (X >> 3)] &= ~(1 << (X & 7));
                    c >>= 1;
                }
            }
        else
            /* dst = dst | src */
            for (h = hmin; h < hmax; h++) {
                char c = pattern[h] >> vmin;
                for (v = vmin; v < vmax; v++) {
                    if (c & 1) {
                        int X = h + x;
                        int Y = v + y;
                        display[Y * disp_bpl + (X >> 3)] |= 1 << (X & 7);
                    }
                    c >>= 1;
                }
            }
    }
    mark_dirty(y + vmin, x + hmin, y + vmax, x + hmax);
}

static int get_pixel(int x, int y) {
    if (x < 0 || x >= disp_w || y < 0 || y >= disp_h)
        return 0;
    return (display[y * disp_bpl + (x >> 3)] & (1 << (x & 7))) != 0;
}

static void set_pixel(int x, int y, bool p) {
    if (x < 0 || x >= disp_w || y < 0 || y >= disp_h)
        return;
    if (p)
        display[y * disp_bpl + (x >> 3)] |= 1 << (x & 7);
    else
        display[y * disp_bpl + (x >> 3)] &= ~(1 << (x & 7));
}

void hide_crosshairs() {
    if (!crosshair_visible)
        return;
    int2 bits = crosshair_back;
    set_pixel(crosshair_x, crosshair_y + 2, bits & 1); bits >>= 1;
    set_pixel(crosshair_x, crosshair_y - 2, bits & 1); bits >>= 1;
    set_pixel(crosshair_x + 2, crosshair_y, bits & 1); bits >>= 1;
    set_pixel(crosshair_x - 2, crosshair_y, bits & 1); bits >>= 1;
    set_pixel(crosshair_x, crosshair_y + 1, bits & 1); bits >>= 1;
    set_pixel(crosshair_x, crosshair_y - 1, bits & 1); bits >>= 1;
    set_pixel(crosshair_x + 1, crosshair_y, bits & 1); bits >>= 1;
    set_pixel(crosshair_x - 1, crosshair_y, bits & 1); bits >>= 1;
    set_pixel(crosshair_x, crosshair_y, bits & 1);
    int t = crosshair_y - 2; if (t < 0) t = 0;
    int l = crosshair_x - 2; if (l < 0) l = 0;
    int b = crosshair_y + 3; if (b >= disp_h) b = disp_h - 1;
    int r = crosshair_x + 3; if (r >= disp_w) r = disp_w - 1;
    mark_dirty(t, l, b + 1, r + 1);
    crosshair_visible = false;
}

void move_crosshairs(int x, int y, bool show) {
    hide_crosshairs();
    if (x < -2 || x >= disp_w + 2 || y < -2 || y >= disp_h + 2)
        return;
    crosshair_x = x;
    crosshair_y = y;
    if (!show)
        return;
    int2 bits = get_pixel(crosshair_x, crosshair_y);
    bits = (bits << 1) + get_pixel(crosshair_x - 1, crosshair_y);
    bits = (bits << 1) + get_pixel(crosshair_x + 1, crosshair_y);
    bits = (bits << 1) + get_pixel(crosshair_x, crosshair_y - 1);
    bits = (bits << 1) + get_pixel(crosshair_x, crosshair_y + 1);
    bits = (bits << 1) + get_pixel(crosshair_x - 2, crosshair_y);
    bits = (bits << 1) + get_pixel(crosshair_x + 2, crosshair_y);
    bits = (bits << 1) + get_pixel(crosshair_x, crosshair_y - 2);
    bits = (bits << 1) + get_pixel(crosshair_x, crosshair_y + 2);
    crosshair_back = bits;
    set_pixel(crosshair_x, crosshair_y, 1);
    set_pixel(crosshair_x - 1, crosshair_y, 1);
    set_pixel(crosshair_x + 1, crosshair_y, 1);
    set_pixel(crosshair_x, crosshair_y - 1, 1);
    set_pixel(crosshair_x, crosshair_y + 1, 1);
    set_pixel(crosshair_x - 2, crosshair_y, 1);
    set_pixel(crosshair_x + 2, crosshair_y, 1);
    set_pixel(crosshair_x, crosshair_y - 2, 1);
    set_pixel(crosshair_x, crosshair_y + 2, 1);
    int t = crosshair_y - 2; if (t < 0) t = 0;
    int l = crosshair_x - 2; if (l < 0) l = 0;
    int b = crosshair_y + 3; if (b >= disp_h) b = disp_h - 1;
    int r = crosshair_x + 3; if (r >= disp_w) r = disp_w - 1;
    mark_dirty(t, l, b + 1, r + 1);
    crosshair_visible = true;
}

bool get_crosshairs(int *x, int *y) {
    if (crosshair_x >= 0 && crosshair_x < disp_w && crosshair_y >= 0 && crosshair_y < disp_h) {
        *x = crosshair_x;
        *y = crosshair_y;
    } else {
        *x = disp_w / 2;
        *y = disp_h / 2;
    }
    return crosshair_visible;
}

void fly_goose() {
    static uint4 lastgoosetime = 0;
    uint4 goosetime = shell_milliseconds();
    if (goosetime < lastgoosetime)
        // shell_milliseconds() wrapped around
        lastgoosetime = 0;
    if (goosetime - 100 < lastgoosetime)
        /* No goose movements if the most recent one was less than 100 ms
         * ago; in other words, maximum goose speed is 10 positions/second
         */
        return;
    lastgoosetime = goosetime;

    if (mode_goose < 0) {
        clear_row(0);
        mode_goose = (-mode_goose) % disp_c;
        draw_char(mode_goose, 0, 6);
    } else {
        draw_char(mode_goose, 0, ' ');
        mode_goose = (mode_goose + 1) % disp_c;
        draw_char(mode_goose, 0, 6);
    }
    flush_display();
}

void move_prgm_highlight(int direction) {
    prgm_highlight_row += direction;
    if (prgm_highlight_row < 0)
        prgm_highlight_row = 0;
    else {
        int avail = disp_r - (mode_header && disp_r >= 4) - (get_front_menu() != MENU_NONE);
        if (prgm_highlight_row >= avail)
            prgm_highlight_row = avail - 1;
    }
}

void squeak() {
    if (flags.f.audio_enable)
        shell_beeper(10);
}

void tone(int n) {
    if (flags.f.audio_enable)
        shell_beeper(n);
}


static void mark_dirty(int top, int left, int bottom, int right) {
    if (is_dirty) {
        if (top < dirty_top)
            dirty_top = top;
        if (left < dirty_left)
            dirty_left = left;
        if (bottom > dirty_bottom)
            dirty_bottom = bottom;
        if (right > dirty_right)
            dirty_right = right;
    } else {
        dirty_top = top;
        dirty_left = left;
        dirty_bottom = bottom;
        dirty_right = right;
        is_dirty = true;
    }
}

void draw_char(int x, int y, char c) {
    int X, Y, h, v;
    unsigned char uc = (unsigned char) c;
    if (x < 0 || x >= disp_c || y < 0 || y >= disp_r)
        return;
    if (undefined_char(uc) || uc == 138)
        uc -= 128;
    X = x * 6;
    Y = y * 8;
    for (v = 0; v < 8; v++) {
        int YY = Y + v;
        for (h = 0; h < 5; h++) {
            int XX = X + h;
            char mask = 1 << (XX & 7);
            if (bigchars[uc][h] & (1 << v))
                display[YY * disp_bpl + (XX >> 3)] |= mask;
            else
                display[YY * disp_bpl + (XX >> 3)] &= ~mask;
        }
    }
    mark_dirty(Y, X, Y + 8, X + 5);
}

void draw_block(int x, int y) {
    int X, Y, h, v;
    if (x < 0 || x >= disp_c || y < 0 || y >= disp_r)
        return;
    X = x * 6;
    Y = y * 8;
    for (v = 0; v < 8; v++) {
        int YY = Y + v;
        for (h = 0; h < 5; h++) {
            int XX = X + h;
            char mask = 1 << (XX & 7);
            if (v < 7)
                display[YY * disp_bpl + (XX >> 3)] |= mask;
            else
                display[YY * disp_bpl + (XX >> 3)] &= ~mask;
        }
    }
    mark_dirty(Y, X, Y + 8, X + 5);
}

const char *get_char(char c) {
    unsigned char uc = (unsigned char) c;
    if (undefined_char(uc) || uc == 138)
        uc -= 128;
    return bigchars[uc];
}

void draw_string(int x, int y, const char *s, int length) {
    while (length != 0 && x < disp_c) {
        draw_char(x++, y, *s++);
        length--;
    }
}

int draw_small_string(int x, int y, const char *s, int length, int max_width, bool right_align, bool left_trunc, bool reverse) {
    if (length == 0)
        return 0;
    int w = 0;
    int n = 0;
    int m = smallchars_map[26];
    int we = smallchars_offset[m + 1] - smallchars_offset[m];
    bool we_done = we > max_width;
    int ne = we_done ? -1 : 0;
    bool ellipsis = false;

    while (n < length) {
        int c = (left_trunc ? s[length - n - 1] : s[n]) & 255;
        if (undefined_char(c) || c == 138)
            c &= 127;
        m = smallchars_map[c];
        int cw = smallchars_offset[m + 1] - smallchars_offset[m];
        if (!we_done) {
            if (we + cw + 1 > max_width)
                we_done = true;
            else {
                we += cw + 1;
                ne++;
            }
        }
        if (w != 0)
            cw++;
        if (w + cw > max_width) {
            ellipsis = true;
            break;
        }
        w += cw;
        n++;
    }

    if (ellipsis) {
        if (ne == -1)
            return 0;
        n = ne;
        w = we;
    }

    if (right_align)
        x = x + max_width - w;

    for (int i = 0; i < n + ellipsis; i++) {
        int c;
        if (left_trunc)
            c = ellipsis ? i == 0 ? 26 : s[length - n - 1 + i] : s[length - n + i];
        else
            c = i == n ? 26 : s[i];
        c &= 255;
        int m = smallchars_map[c];
        int o = smallchars_offset[m];
        int cw = smallchars_offset[m + 1] - o;
        for (int j = 0; j < cw; j++) {
            if (x >= 0 && x < disp_w) {
                int b = smallchars[o + j];
                for (int k = 0; k < 8; k++) {
                    int Y = k + y;
                    if (Y >= 0 && Y < disp_h && (b >> k) & 1)
                        if (reverse)
                            display[Y * disp_bpl + (x >> 3)] &= ~(1 << (x & 7));
                        else
                            display[Y * disp_bpl + (x >> 3)] |= 1 << (x & 7);
                }
            }
            x++;
        }
        x++;
    }
    return w;
}

int small_string_width(const char *s, int length) {
    int w = 0;
    for (int n = 0; n < length; n++) {
        int c = s[n] & 255;
        if (undefined_char(c) || c == 138)
            c &= 127;
        int m = smallchars_map[c];
        int cw = smallchars_offset[m + 1] - smallchars_offset[m];
        if (w != 0)
            cw++;
        w += cw;
    }
    return w;
}

void draw_message(int y, const char *s, int length, bool flush) {
    clear_row(y);
    draw_string(0, y, s, length);
    mode_message_lines = y + 1;
    messages.resize(y + 1);
    messages[y] = std::string(s, length);
    if (flush)
        flush_display();
}

void draw_long_message(int y, const char *s, int length, bool flush) {
    while (length > 0 && y < disp_r) {
        int w = length > disp_c ? disp_c : length;
        draw_message(y, s, w, false);
        y++;
        s += w;
        length -= w;
    }
    if (flush)
        flush_display();
}

void clear_message() {
    messages.clear();
    mode_message_lines = 0;
}

void fill_rect(int x, int y, int width, int height, int color) {
    if (x < 0) {
        width += x;
        x = 0;
    } else if (x >= disp_w)
        return;
    if (y < 0) {
        height += y;
        y = 0;
    } else if (y >= disp_h)
        return;
    if (x + width > disp_w)
        width = disp_w - x;
    if (y + height > disp_h)
        height = disp_h - y;
    int h, v;
    for (v = y; v < y + height; v++)
        for (h = x; h < x + width; h++)
            if (color)
                display[v * disp_bpl + (h >> 3)] |= 1 << (h & 7);
            else
                display[v * disp_bpl + (h >> 3)] &= ~(1 << (h & 7));
    mark_dirty(y, x, y + height, x + width);
}

void draw_key(int n, int highlight, int hide_meta,
                        const char *s, int length, bool reverse /* = false */) {

    fill_rect(n * disp_c, disp_h - 7, disp_c - 1, 7, 1);
    if (reverse)
        fill_rect(n * disp_c + 1, disp_h - 6, disp_c - 3, 5, 0);

    int swidth = 0;
    int len = 0;
    int len2;
    int x, i;
    int fatdot = 31;

    /* Note: the SST handling code uses a magic value of 2 in prgm_mode
     * so that we know *not* to suppress menu highlights while stepping. */
    if (flags.f.prgm_mode == 1)
        highlight = 0;

    if (highlight) {
        int f = smallchars_map[fatdot];
        swidth = smallchars_offset[f + 1] - smallchars_offset[f];
    }

    int hidden = 0;
    while (len < length) {
        int c = (unsigned char) s[len++];

        if (undefined_char(c) || c == 138)
            if (hide_meta) {
                hidden++;
                continue;
            } else
                c &= 127;
        int m = smallchars_map[c];
        int cw = smallchars_offset[m + 1] - smallchars_offset[m];
        if (swidth != 0)
            cw++;
        if (swidth + cw > disp_c - 3) {
            len--;
            hidden = 0;
            break;
        }
        swidth += cw;
    }

    if (swidth == 0)
        // This means either an empty string, or a string consisting of
        // only 'meta' characters. The latter is used to make CMD_NULL
        // show up as blank in menu keys, while being shown as a regular
        // word when displaying command feedback. We don't want to
        // un-hide anything in this case, so we bow out now.
        return;

    int unhidden = 0;
    if (hidden > 0 && disp_c > 22) {
        // The 'meta' characters were selected based on having standard
        // HP-42S-like menu keys, that is, having 19 horizontal pixels
        // to work with. If we have a wider screen, we have more pixels
        // per menu key, so let's see if we can display more characters.
        for (int i = 0; i < len; i++) {
            int c = (unsigned char) s[i];
            if (!undefined_char(c) && c != 138)
                continue;
            c &= 127;
            int m = smallchars_map[c];
            int cw = smallchars_offset[m + 1] - smallchars_offset[m] + 1;
            if (swidth + cw > disp_c - 3)
                break;
            swidth += cw;
            unhidden++;
            if (--hidden == 0)
                break;
        }
    }

    x = n * disp_c + (disp_c - 1 - swidth) / 2;
    len2 = highlight ? len + 1 : len;
    for (i = 0; i < len2; i++) {
        int c, m, o, cw, j;
        if (i == len)
            c = fatdot;
        else
            c = (unsigned char) s[i];
        if (undefined_char(c) || c == 138) {
            if (hide_meta) {
                if (unhidden == 0)
                    continue;
                unhidden--;
            }
            c &= 127;
        }
        m = smallchars_map[c];
        o = smallchars_offset[m];
        cw = smallchars_offset[m + 1] - o;
        bool tp = false, bp = false, p;
        for (j = 0; j < cw; j++) {
            int b, k;
            b = smallchars[o + j];
            for (k = 0; k < 8; k++)
                if ((b >> k) & 1) {
                    if (reverse) {
                        display[(k + disp_h - 8) * disp_bpl + (x >> 3)] |= (1 << (x & 7));
                        if (k == 1 || k == 7) {
                            if (k == 1) {
                                p = tp;
                                tp = true;
                            } else {
                                p = bp;
                                bp = true;
                            }
                            if (!p)
                                display[(k + disp_h - 8) * disp_bpl + ((x - 1) >> 3)] &= ~(1 << ((x - 1) & 7));
                            display[(k + disp_h - 8) * disp_bpl + ((x + 1) >> 3)] &= ~(1 << ((x + 1) & 7));
                        }
                    } else
                        display[(k + disp_h - 8) * disp_bpl + (x >> 3)] &= ~(1 << (x & 7));
                } else {
                    if (reverse) {
                        if (k == 1)
                            tp = false;
                        else if (k == 7)
                            bp = false;
                    }
                }
            x++;
        }
        x++;
    }
    /* No need for mark_dirty(); fill_rect() took care of that already. */

    /* Support for automatically mapping physical cursor left,
     * cursor right, and delete keys, to menu keys with legends
     * consisting of arrows, double-head arrows, or the word
     * DEL.
     */
    if (string_equals(s, length, "\20", 1))
        /* <- */
        special_key[n] = 1;
    else if (string_equals(s, length, "<\20", 2)
          || string_equals(s, length, "^", 1))
        /* <<- or up */
        special_key[n] = 2;
    else if (string_equals(s, length, "\17", 1))
        /* -> */
        special_key[n] = 3;
    else if (string_equals(s, length, "\17>", 2)
          || string_equals(s, length, "\16", 1))
        /* ->> or down */
        special_key[n] = 4;
    else if (string_equals(s, length, "DEL", 3))
        special_key[n] = 5;
    else
        special_key[n] = 0;
}

bool should_highlight(int cmd) {
    switch (cmd) {
        case CMD_FIX:
            return flags.f.fix_or_all
                        && !flags.f.eng_or_all;
        case CMD_SCI:
            return !flags.f.fix_or_all
                        && !flags.f.eng_or_all;
        case CMD_ENG:
            return !flags.f.fix_or_all
                        && flags.f.eng_or_all;
        case CMD_ALL:
            return flags.f.fix_or_all
                        && flags.f.eng_or_all;
        case CMD_RDXDOT:
            return flags.f.decimal_point;
        case CMD_RDXCOMMA:
            return !flags.f.decimal_point;
        case CMD_DEG:
            return !flags.f.rad && !flags.f.grad;
        case CMD_RAD:
            return flags.f.rad;
        case CMD_GRAD:
            return !flags.f.rad && flags.f.grad;
        case CMD_POLAR:
            return flags.f.polar;
        case CMD_RECT:
            return !flags.f.polar;
        case CMD_QUIET:
            return !flags.f.audio_enable;
        case CMD_CPXRES:
            return !flags.f.real_result_only;
        case CMD_REALRES:
            return flags.f.real_result_only;
        case CMD_KEYASN:
            return !flags.f.local_label;
        case CMD_LCLBL:
            return flags.f.local_label;
        case CMD_BSIGNED:
            return flags.f.base_signed;
        case CMD_BWRAP:
            return flags.f.base_wrap;
        case CMD_MDY:
            return !flags.f.ymd && !flags.f.dmy;
        case CMD_DMY:
            return !flags.f.ymd && flags.f.dmy;
        case CMD_YMD:
            return flags.f.ymd;
        case CMD_CLK12:
            return !mode_time_clk24;
        case CMD_CLK24:
            return mode_time_clk24;
        case CMD_4STK:
            return !flags.f.big_stack;
        case CMD_NSTK:
            return flags.f.big_stack;
        case CMD_STD:
            return !flags.f.eqn_compat;
        case CMD_COMP:
            return flags.f.eqn_compat;
        case CMD_DIRECT:
            return flags.f.direct_solver;
        case CMD_NUMERIC:
            return !flags.f.direct_solver;
        case CMD_PON:
            return flags.f.printer_exists;
        case CMD_POFF:
            return !flags.f.printer_exists;
        case CMD_MAN:
            return !flags.f.trace_print
                        && !flags.f.normal_print;
        case CMD_NORM:
            return !flags.f.trace_print
                        && flags.f.normal_print;
        case CMD_TRACE:
            return flags.f.trace_print
                        && !flags.f.normal_print;
        case CMD_STRACE:
            return flags.f.trace_print
                        && flags.f.normal_print;
        case CMD_ALLSIGMA:
            return flags.f.all_sigma;
        case CMD_LINSIGMA:
            return !flags.f.all_sigma;
        case CMD_LINF:
            return flags.f.lin_fit;
        case CMD_LOGF:
            return flags.f.log_fit;
        case CMD_EXPF:
            return flags.f.exp_fit;
        case CMD_PWRF:
            return flags.f.pwr_fit;
        case CMD_WRAP:
            return !flags.f.grow;
        case CMD_GROW:
            return flags.f.grow;
        case CMD_BINM:
            return get_base() == 2;
        case CMD_OCTM:
            return get_base() == 8;
        case CMD_DECM:
            return get_base() == 10;
        case CMD_HEXM:
            return get_base() == 16;
        case CMD_HEADER:
            return mode_header;
        case CMD_1LINE:
            return !mode_multi_line;
        case CMD_NLINE:
            return mode_multi_line;
        case CMD_LTOP:
            return mode_lastx_top;
        case CMD_ATOP:
            return mode_alpha_top;
        case CMD_HFLAGS:
            return mode_header_flags;
        case CMD_HPOLAR:
            return mode_header_polar;
        case CMD_STK:
            return mode_matedit_stk;
        case CMD_TBEGIN: {
            vartype *v = recall_var("BEGIN", 5);
            return v != NULL && v->type == TYPE_REAL && ((vartype_real *) v)->x == 1;
        }
        case CMD_TEND: {
            vartype *v = recall_var("BEGIN", 5);
            return v != NULL && v->type == TYPE_REAL && ((vartype_real *) v)->x == 0;
        }
    }
    return false;
}

int special_menu_key(int which) {
    for (int i = 0; i < 6; i++)
        if (special_key[i] == which)
            return i + 1;
    return 0;
}

void clear_row(int row) {
    fill_rect(0, row * 8, disp_w, 8, 0);
    crosshair_visible = false;
}

static int prgmline2buf(char *buf, int len, int4 line, int highlight,
                        int cmd, arg_struct *arg, const char *orig_num,
                        bool shift_left = false,
                        bool highlight_final_end = true,
                        char **xstr = NULL) {
    int bufptr = 0;
    if (line != -1) {
        if (line < 10)
            char2buf(buf, len, &bufptr, '0');
        bufptr += int2string(line, buf + bufptr, len - bufptr);
        char h = highlight == 0 ? ' '
                : highlight == 2 && !current_prgm.is_editable() ? 134
                : highlight == 2 && current_prgm.is_locked() ? 135
                : 6;
        char2buf(buf, len, &bufptr, h);
    }

    if (line == 0) {
        directory *saved_cwd = cwd;
        cwd = dir_list[current_prgm.dir];
        int4 size = core_program_size(current_prgm.idx);
        cwd = saved_cwd;
        string2buf(buf, len, &bufptr, "{ ", 2);
        bufptr += int2string(size, buf + bufptr, len - bufptr);
        string2buf(buf, len, &bufptr, "-Byte Prgm }", 12);
    } else if (alpha_active() && mode_alpha_entry && highlight) {
        int append = entered_string_length > 0 && entered_string[0] == 127;
        if (append) {
            string2buf(buf, len, &bufptr, "\177\"", 2);
            string2buf(buf, len, &bufptr, entered_string + 1,
                            entered_string_length - 1);
        } else {
            char2buf(buf, len, &bufptr, '"');
            string2buf(buf, len, &bufptr, entered_string, entered_string_length);
        }
        char2buf(buf, len, &bufptr, '_');
    } else if (highlight_final_end && cmd == CMD_END
                    && current_prgm.idx == dir_list[current_prgm.dir]->prgms_count - 1) {
        string2buf(buf, len, &bufptr, ".END.", 5);
    } else if (cmd == CMD_NUMBER || cmd == CMD_N_PLUS_U && arg->type != ARGTYPE_NONE) {
        const char *num;
        if (orig_num != NULL)
            num = orig_num;
        else
            num = phloat2program(arg->val_d);
        int numlen = (int) strlen(num);
        if (cmd == CMD_N_PLUS_U) {
            int tlen = bufptr + numlen + 1 + arg->length;
            if (tlen > len && xstr != NULL) {
                char *b = (char *) malloc(tlen);
                memcpy(b, buf, bufptr);
                memcpy(b + bufptr, num, numlen);
                b[bufptr + numlen] = '_';
                memcpy(b + bufptr + numlen + 1, arg->val.xstr, arg->length);
                *xstr = b;
                return tlen;
            }
            char *b = (char *) malloc(numlen + arg->length + 2);
            memcpy(b, num, numlen);
            b[numlen] = '_';
            memcpy(b + numlen + 1, arg->val.xstr, arg->length);
            b[numlen + arg->length + 1] = 0;
            num = b;
            numlen += arg->length + 1;
        }
        if (bufptr + numlen <= len) {
            memcpy(buf + bufptr, num, numlen);
            bufptr += numlen;
        } else {
            if (shift_left) {
                buf[0] = 26;
                if (numlen >= len - 1) {
                    memcpy(buf + 1, num + numlen - len + 1, len - 1);
                } else {
                    int off = bufptr + numlen - len;
                    memmove(buf + 1, buf + off + 1, bufptr - off - 1);
                    bufptr -= off;
                    memcpy(buf + bufptr, num, len - bufptr);
                }
            } else {
                memcpy(buf + bufptr, num, len - bufptr - 1);
                buf[len - 1] = 26;
            }
            bufptr = len;
        }
        if (cmd == CMD_N_PLUS_U)
            free((char *) num);
    } else if (cmd == CMD_STRING) {
        int append = arg->length > 0 && arg->val.text[0] == 127;
        if (append)
            char2buf(buf, len, &bufptr, 127);
        char2buf(buf, len, &bufptr, '"');
        string2buf(buf, len, &bufptr, arg->val.text + append,
                                     arg->length - append);
        char2buf(buf, len, &bufptr, '"');
    } else if (cmd == CMD_XSTR && xstr != NULL && bufptr + 7 + arg->length > len) {
        *xstr = (char *) malloc(bufptr + 7 + arg->length);
        if (*xstr == NULL)
            goto normal;
        memcpy(*xstr, buf, bufptr);
        bufptr += command2buf(*xstr + bufptr, arg->length + 7, cmd, arg);
    } else if (cmd == CMD_EMBED && xstr != NULL) {
        equation_data *eqd = eq_dir->prgms[arg->val.num].eq_data;
        int eqlen = (arg->type == ARGTYPE_NUM ? 2 : 7) + eqd->length;
        if (bufptr + eqlen <= len)
            goto normal;
        *xstr = (char *) malloc(bufptr + eqlen);
        if (*xstr == NULL)
            goto normal;
        memcpy(*xstr, buf, bufptr);
        bufptr += command2buf(*xstr + bufptr, eqlen, cmd, arg);
    } else {
        normal:
        bufptr += command2buf(buf + bufptr, len - bufptr, cmd, arg);
    }

    return bufptr;
}

void tb_write(textbuf *tb, const char *data, size_t size) {
    if (tb->size + size > tb->capacity) {
        size_t newcapacity = tb->capacity == 0 ? 1024 : (tb->capacity << 1);
        while (newcapacity < tb->size + size)
            newcapacity <<= 1;
        char *newbuf = (char *) realloc(tb->buf, newcapacity);
        if (newbuf == NULL) {
            /* Bummer! Let's just append as much as we can */
            memcpy(tb->buf + tb->size, data, tb->capacity - tb->size);
            tb->size = tb->capacity;
            tb->fail = true;
        } else {
            tb->buf = newbuf;
            tb->capacity = newcapacity;
            memcpy(tb->buf + tb->size, data, size);
            tb->size += size;
        }
    } else {
        memcpy(tb->buf + tb->size, data, size);
        tb->size += size;
    }
}

void tb_indent(textbuf *tb, int indent) {
    for (int i = 0; i < indent; i++)
        tb_write(tb, " ", 1);
}

void tb_write_null(textbuf *tb) {
    char c = 0;
    tb_write(tb, &c, 1);
}

void tb_print_current_program(textbuf *tb) {
    int4 pc = 0;
    int line = 0;
    int cmd;
    arg_struct arg;
    bool end = false;
    char buf[100];
    char utf8buf[500];
    do {
        const char *orig_num;
        if (line > 0) {
            get_next_command(&pc, &cmd, &arg, 0, &orig_num);
            if (cmd == CMD_END)
                end = true;
        }
        char *xstr = NULL;
        int len = prgmline2buf(buf, 100, line, cmd == CMD_LBL, cmd, &arg, orig_num, false, false, &xstr);
        char *buf2 = xstr == NULL ? buf : xstr;
        for (int i = 0; i < len; i++)
            if (buf2[i] == 10)
                buf2[i] = 138;
        int off = 0;
        while (len > 0) {
            int slen = len <= 100 ? len : 100;
            int utf8len = hp2ascii(utf8buf, buf2 + off, slen);
            tb_write(tb, utf8buf, utf8len);
            off += slen;
            len -= slen;
        }
        tb_write(tb, "\r\n", 2);
        free(xstr);
        line++;
    } while (!end);
}

static std::string get_incomplete_command();

static int display_prgm_line(int offset, int headers = 0, int footers = 0) {
    /* When the current line is being displayed (offset = 0), this tries
     * to display it in full, across multiple lines, if necessary.
     * It tries to draw the line at row prgm_highlight_row + headers,
     * using the space left over by the headers and footers. If the line
     * would run into the footers, it tries to draw it higher. If the
     * line doesn't fit even after moving, it is truncated.
     * If the line has to be moved in order to fit, prgm_highlight_row is
     * adjusted accordingly. The function returns the total number of
     * lines used.
     * With offset = INT_MAX, the behavior is as above, except the line
     * is displayed at line 0, regardless of prgm_highlight_row, headers,
     * or footers. This is for SHOW.
     * With any other offset, current_line + offset is displayed at
     * prgm_highlight_row + offset, truncated to one line.
     */
    bool show = offset == INT_MAX;
    if (show)
        offset = 0;

    try {
        std::string buf;
        int4 line = pc2line(pc) + offset;
        if (mode_command_entry && offset == 0) {
            buf = get_incomplete_command();
        } else if (mode_number_entry && offset == 0) {
            if (line < 10)
                buf += '0';
            char nbuf[10];
            int len = int2string(line, nbuf, 10);
            buf += std::string(nbuf, len);
            buf += '\6';
            buf += std::string(cmdline, cmdline_length);
            buf += '_';
        } else {
            int cmd;
            arg_struct arg;
            const char *orig_num;
            if (line > 0) {
                int4 tmpline = line;
                if ((mode_command_entry || mode_number_entry || mode_alpha_entry) && offset > 0)
                    tmpline--;
                int4 tmppc = line2pc(tmpline);
                get_next_command(&tmppc, &cmd, &arg, 0, &orig_num);
            }
            char lbuf[100];
            char *xstr = NULL;
            int len = prgmline2buf(lbuf, 100, line, offset == 0 ? 2 : 0, cmd, &arg, orig_num, false, true, &xstr);
            buf = std::string(xstr == NULL ? lbuf : xstr, len);
            free(xstr);
        }

        int row = headers + prgm_highlight_row + offset;
        int orig_row = row;
        int lines;
        int4 nlines = pc2line(dir_list[current_prgm.dir]->prgms[current_prgm.idx].size);
        if (mode_command_entry || mode_number_entry || mode_alpha_entry)
            nlines++;

        if (offset != 0) {
            if (line < 0) {
                out_of_range:
                clear_row(row);
                return 1;
            }
            if (line > nlines)
                goto out_of_range;
            lines = 1;
        } else if (show) {
            row = 0;
            lines = disp_r;
        } else {
            lines = disp_r - headers - footers;
        }
        int maxlength = lines * disp_c;
        if (buf.length() > maxlength)
            buf = buf.substr(0, maxlength - 1) + "\32";
        lines = (buf.length() + disp_c - 1) / disp_c;

        if (!show && offset == 0) {
            int excess = row + lines + footers - disp_r;
            if (excess > 0)
                row -= excess;
            excess = row + lines + footers - disp_r + nlines - line;
            if (excess < 0)
                row -= excess;
            excess = row - line - headers;
            if (excess > 0)
                row -= excess;
            prgm_highlight_row += row - orig_row;
        }

        int pos = 0;
        const char *p = buf.c_str();
        int len = buf.length();
        for (int i = row; i < row + lines; i++) {
            clear_row(i);
            int end = pos + disp_c;
            if (end > len)
                end = len;
            draw_string(0, i, p + pos, end - pos);
            pos = end;
        }

        return lines;

    } catch (std::bad_alloc &) {
        int row = headers + prgm_highlight_row;
        clear_row(row);
        draw_string(0, row, "<Low Mem>", 9);
        return 1;
    }
}

static void display_level(int level, int row) {
    clear_row(row);
    if (!flags.f.big_stack && level > 3)
        return;
    int len = disp_c + 1;
    char *buf = (char *) malloc(len);
    freer f(buf);

    int bufptr = 0;
    if (level == 0 && (matedit_mode == 2 || matedit_mode == 3)) {
        char nbuf[10];
        int n;
        for (int i = 0; i < matedit_stack_depth; i++) {
            n = int2string(matedit_stack[i].coord + 1, nbuf, 10);
            string2buf(buf, len, &bufptr, nbuf, n);
            char2buf(buf, len, &bufptr, '.');
        }
        if (matedit_is_list) {
            vartype *m;
            int err = matedit_get(&m);
            if (err != ERR_NONE || ((vartype_list *) m)->size == 0) {
                char2buf(buf, len, &bufptr, 'E');
            } else {
                n = int2string(matedit_i + 1, nbuf, 10);
                string2buf(buf, len, &bufptr, nbuf, n);
            }
        } else {
            n = int2string(matedit_i + 1, nbuf, 10);
            string2buf(buf, len, &bufptr, nbuf, n);
            char2buf(buf, len, &bufptr, ':');
            n = int2string(matedit_j + 1, nbuf, 10);
            string2buf(buf, len, &bufptr, nbuf, n);
        }
        char2buf(buf, len, &bufptr, '=');
    } else if (level == 0 && input_length > 0) {
        string2buf(buf, len, &bufptr, input_name, input_length);
        char2buf(buf, len, &bufptr, '?');
    } else if (level == -1) {
        string2buf(buf, len, &bufptr, "\204\200", 2);
    } else if (flags.f.big_stack) {
        bufptr = int2string(level + 1, buf, len);
        char2buf(buf, len, &bufptr, '\200');
    } else {
        char2buf(buf, len, &bufptr, "x\201z\203"[level]);
        char2buf(buf, len, &bufptr, '\200');
    }
    if (level == -1)
        bufptr += vartype2string(lastx, buf + bufptr, len - bufptr);
    else if (level <= sp)
        bufptr += vartype2string(stack[sp - level], buf + bufptr, len - bufptr);
    if (bufptr > disp_c) {
        buf[disp_c - 1] = 26;
        bufptr = disp_c;
    }
    draw_string(0, row, buf, bufptr);
}

static void full_list_to_string(vartype *v, std::string *buf, int maxlen) {
    *buf += "{ ";
    if (buf->length() >= maxlen)
        return;
    vartype_list *list = (vartype_list *) v;
    for (int i = 0; i < list->size; i++) {
        vartype *v2 = list->array->data[i];
        if (v2->type == TYPE_LIST) {
            full_list_to_string(v2, buf, maxlen);
        } else if (v2->type == TYPE_STRING || v2->type == TYPE_EQUATION) {
            const char *text;
            int length;
            char delim;
            if (v2->type == TYPE_STRING) {
                vartype_string *s = (vartype_string *) v2;
                text = s->txt();
                length = s->length;
                delim = '"';
            } else {
                vartype_equation *eq = (vartype_equation *) v2;
                text = eq->data->text;
                length = eq->data->length;
                delim = eq->data->compatMode ? '`' : '\'';
            }
            *buf += delim;
            *buf += std::string(text, length);
            *buf += delim;
            *buf += " ";
        } else {
            char b[100];
            int len = vartype2string(v2, b, 100);
            *buf += std::string(b, len);
            *buf += " ";
        }
        if (buf->length() >= maxlen)
            return;
    }
    *buf += "} ";
}

static void full_real_matrix_to_string(vartype *v, std::string *buf, int lines_available) {
    vartype_realmatrix *rm = (vartype_realmatrix *) v;
    int rows = rm->rows;
    int cols = rm->columns;
    int lines = rows < lines_available ? rows : lines_available;
    int line_end = 0;
    for (int r = 0; r < lines; r++) {
        *buf += r == 0 ? "[[" : " [";
        line_end += disp_c;
        int n = r * cols;
        for (int c = 0; c < cols && buf->length() < line_end; c++) {
            *buf += " ";
            if (rm->array->is_string[n] == 0) {
                phloat p = rm->array->data[n];
                char b[50];
                int len = easy_phloat2string(p, b, 50, 0);
                *buf += std::string(b, len);
            } else {
                const char *text;
                int4 length;
                get_matrix_string(rm, n, &text, &length);
                *buf += "\"";
                *buf += std::string(text, length);
                *buf += "\"";
            }
            n++;
        }
        *buf += r == rows - 1 ? " ]]" : " ]";
        if (buf->length() > line_end) {
            buf->erase(line_end - 1);
            *buf += "\32";
        } else {
            while (buf->length() < line_end)
                *buf += " ";
        }
    }
}

static void full_complex_matrix_to_string(vartype *v, std::string *buf, int lines_available) {
    vartype_complexmatrix *cm = (vartype_complexmatrix *) v;
    int rows = ((vartype_realmatrix *) v)->rows;
    int cols = ((vartype_realmatrix *) v)->columns;
    int lines = rows < lines_available ? rows : lines_available;
    int line_end = 0;
    vartype_complex cplx;
    cplx.type = TYPE_COMPLEX;
    for (int r = 0; r < lines; r++) {
        *buf += r == 0 ? "[[" : " [";
        line_end += disp_c;
        int n = r * cols * 2;
        for (int c = 0; c < cols && buf->length() < line_end; c++) {
            *buf += " ";
            cplx.re = cm->array->data[n++];
            cplx.im = cm->array->data[n++];
            char b[100];
            int len = vartype2string((vartype *) &cplx, b, 100);
            *buf += std::string(b, len);
        }
        *buf += r == rows - 1 ? " ]]" : " ]";
        if (buf->length() > line_end) {
            buf->erase(line_end - 1);
            *buf += "\32";
        } else {
            while (buf->length() < line_end)
                *buf += " ";
        }
    }
}

static int display_x(int row, int lines_available) {
    if ((disp_r == 2 || !mode_multi_line) && !mode_number_entry) {
        display_level(0, row);
        return 1;
    }

    try {
        std::string line;
        char buf[100];

        if (matedit_mode == 2 || matedit_mode == 3) {
            int len;
            for (int i = 0; i < matedit_stack_depth; i++) {
                len = int2string(matedit_stack[i].coord + 1, buf, 100);
                line += std::string(buf, len);
                line += '.';
            }
            if (matedit_is_list) {
                vartype *m;
                int err = matedit_get(&m);
                if (err != ERR_NONE || ((vartype_list *) m)->size == 0) {
                    line += 'E';
                } else {
                    len = int2string(matedit_i + 1, buf, 100);
                    line += std::string(buf, len);
                }
            } else {
                len = int2string(matedit_i + 1, buf, 100);
                line += std::string(buf, len);
                line += ':';
                len = int2string(matedit_j + 1, buf, 100);
                line += std::string(buf, len);
            }
            line += '=';
        } else if (input_length > 0) {
            line = std::string(input_name, input_length);
            line += '?';
        } else if (flags.f.big_stack) {
            line = "1\200";
        } else {
            line = "x\200";
        }

        if (mode_number_entry) {
            line += std::string(cmdline, cmdline_length);
            line += '_';
            int maxlen = disp_r == 2 ? disp_c : lines_available * disp_c;
            if (line.length() > maxlen) {
                line = line.substr(line.length() - maxlen + 1);
                line = std::string("\32", 1) + line;
            }
        } else if (sp >= 0) {
            vartype *v = stack[sp];
            if (v->type == TYPE_STRING) {
                vartype_string *s = (vartype_string *) v;
                line += '"';
                line += std::string(s->txt(), s->length);
                line += '"';
            } else if (v->type == TYPE_EQUATION) {
                vartype_equation *eq = (vartype_equation *) v;
                char d = eq->data->compatMode ? '`' : '\'';
                line += d;
                line += std::string(eq->data->text, eq->data->length);
                line += d;
            } else if (v->type == TYPE_UNIT) {
                v->type = TYPE_REAL;
                int len = vartype2string(v, buf, 100);
                line += std::string(buf, len);
                v->type = TYPE_UNIT;
                vartype_unit *u = (vartype_unit *) v;
                line += '_';
                line += std::string(u->text, u->length);
            } else if (v->type == TYPE_LIST) {
                int maxlen = lines_available * disp_c;
                full_list_to_string(v, &line, maxlen + 2);
                line.erase(line.length() - 1);
                if (line.length() > maxlen) {
                    line.erase(maxlen - 1);
                    line += "\32";
                }
            } else if (v->type == TYPE_REALMATRIX) {
                full_real_matrix_to_string(v, &line, lines_available);
            } else if (v->type == TYPE_COMPLEXMATRIX) {
                full_complex_matrix_to_string(v, &line, lines_available);
            } else {
                int len = vartype2string(v, buf, 100);
                line += std::string(buf, len);
            }
            int maxlen = lines_available * disp_c;
            if (line.length() > maxlen) {
                line = line.substr(0, maxlen - 1);
                line += '\32';
            }
        }

        int lines = (line.length() + disp_c - 1) / disp_c;
        int pos = 0;
        const char *p = line.c_str();
        int len = line.length();
        for (int i = row + 1 - lines; i <= row; i++) {
            clear_row(i);
            int end = pos + disp_c;
            if (end > len)
                end = len;
            draw_string(0, i, p + pos, end - pos);
            pos = end;
        }

        return lines;

    } catch (std::bad_alloc &) {
        draw_string(0, row, "<Low Mem>", 9);
        return 1;
    }
}

static std::string get_incomplete_command() {
    try {
        std::string buf;
        const command_spec *cmd = &cmd_array[incomplete_command];

        if (flags.f.prgm_mode && (cmd->flags & FLAG_IMMED) == 0) {
            int line = pc2line(pc);
            if (line < 10)
                buf += '0';
            char numbuf[10];
            int len = int2string(line, numbuf, 10);
            buf += std::string(numbuf, len);
            buf += (char) (!current_prgm.is_editable() ? 134 : current_prgm.is_locked() ? 135 : 6);
        }

        if (incomplete_command == CMD_ASSIGNb) {
            buf += "ASSIGN \"";
            buf += std::string(pending_command_arg.val.text, pending_command_arg.length);
            buf += "\" TO _";
            goto done;
        }

        if (incomplete_argtype == ARG_MKEY) {
            /* KEYG/KEYX */
            buf += "KEY _";
            goto done;
        }

        if (incomplete_command == CMD_SIMQ)
            buf += "Number of Unknowns ";
        else {
            buf += std::string(cmd->name, cmd->name_length);
            buf += ' ';
        }

        if (incomplete_ind)
            buf += "IND ";
        if (incomplete_alpha) {
            buf += '"';
            buf += std::string(incomplete_str, incomplete_length);
            buf += '_';
        } else {
            int d = 1;
            int i;
            for (i = 0; i < incomplete_length - 1; i++)
                d *= 10;
            for (i = 0; i < incomplete_maxdigits; i++) {
                if (i < incomplete_length) {
                    buf += (char) ('0' + (incomplete_num / d) % 10);
                    d /= 10;
                } else
                    buf += '_';
            }
        }

        done:
        return buf;

    } catch (std::bad_alloc &) {
        return std::string("<Low Mem>", 9);
    }
}

static int display_incomplete_command(int row, int available_lines) {
    std::string buf = get_incomplete_command();
    int maxlen = disp_c * available_lines;
    if (buf.length() > maxlen)
        buf = std::string("\32", 1) + buf.substr(buf.length() - maxlen + 1);
    int lines = (buf.length() + disp_c - 1) / disp_c;
    int pos = 0;
    const char *p = buf.c_str();
    for (int i = 0; i < lines; i++) {
        int len = buf.length() - pos;
        if (len > disp_c)
            len = disp_c;
        int r = row - lines + i + 1;
        clear_row(r);
        draw_string(0, r, p + pos, len);
        pos += len;
    }
    return lines;
}

void display_error(int error) {
    clear_row(0);
    int err_len;
    const char *err_text;
    if (error == -1) {
        err_len = lasterr_length;
        err_text = lasterr_text;
    } else {
        err_len = errors[error].length;
        err_text = errors[error].text;
    }
    draw_message(0, err_text, err_len, false);
    if (!flags.f.prgm_mode && (flags.f.trace_print || flags.f.normal_print) && flags.f.printer_exists)
        print_text(err_text, err_len, true);
}

int display_command(int row, int available_lines) {
    try {
        std::string buf;

        const command_spec *cmd = &cmd_array[pending_command];
        int catsect;
        int hide = pending_command == CMD_VMEXEC
                || pending_command == CMD_PMEXEC
                || (pending_command == CMD_XEQ
                    && xeq_invisible
                    && get_front_menu() == MENU_CATALOG
                    && ((catsect = get_cat_section()) == CATSECT_PGM
                        || catsect == CATSECT_PGM_ONLY));

        if (pending_command >= CMD_ASGN01 && pending_command <= CMD_ASGN18)
            buf += "ASSIGN ";
        else if (!hide) {
            if (pending_command == CMD_SIMQ)
                buf += "Number of Unknowns ";
            else {
                buf += std::string(cmd->name, cmd->name_length);
                buf += ' ';
            }
        }

        if (cmd->argtype == ARG_NONE)
            goto done;

        if (pending_command_arg.type == ARGTYPE_IND_NUM
                || pending_command_arg.type == ARGTYPE_IND_STK
                || pending_command_arg.type == ARGTYPE_IND_STR)
            buf += "IND ";

        if (pending_command_arg.type == ARGTYPE_NUM
                || pending_command_arg.type == ARGTYPE_IND_NUM) {
            int d = 1, i;
            int leadingzero = 1;
            for (i = 0; i < pending_command_arg.length - 1; i++)
                d *= 10;
            for (i = 0; i < pending_command_arg.length; i++) {
                int digit = (pending_command_arg.val.num / d) % 10;
                if (digit != 0 || i >= pending_command_arg.length - 2)
                    leadingzero = 0;
                if (!leadingzero)
                    buf += (char) ('0' + digit);
                d /= 10;
            }
        } else if (pending_command_arg.type == ARGTYPE_STK
                || pending_command_arg.type == ARGTYPE_IND_STK) {
            buf += "ST ";
            buf += pending_command_arg.val.stk;
        } else if (pending_command_arg.type == ARGTYPE_STR
                || pending_command_arg.type == ARGTYPE_IND_STR) {
            buf += '"';
            buf += std::string(pending_command_arg.val.text,
                               pending_command_arg.length);
            buf += '"';
        } else if (pending_command_arg.type == ARGTYPE_LBLINDEX) {
            int labelindex = pending_command_arg.val.num;
            directory *dir = get_dir(pending_command_arg.target);
            if (dir->labels[labelindex].length == 0)
                if (labelindex == dir->labels_count - 1)
                    buf += ".END.";
                else
                    buf += "END";
            else {
                buf += '"';
                buf += std::string(dir->labels[labelindex].name,
                                   dir->labels[labelindex].length);
                buf += '"';
            }
        } else if (pending_command_arg.type == ARGTYPE_XSTR) {
            buf += '"';
            buf += std::string(pending_command_arg.val.xstr,
                               pending_command_arg.length);
            buf += '"';
        } else if (pending_command_arg.type == ARGTYPE_EQN) {
            equation_data *eqd = eq_dir->prgms[pending_command_arg.val.num].eq_data;
            buf += eqd->compatMode ? '`' : '\'';
            buf += std::string(eqd->text, eqd->length);
            buf += eqd->compatMode ? '`' : '\'';
        } else /* ARGTYPE_LCLBL */ {
            buf += pending_command_arg.val.lclbl;
        }

        if (pending_command >= CMD_ASGN01 && pending_command <= CMD_ASGN18) {
            int keynum = pending_command - CMD_ASGN01 + 1;
            buf += " TO ";
            buf += (char) ('0' + keynum / 10);
            buf += (char) ('0' + keynum % 10);
        }

        done:
        int maxlen = disp_c * available_lines;
        if (buf.length() > maxlen)
            buf = buf.substr(0, maxlen - 1) + std::string("\32", 1);
        int lines = (buf.length() + disp_c - 1) / disp_c;
        int pos = 0;
        const char *p = buf.c_str();
        for (int i = 0; i < lines; i++) {
            int len = buf.length() - pos;
            if (len > disp_c)
                len = disp_c;
            int r = row == 0 ? i : row - lines + i + 1;
            clear_row(r);
            draw_string(0, r, p + pos, len);
            pos += len;
        }
        return lines;
    } catch (std::bad_alloc &) {
        draw_string(0, row, "<Low Mem>", 9);
        return 1;
    }
}

int appmenu_exitcallback_1(int menuid, bool exitall);
int appmenu_exitcallback_2(int menuid, bool exitall);
int appmenu_exitcallback_3(int menuid, bool exitall);
int appmenu_exitcallback_4(int menuid, bool exitall);
int appmenu_exitcallback_5(int menuid, bool exitall);
int appmenu_exitcallback_6(int menuid, bool exitall);
int appmenu_exitcallback_7(int menuid, bool exitall);
int appmenu_exitcallback_8(int menuid, bool exitall);

static int set_appmenu(int menuid, bool exitall) {
    if (mode_appmenu != MENU_NONE && appmenu_exitcallback != 0) {
        /* We delegate the set_menu() call to the callback,
         * but only once. If the callback wants to stay active,
         * it will have to call set_appmenu_callback() itself
         * to reinstate itself.
         */
        int cb = appmenu_exitcallback;
        appmenu_exitcallback = 0;
        /* NOTE: I don't use a 'traditional' callback pointer
         * because appmenu_exitcallback has to be persistable,
         * and pointers to code do not have that property.
         */
        switch (cb) {
            case 1: return appmenu_exitcallback_1(menuid, exitall);
            case 2: return appmenu_exitcallback_2(menuid, exitall);
            case 3: return appmenu_exitcallback_3(menuid, exitall);
            case 4: return appmenu_exitcallback_4(menuid, exitall);
            case 5: return appmenu_exitcallback_5(menuid, exitall);
            case 6: return appmenu_exitcallback_6(menuid, exitall);
            case 7: return appmenu_exitcallback_7(menuid, exitall);
            case 8: return appmenu_exitcallback_8(menuid, exitall);
            default: return ERR_INTERNAL_ERROR;
        }
    } else {
        mode_appmenu = menuid;
        appmenu_exitcallback = 0;
        return ERR_NONE;
    }
}

int start_varmenu_lbl(const char *name, int len, int role) {
    pgm_index saved_prgm = current_prgm;
    pgm_index prgm;
    int4 pc;
    int command;
    arg_struct arg, arg2;

    arg.type = ARGTYPE_STR;
    string_copy(arg.val.text, &arg.length, name, len);
    if (!find_global_label(&arg, &prgm, &pc))
        return ERR_LABEL_NOT_FOUND;
    pc += get_command_length(prgm, pc);
    current_prgm = prgm;
    get_next_command(&pc, &command, &arg2, 0, NULL);
    current_prgm = saved_prgm;
    if (command != CMD_MVAR)
        return ERR_NO_MENU_VARIABLES;
    config_varmenu_lbl(arg.val.text, arg.length);
    varmenu_row = 0;
    varmenu_role = role;
    return set_menu_return_err(MENULEVEL_APP, MENU_VARMENU, false);
}

int start_varmenu_eqn(vartype *eq, int role) {
    equation_data *eqd = ((vartype_equation *) eq)->data;
    if (!has_parameters(eqd))
        return ERR_NO_MENU_VARIABLES;
    mode_varmenu_whence = CATSECT_TOP;
    config_varmenu_eqn(eq);
    varmenu_row = 0;
    varmenu_role = role;
    return set_menu_return_err(MENULEVEL_APP, MENU_VARMENU, false);
}

void config_varmenu_lbl(const char *name, int len) {
    string_copy(varmenu, &varmenu_length, name, len);
    free_vartype(varmenu_eqn);
    varmenu_eqn = NULL;
}

void config_varmenu_eqn(vartype *eq) {
    free_vartype(varmenu_eqn);
    varmenu_eqn = dup_vartype(eq);
}

void config_varmenu_none() {
    varmenu_length = 0;
    free_vartype(varmenu_eqn);
    varmenu_eqn = NULL;
}

void draw_varmenu() {
    if (mode_appmenu != MENU_VARMENU)
        return;

    if (varmenu_eqn != NULL) {
        char ktext[6][7];
        int klen[6];
        int need_eval = varmenu_role == 1 && !is_equation(varmenu_eqn) ? 1 : varmenu_role >= 4 && varmenu_role <= 6 ? 2 : 0;
        get_varmenu_row_for_eqn(varmenu_eqn, need_eval, &varmenu_rows, &varmenu_row, ktext, klen);
        set_annunciators(varmenu_rows > 1, -1, -1, -1, -1, -1);
        int black, total;
        num_parameters(varmenu_eqn, &black, &total);
        for (int i = 0; i < 6; i++) {
            string_copy(varmenu_labeltext[i], &varmenu_labellength[i], ktext[i], klen[i]);
            bool invert = false;
            if (need_eval != 0 && varmenu_row == 0 && i == 0) {
                invert = true;
            } else {
                int n = varmenu_row * 6 + i - (need_eval != 0);
                invert = n >= black && n < total;
            }
            draw_key(i, 0, 0, ktext[i], klen[i], invert);
        }
    } else {
        arg_struct arg;
        pgm_index saved_prgm, prgm;
        int4 pc, pc2;
        int command, i, row, key;
        int num_mvars = 0;

        arg.type = ARGTYPE_STR;
        arg.length = varmenu_length;
        for (i = 0; i < arg.length; i++)
            arg.val.text[i] = varmenu[i];
        if (!find_global_label(&arg, &prgm, &pc)) {
            set_appmenu(MENU_NONE, false);
            config_varmenu_none();
            return;
        }
        saved_prgm = current_prgm;
        current_prgm = prgm;
        pc += get_command_length(prgm, pc);
        pc2 = pc;
        while (get_next_command(&pc, &command, &arg, 0, NULL), command == CMD_MVAR)
            num_mvars++;
        if (num_mvars == 0) {
            current_prgm = saved_prgm;
            set_appmenu(MENU_NONE, false);
            config_varmenu_none();
            return;
        }

        bool need_eval = varmenu_role >= 4 && varmenu_role <= 6;
        if (need_eval)
            num_mvars++;

        varmenu_rows = (num_mvars + 5) / 6;
        if (varmenu_row >= varmenu_rows)
            varmenu_row = varmenu_rows - 1;
        set_annunciators(varmenu_rows > 1, -1, -1, -1, -1, -1);

        row = 0;
        key = 0;
        if (need_eval) {
            if (varmenu_row == 0)
                draw_key(key++, 0, 0, "STK", 3, true);
        }

        while (get_next_command(&pc2, &command, &arg, 0, NULL), command == CMD_MVAR) {
            if (row == varmenu_row) {
                string_copy(varmenu_labeltext[key], &varmenu_labellength[key], arg.val.text, arg.length);
                draw_key(key, 0, 0, arg.val.text, arg.length);
            }
            if (key++ == 5) {
                if (row++ == varmenu_row)
                    break;
                else
                    key = 0;
            }
        }
        current_prgm = saved_prgm;
        while (key < 6) {
            varmenu_labellength[key] = 0;
            draw_key(key, 0, 0, "", 0);
            key++;
        }
    }
}

static int fcn_cat[] = {
    CMD_ABS,      CMD_ACOS,      CMD_ACOSH,    CMD_ADV,        CMD_AGRAPH,  CMD_AIP,
    CMD_ALENG,    CMD_ALL,       CMD_ALLSIGMA, CMD_AND,        CMD_AOFF,    CMD_AON,
    CMD_ARCL,     CMD_AROT,      CMD_ASHF,     CMD_ASIN,       CMD_ASINH,   CMD_ASSIGNa,
    CMD_ASTO,     CMD_ATAN,      CMD_ATANH,    CMD_ATOX,       CMD_AVIEW,   CMD_BASEADD,
    CMD_BASESUB,  CMD_BASEMUL,   CMD_BASEDIV,  CMD_BASECHS,    CMD_BEEP,    CMD_BEST,
    CMD_BINM,     CMD_BIT_T,     CMD_BST,      CMD_CF,         CMD_CLA,     CMD_CLALLa,
    CMD_CLD,      CMD_CLKEYS,    CMD_CLLCD,    CMD_CLMENU,     CMD_CLP,     CMD_CLRG,
    CMD_CLST,     CMD_CLV,       CMD_CLX,      CMD_CLSIGMA,    CMD_COMB,    CMD_COMPLEX,
    CMD_CORR,     CMD_COS,       CMD_COSH,     CMD_CPXRES,     CMD_CPX_T,   CMD_CROSS,
    CMD_CUSTOM,   CMD_DECM,      CMD_DEG,      CMD_DEL,        CMD_DELAY,   CMD_DELR,
    CMD_DET,      CMD_DIM,       CMD_DIM_T,    CMD_DOT,        CMD_DSE,     CMD_EDIT,
    CMD_EDITN,    CMD_END,       CMD_ENG,      CMD_ENTER,      CMD_EXITALL, CMD_EXPF,
    CMD_E_POW_X,  CMD_E_POW_X_1, CMD_FC_T,     CMD_FCC_T,      CMD_FCSTX,   CMD_FCSTY,
    CMD_FIX,      CMD_FNRM,      CMD_FP,       CMD_FS_T,       CMD_FSC_T,   CMD_GAMMA,
    CMD_GETKEY,   CMD_GETM,      CMD_GRAD,     CMD_GROW,       CMD_GTO,     CMD_HEXM,
    CMD_HMSADD,   CMD_HMSSUB,    CMD_I_ADD,    CMD_I_SUB,      CMD_INDEX,   CMD_INPUT,
    CMD_INSR,     CMD_INTEG,     CMD_INVRT,    CMD_IP,         CMD_ISG,     CMD_J_ADD,
    CMD_J_SUB,    CMD_KEYASN,    CMD_KEYG,     CMD_KEYX,       CMD_LASTX,   CMD_LBL,
    CMD_LCLBL,    CMD_LINF,      CMD_LINSIGMA, CMD_LIST,       CMD_LN,      CMD_LN_1_X,
    CMD_LOG,      CMD_LOGF,      CMD_MAN,      CMD_MAT_T,      CMD_MEAN,    CMD_MENU,
    CMD_MOD,      CMD_MVAR,      CMD_FACT,     CMD_NEWMAT,     CMD_NORM,    CMD_NOT,
    CMD_OCTM,     CMD_OFF,       CMD_OLD,      CMD_ON,         CMD_OR,      CMD_PERM,
    CMD_PGMINT,   CMD_PGMSLV,    CMD_PI,       CMD_PIXEL,      CMD_POLAR,   CMD_POSA,
    CMD_PRA,      CMD_PRLCD,     CMD_POFF,     CMD_PROMPT,     CMD_PON,     CMD_PRP,
    CMD_PRSTK,    CMD_PRUSR,     CMD_PRV,      CMD_PRX,        CMD_PRSIGMA, CMD_PSE,
    CMD_PUTM,     CMD_PWRF,      CMD_QUIET,    CMD_RAD,        CMD_RAN,     CMD_RCL,
    CMD_RCL_ADD,  CMD_RCL_SUB,   CMD_RCL_MUL,  CMD_RCL_DIV,    CMD_RCLEL,   CMD_RCLIJ,
    CMD_RDXCOMMA, CMD_RDXDOT,    CMD_REALRES,  CMD_REAL_T,     CMD_RECT,    CMD_RND,
    CMD_RNRM,     CMD_ROTXY,     CMD_RSUM,     CMD_RTN,        CMD_SWAP_R,  CMD_RUP,
    CMD_RDN,      CMD_SCI,       CMD_SDEV,     CMD_SEED,       CMD_SF,      CMD_SIGN,
    CMD_SIN,      CMD_SINH,      CMD_SIZE,     CMD_SLOPE,      CMD_SOLVE,   CMD_SQRT,
    CMD_SST,      CMD_STO,       CMD_STO_ADD,  CMD_STO_SUB,    CMD_STO_MUL, CMD_STO_DIV,
    CMD_STOEL,    CMD_STOIJ,     CMD_STOP,     CMD_STR_T,      CMD_SUM,     CMD_TAN,
    CMD_TANH,     CMD_TONE,      CMD_TRACE,    CMD_TRANS,      CMD_UVEC,    CMD_VARMENU,
    CMD_VIEW,     CMD_WMEAN,     CMD_WRAP,     CMD_X_SWAP,     CMD_SWAP,    CMD_X_LT_0,
    CMD_X_LT_Y,   CMD_X_LE_0,    CMD_X_LE_Y,   CMD_X_EQ_0,     CMD_X_EQ_Y,  CMD_X_NE_0,
    CMD_X_NE_Y,   CMD_X_GT_0,    CMD_X_GT_Y,   CMD_X_GE_0,     CMD_X_GE_Y,  CMD_XEQ,
    CMD_XOR,      CMD_XTOA,      CMD_SQUARE,   CMD_YINT,       CMD_Y_POW_X, CMD_INV,
    CMD_10_POW_X, CMD_ADD,       CMD_SUB,      CMD_MUL,        CMD_DIV,     CMD_CHS,
    CMD_SIGMAADD, CMD_SIGMASUB,  CMD_SIGMAREG, CMD_SIGMAREG_T, CMD_TO_DEC,  CMD_TO_DEG,
    CMD_TO_HMS,   CMD_TO_HR,     CMD_TO_OCT,   CMD_TO_POL,     CMD_TO_RAD,  CMD_TO_REC,
    CMD_LEFT,     CMD_UP,        CMD_DOWN,     CMD_RIGHT,      CMD_PERCENT, CMD_PERCENT_CH,
    CMD_FIND,     CMD_MAX,       CMD_MIN,      CMD_NULL,       CMD_NULL,    CMD_NULL
};

static int ext_time_cat[] = {
    CMD_ADATE,     CMD_ATIME, CMD_ATIME24, CMD_CLK12, CMD_CLK24, CMD_DATE,
    CMD_DATE_PLUS, CMD_DDAYS, CMD_DMY,     CMD_DOW,   CMD_MDY,   CMD_TIME,
    CMD_YMD,       CMD_NULL,  CMD_NULL,    CMD_NULL,  CMD_NULL,  CMD_NULL
};

static int ext_xfcn_cat[] = {
    CMD_ANUM, CMD_RCLFLAG, CMD_STOFLAG, CMD_X_SWAP_F, CMD_NULL, CMD_NULL
};

static int ext_base_cat[] = {
    CMD_BRESET, CMD_BSIGNED, CMD_BWRAP, CMD_WSIZE, CMD_WSIZE_T, CMD_A_THRU_F_2
};

static int ext_prgm_cat[] = {
    CMD_CPXMAT_T, CMD_CSLD_T, CMD_ERRMSG,  CMD_ERRNO,   CMD_FUNC,    CMD_GETKEY1,
    CMD_GETKEYA,  CMD_LSTO,   CMD_LASTO,   CMD_LCLV,    CMD_NOP,     CMD_PGMMENU,
    CMD_PGMVAR,   CMD_RTNERR, CMD_RTNNO,   CMD_RTNYES,  CMD_SKIP,    CMD_SST_UP,
    CMD_SST_RT,   CMD_TYPE_T, CMD_VARMNU1, -2 /* 0? */, -3 /* X? */, CMD_NULL
};

static int ext_str_cat[] = {
    CMD_APPEND,    CMD_C_TO_N, CMD_EXTEND, CMD_HEAD,    CMD_LENGTH, CMD_TO_LIST,
    CMD_FROM_LIST, CMD_LIST_T, CMD_LXASTO, CMD_NEWLIST, CMD_N_TO_C, CMD_N_TO_S,
    CMD_NN_TO_S,   CMD_POS,    CMD_REV,    CMD_SUBSTR,  CMD_S_TO_N, CMD_XASTO,
    CMD_XSTR,      CMD_XVIEW,  CMD_NULL,   CMD_NULL,    CMD_NULL,   CMD_NULL
};

static int ext_stk_cat[] = {
    CMD_4STK,   CMD_DEPTH, CMD_DROP, CMD_DROPN, CMD_DUP,  CMD_DUPN,
    CMD_L4STK,  CMD_LNSTK, CMD_NSTK, CMD_PICK,  CMD_RUPN, CMD_RDNN,
    CMD_UNPICK, CMD_NULL,  CMD_NULL, CMD_NULL,  CMD_NULL, CMD_NULL
};

static int ext_eqn_cat[] = {
    CMD_COMP,    CMD_DIRECT, CMD_EDITEQN, CMD_EQN_T,   CMD_EQNINT, CMD_EQNMENU,
    CMD_EQNMNU1, CMD_EQNSLV, CMD_EQNVAR,  CMD_EVAL,    CMD_EVALN,  CMD_NEWEQN,
    CMD_NUMERIC, CMD_PARSE,  CMD_STD,     CMD_UNPARSE, CMD_NULL,   CMD_NULL
};

static int ext_unit_cat[] = {
    CMD_CONVERT, CMD_UBASE, CMD_UFACT, CMD_UVAL, CMD_TO_UNIT, CMD_FROM_UNIT,
    CMD_UNIT_T,  CMD_NULL,  CMD_NULL,  CMD_NULL, CMD_NULL,    CMD_NULL
};

static int ext_stat_cat[] = {
    CMD_SX,    CMD_SX2,   CMD_SY,   CMD_SY2,   CMD_SXY,     CMD_SN,
    CMD_SLNX,  CMD_SLNX2, CMD_SLNY, CMD_SLNY2, CMD_SLNXLNY, CMD_SXLNY,
    CMD_SYLNX, CMD_NULL,  CMD_NULL, CMD_NULL,  CMD_NULL,    CMD_NULL
};

static int ext_dir_cat[] = {
    CMD_CHDIR,   CMD_CRDIR,   CMD_HOME,    CMD_PATH,   CMD_PGDIR, CMD_PRALL,
    CMD_REFCOPY, CMD_REFFIND, CMD_REFMOVE, CMD_RENAME, CMD_UPDIR, CMD_NULL
};

static int ext_disp_cat[] = {
    CMD_ATOP,   CMD_COL_PLUS, CMD_COL_MINUS, CMD_GETDS, CMD_HEADER,   CMD_HEIGHT,
    CMD_HFLAGS, CMD_HPOLAR,   CMD_LTOP,      CMD_NLINE, CMD_ROW_PLUS, CMD_ROW_MINUS,
    CMD_SETDS,  CMD_WIDTH,    CMD_1LINE,     CMD_NULL,  CMD_NULL,     CMD_NULL
};

#if defined(ANDROID) || defined(IPHONE)
#ifdef FREE42_FPTEST
static int ext_misc_cat[] = {
    CMD_A2LINE, CMD_A2PLINE, CMD_C_LN_1_X, CMD_C_E_POW_X_1, CMD_FMA,     CMD_GETLI,
    CMD_GETMI,  CMD_IDENT,   CMD_LINE,     CMD_LOCK,        CMD_PCOMPLX, CMD_PLOT_M,
    CMD_PRREG,  CMD_PUTLI,   CMD_PUTMI,    CMD_RCOMPLX,     CMD_SPFV,    CMD_SPPV,
    CMD_STRACE, CMD_TVM,     CMD_UNLOCK,   CMD_USFV,        CMD_USPV,    CMD_X2LINE,
    CMD_ACCEL,  CMD_LOCAT,   CMD_HEADING,  CMD_FPTEST,      CMD_NULL,    CMD_NULL
};
#define MISC_CAT_ROWS 5
#else
static int ext_misc_cat[] = {
    CMD_A2LINE, CMD_A2PLINE, CMD_C_LN_1_X, CMD_C_E_POW_X_1, CMD_FMA,     CMD_GETLI,
    CMD_GETMI,  CMD_IDENT,   CMD_LINE,     CMD_LOCK,        CMD_PCOMPLX, CMD_PLOT_M,
    CMD_PRREG,  CMD_PUTLI,   CMD_PUTMI,    CMD_RCOMPLX,     CMD_SPFV,    CMD_SPPV,
    CMD_STRACE, CMD_TVM,     CMD_UNLOCK,   CMD_USFV,        CMD_USPV,    CMD_X2LINE,
    CMD_ACCEL,  CMD_LOCAT,   CMD_HEADING,  CMD_NULL,        CMD_NULL,    CMD_NULL
};
#define MISC_CAT_ROWS 5
#endif
#else
#ifdef FREE42_FPTEST
static int ext_misc_cat[] = {
    CMD_A2LINE, CMD_A2PLINE, CMD_C_LN_1_X, CMD_C_E_POW_X_1, CMD_FMA,     CMD_GETLI,
    CMD_GETMI,  CMD_IDENT,   CMD_LINE,     CMD_LOCK,        CMD_PCOMPLX, CMD_PLOT_M,
    CMD_PRREG,  CMD_PUTLI,   CMD_PUTMI,    CMD_RCOMPLX,     CMD_SPFV,    CMD_SPPV,
    CMD_STRACE, CMD_TVM,     CMD_UNLOCK,   CMD_USFV,        CMD_USPV,    CMD_X2LINE,
    CMD_FPTEST, CMD_NULL,    CMD_NULL,     CMD_NULL,        CMD_NULL,    CMD_NULL
};
#define MISC_CAT_ROWS 5
#else
static int ext_misc_cat[] = {
    CMD_A2LINE, CMD_A2PLINE, CMD_C_LN_1_X, CMD_C_E_POW_X_1, CMD_FMA,     CMD_GETLI,
    CMD_GETMI,  CMD_IDENT,   CMD_LINE,     CMD_LOCK,        CMD_PCOMPLX, CMD_PLOT_M,
    CMD_PRREG,  CMD_PUTLI,   CMD_PUTMI,    CMD_RCOMPLX,     CMD_SPFV,    CMD_SPPV,
    CMD_STRACE, CMD_TVM,     CMD_UNLOCK,   CMD_USFV,        CMD_USPV,    CMD_X2LINE
};
#define MISC_CAT_ROWS 4
#endif
#endif

static int ext_0_cmp_cat[] = {
    CMD_0_EQ_NN, CMD_0_NE_NN, CMD_0_LT_NN, CMD_0_GT_NN, CMD_0_LE_NN, CMD_0_GE_NN
};

static int ext_x_cmp_cat[] = {
    CMD_X_EQ_NN, CMD_X_NE_NN, CMD_X_LT_NN, CMD_X_GT_NN, CMD_X_LE_NN, CMD_X_GE_NN
};

bool show_nonlocal_vars(int catsect) {
    if (catsect != CATSECT_REAL_ONLY
            && catsect != CATSECT_MAT_ONLY
            && catsect != CATSECT_MAT_LIST_ONLY
            && catsect != CATSECT_EQN_ONLY
            && catsect != CATSECT_VARS_ONLY
            && catsect != CATSECT_LIST_STR_ONLY)
        return false;
    if (incomplete_ind)
        return catsect == CATSECT_REAL_ONLY;
    return incomplete_command != CMD_STO
            && incomplete_command != CMD_STO_ADD
            && incomplete_command != CMD_STO_SUB
            && incomplete_command != CMD_STO_MUL
            && incomplete_command != CMD_STO_DIV
            && incomplete_command != CMD_LSTO
            && incomplete_command != CMD_ASTO
            && incomplete_command != CMD_LASTO
            && incomplete_command != CMD_XASTO
            && incomplete_command != CMD_LXASTO
            && incomplete_command != CMD_GSTO
            && incomplete_command != CMD_ISG
            && incomplete_command != CMD_DSE
            && incomplete_command != CMD_HEAD
            && incomplete_command != CMD_X_SWAP
            && incomplete_command != CMD_CLV
            && incomplete_command != CMD_LCLV
            && incomplete_command != CMD_INDEX
            && incomplete_command != CMD_EDITN
            && incomplete_command != CMD_DIM;
}

static void draw_catalog() {
    int catsect = get_cat_section();
    int catindex = get_cat_index();
    if (catsect == CATSECT_TOP) {
        draw_top:
        if ((skin_flags & 1) == 0)
            draw_key(0, 0, 0, "DIRS", 4);
        else
            draw_key(0, 0, 0, "FCN", 3);
        draw_key(1, 0, 0, "PGM", 3);
        draw_key(2, 0, 0, "REAL", 4);
        draw_key(3, 0, 0, "CPX", 3);
        draw_key(4, 0, 0, "MAT", 3);
        if ((skin_flags & 2) == 0)
            draw_key(5, 0, 0, "UNITS", 5);
        else
            draw_key(5, 0, 0, "MEM", 3);
        mode_updown = true;
        set_annunciators(1, -1, -1, -1, -1, -1);
    } else if (catsect == CATSECT_MORE) {
        draw_other:
        draw_key(0, 0, 0, "LIST", 4);
        draw_key(1, 0, 0, "EQN", 3);
        draw_key(2, 0, 0, "EQN", 3, true);
        draw_key(3, 0, 0, "OTHER", 5);
        if ((skin_flags & 1) == 0)
            draw_key(4, 0, 0, "FCN", 3);
        else
            draw_key(4, 0, 0, "DIRS", 4);
        if ((skin_flags & 2) == 0)
            draw_key(5, 0, 0, "MEM", 3);
        else
            draw_key(5, 0, 0, "UNITS", 5);
        mode_updown = true;
        set_annunciators(1, -1, -1, -1, -1, -1);
    } else if (catsect == CATSECT_EXT_1) {
        draw_key(0, 0, 0, "TIME", 4);
        draw_key(1, 0, 0, "XFCN", 4);
        draw_key(2, 0, 0, "BASE", 4);
        draw_key(3, 0, 0, "PRGM", 4);
        draw_key(4, 0, 0, "STR", 3);
        draw_key(5, 0, 0, "STK", 3);
        mode_updown = true;
        set_annunciators(1, -1, -1, -1, -1, -1);
    } else if (catsect == CATSECT_EXT_2) {
        draw_key(0, 0, 0, "EQNS", 4);
        draw_key(1, 0, 0, "UNIT", 4);
        draw_key(2, 0, 0, "STAT", 4);
        draw_key(3, 0, 0, "DIR", 3);
        draw_key(4, 0, 0, "DISP", 4);
        draw_key(5, 0, 0, "MISC", 4);
        mode_updown = true;
        set_annunciators(1, -1, -1, -1, -1, -1);
    } else if (catsect == CATSECT_PGM
            || catsect == CATSECT_PGM_ONLY
            || catsect == CATSECT_PGM_SOLVE
            || catsect == CATSECT_PGM_INTEG
            || catsect == CATSECT_PGM_MENU) {

        /* Show menu of alpha labels */

        bool show_nonlocal = catsect == CATSECT_PGM_ONLY
                            && (incomplete_command == CMD_GTO
                             || incomplete_command == CMD_XEQ
                             || incomplete_command == CMD_PRP
                             || incomplete_command == CMD_PGMINT
                             || incomplete_command == CMD_PGMSLV)
                            || catsect == CATSECT_PGM_SOLVE
                            || catsect == CATSECT_PGM_INTEG
                            || catsect == CATSECT_PGM_MENU;

        try {
            directory *dir = cwd;
            vartype_list *path = show_nonlocal ? get_path() : NULL;
            int path_index = -1;
            std::vector<std::string> names;
            std::vector<int4> dirs;
            std::vector<int> items;
            std::set<int4> past_dirs;
            int nlocal = 0;

            if (catsect == CATSECT_PGM_SOLVE
                    || catsect == CATSECT_PGM_INTEG
                    || catsect == CATSECT_PGM_MENU) {
                names.push_back(std::string("=", 1));
                dirs.push_back(0);
                items.push_back(-2);
                nlocal++;
            }

            do {
                past_dirs.insert(dir->id);
                if (catsect == CATSECT_PGM || catsect == CATSECT_PGM_ONLY) {
                    for (int i = dir->labels_count - 1; i >= 0; i--) {
                        if (dir->labels[i].length == 0 && i > 0 && dir->labels[i - 1].prgm == dir->labels[i].prgm)
                            continue;
                        if (dir->labels[i].length > 0)
                            names.push_back(std::string(dir->labels[i].name, dir->labels[i].length));
                        else if (i == dir->labels_count - 1)
                            names.push_back(std::string(".END.", 5));
                        else
                            names.push_back(std::string("END", 3));
                        dirs.push_back(dir->id);
                        items.push_back(i);
                        if (dir == cwd)
                            nlocal++;
                    }
                } else { /* CATSECT_PGM_SOLVE, CATSECT_PGM_INTEG, CATSECT_PGM_MENU */
                    directory *saved_cwd = cwd;
                    cwd = dir;
                    for (int i = dir->labels_count - 1; i >= 0; i--)
                        if (label_has_mvar(dir->id, i)) {
                            names.push_back(std::string(dir->labels[i].name, dir->labels[i].length));
                            dirs.push_back(dir->id);
                            items.push_back(i);
                            if (dir == saved_cwd)
                                nlocal++;
                        }
                    cwd = saved_cwd;
                }
                if (!show_nonlocal)
                    break;
                if (path_index == -1) {
                    dir = dir->parent;
                    if (dir == NULL) {
                        if (path != NULL) {
                            path_index = 0;
                            goto do_path_1;
                        }
                    }
                } else {
                    do_path_1:
                    dir = NULL;
                    while (path_index < path->size) {
                        vartype *v = path->array->data[path_index++];
                        if (v->type != TYPE_DIR_REF)
                            continue;
                        dir = get_dir(((vartype_dir_ref *) v)->dir);
                        if (dir == NULL)
                            continue;
                        if (past_dirs.find(dir->id) == past_dirs.end())
                            break;
                        dir = NULL;
                    }
                }
            } while (dir != NULL);

            catalogmenu_rows[catindex] = (names.size() + 5) / 6;
            if (catalogmenu_row[catindex] >= catalogmenu_rows[catindex])
                catalogmenu_row[catindex] = catalogmenu_rows[catindex] - 1;
            int row = catalogmenu_row[catindex];

            for (int k = 0; k < 6; k++) {
                int n = k + row * 6;
                if (n < names.size()) {
                    draw_key(k, 0, 0, names[n].c_str(), names[n].length(), items[n] == -2 || n >= nlocal);
                    catalogmenu_dir[catindex][k] = dirs[n];
                    catalogmenu_item[catindex][k] = items[n];
                } else {
                    draw_key(k, 0, 0, "", 0);
                    catalogmenu_item[catindex][k] = -1;
                }
            }

        } catch (std::bad_alloc &) {
            catalogmenu_rows[catindex] = 1;
            catalogmenu_row[catindex] = 0;
            mode_updown = 0;
            for (int k = 0; k < 6; k++) {
                draw_key(k, 0, 0, "", 0, true);
                catalogmenu_item[catindex][k] = -1;
            }
        }

        mode_updown = catalogmenu_rows[catindex] > 1;
        set_annunciators(mode_updown, -1, -1, -1, -1, -1);
    } else if (catsect == CATSECT_FCN
            || catsect >= CATSECT_EXT_TIME && catsect <= CATSECT_EXT_X_CMP) {
        int *subcat;
        int subcat_rows;
        switch (catsect) {
            case CATSECT_FCN: subcat = fcn_cat; subcat_rows = 43; break;
            case CATSECT_EXT_TIME: subcat = ext_time_cat; subcat_rows = 3; break;
            case CATSECT_EXT_XFCN: subcat = ext_xfcn_cat; subcat_rows = 1; break;
            case CATSECT_EXT_BASE: subcat = ext_base_cat; subcat_rows = 1; break;
            case CATSECT_EXT_PRGM: subcat = ext_prgm_cat; subcat_rows = 4; break;
            case CATSECT_EXT_STR: subcat = ext_str_cat; subcat_rows = 4; break;
            case CATSECT_EXT_STK: subcat = ext_stk_cat; subcat_rows = 3; break;
            case CATSECT_EXT_EQN: subcat = ext_eqn_cat; subcat_rows = 3; break;
            case CATSECT_EXT_UNIT: subcat = ext_unit_cat; subcat_rows = 2; break;
            case CATSECT_EXT_STAT: subcat = ext_stat_cat; subcat_rows = 3; break;
            case CATSECT_EXT_DIR: subcat = ext_dir_cat; subcat_rows = 2; break;
            case CATSECT_EXT_DISP: subcat = ext_disp_cat; subcat_rows = 3; break;
            case CATSECT_EXT_MISC: subcat = ext_misc_cat; subcat_rows = MISC_CAT_ROWS; break;
            case CATSECT_EXT_0_CMP: subcat = ext_0_cmp_cat; subcat_rows = 1; break;
            case CATSECT_EXT_X_CMP: subcat = ext_x_cmp_cat; subcat_rows = 1; break;
        }

        int desired_row = catalogmenu_row[catindex];
        if (desired_row >= subcat_rows)
            desired_row = 0;
        for (int i = 0; i < 6; i++) {
            int cmd = subcat[desired_row * 6 + i];
            catalogmenu_item[catindex][i] = cmd;
            if (cmd == -2)
                draw_key(i, 0, 1, "0?", 2);
            else if (cmd == -3)
                draw_key(i, 0, 1, "X?", 2);
            else
                draw_key(i, 0, 1, cmd_array[cmd].name,
                                  cmd_array[cmd].name_length);
        }
        catalogmenu_rows[catindex] = subcat_rows;
        mode_updown = subcat_rows > 1;
        set_annunciators(mode_updown ? 1 : 0, -1, -1, -1, -1, -1);
    } else if (catsect == CATSECT_EQN_NAMED) {
        std::vector<std::string> eqns;
        try {
            eqns = get_equation_names();
        } catch (std::bad_alloc &) {
            set_cat_section(CATSECT_MORE);
            display_error(ERR_INSUFFICIENT_MEMORY);
            goto draw_other;
        }
        int n = eqns.size();
        int rows = (n + 5) / 6;
        if (rows == 0) {
            set_cat_section(CATSECT_MORE);
            goto draw_other;
        }
        catalogmenu_rows[catindex] = rows;
        int row = catalogmenu_row[catindex];
        if (row >= rows)
            catalogmenu_row[catindex] = row = rows - 1;
        for (int i = 0; i < 6; i++) {
            int j = row * 6 + i;
            if (j < n) {
                std::string s = eqns[j];
                draw_key(i, 0, 0, s.c_str(), s.length());
            } else
                draw_key(i, 0, 0, "", 0);
        }
        mode_updown = rows > 1;
        set_annunciators(mode_updown ? 1 : 0, -1, -1, -1, -1, -1);
    } else if (catsect == CATSECT_UNITS_1) {
        draw_key(0, 0, 0, "LENG", 4);
        draw_key(1, 0, 0, "AREA", 4);
        draw_key(2, 0, 0, "VOL", 3);
        draw_key(3, 0, 0, "TIME", 4);
        draw_key(4, 0, 0, "SPEED", 5);
        draw_key(5, 0, 0, "MASS", 4);
        mode_updown = true;
        set_annunciators(1, -1, -1, -1, -1, -1);
    } else if (catsect == CATSECT_UNITS_2) {
        draw_key(0, 0, 0, "FORCE", 5);
        draw_key(1, 0, 0, "ENRG", 4);
        draw_key(2, 0, 0, "POWR", 4);
        draw_key(3, 0, 0, "PRESS", 5);
        draw_key(4, 0, 0, "TEMP", 4);
        draw_key(5, 0, 0, "ELEC", 4);
        mode_updown = true;
        set_annunciators(1, -1, -1, -1, -1, -1);
    } else if (catsect == CATSECT_UNITS_3) {
        draw_key(0, 0, 0, "ANGL", 4);
        draw_key(1, 0, 0, "LIGHT", 5);
        draw_key(2, 0, 0, "RAD", 3);
        draw_key(3, 0, 0, "VISC", 4);
        draw_key(4, 0, 0, "", 0);
        draw_key(5, 0, 0, "", 0);
        mode_updown = true;
        set_annunciators(1, -1, -1, -1, -1, -1);
    } else if (catsect >= CATSECT_UNITS_LENG && catsect <= CATSECT_UNITS_VISC) {
        const char *text[6];
        int length[6];
        get_units_cat_row(catsect, text, length, &catalogmenu_row[catindex], &catalogmenu_rows[catindex]);
        for (int i = 0; i < 6; i++)
            draw_key(i, 0, 0, text[i], length[i]);
        mode_updown = catalogmenu_rows[catindex] > 1;
        set_annunciators(mode_updown, -1, -1, -1, -1, -1);
    } else if (catsect == CATSECT_DIRS || catsect == CATSECT_DIRS_ONLY) {
        int up, lcount = 0, vcount;
        if (catsect == CATSECT_DIRS) {
            up = cwd != root;
            int lastprgm = -1;
            for (int i = 0; i < cwd->labels_count; i++) {
                if (cwd->labels[i].length > 0 || cwd->labels[i].prgm != lastprgm)
                    lcount++;
                lastprgm = cwd->labels[i].prgm;
            }
            vcount = cwd->vars_count;
        } else {
            up = 0;
            vcount = 0;
        }
        int rows = (up + cwd->children_count + lcount + vcount + 5) / 6;
        if (rows == 0)
            rows = 1;
        int row = catalogmenu_row[catindex];
        if (row >= rows)
            row = 0;
        catalogmenu_rows[catindex] = rows;
        catalogmenu_row[catindex] = row;
        for (int i = 0; i < 6; i++) {
            int p = row * 6 + i;
            if (p < up) {
                draw_key(i, 0, 0, "..", 2);
                continue;
            }
            p -= up;
            if (p < cwd->children_count) {
                draw_key(i, 0, 0, cwd->children[p].name, cwd->children[p].length);
                continue;
            }
            p -= cwd->children_count;
            if (p < lcount) {
                int lastprgm = -1;
                int l2count = lcount - 1;
                for (int j = 0; j < cwd->labels_count; j++) {
                    if (cwd->labels[j].length > 0 || cwd->labels[j].prgm != lastprgm) {
                        if (l2count == p) {
                            if (cwd->labels[j].length > 0)
                                draw_key(i, 0, 0, cwd->labels[j].name, cwd->labels[j].length, true);
                            else if (cwd->labels[j].prgm == cwd->prgms_count - 1)
                                draw_key(i, 0, 0, ".END.", 5, true);
                            else
                                draw_key(i, 0, 0, "END", 3, true);
                            goto label_done;
                        }
                        l2count--;
                        lastprgm = cwd->labels[j].prgm;
                    }
                }
                label_done:
                continue;
            }
            p -= lcount;
            if (p < vcount) {
                p = vcount - p - 1;
                draw_key(i, 0, 0, cwd->vars[p].name, cwd->vars[p].length);
                continue;
            }
            draw_key(i, 0, 0, "", 0);
        }
        mode_updown = rows > 1;
        set_annunciators(mode_updown, -1, -1, -1, -1, -1);
    } else {
        bool show_type[TYPE_SENTINEL];
        for (int i = 0; i < TYPE_SENTINEL; i++)
            show_type[i] = false;

        switch (catsect) {
            case CATSECT_REAL:
            case CATSECT_REAL_ONLY:
                show_type[TYPE_REAL] = true;
                show_type[TYPE_STRING] = true;
                break;
            case CATSECT_CPX:
                show_type[TYPE_COMPLEX] = true;
                break;
            case CATSECT_MAT:
            case CATSECT_MAT_ONLY:
                show_type[TYPE_REALMATRIX] = true;
                show_type[TYPE_COMPLEXMATRIX] = true;
                break;
            case CATSECT_MAT_LIST:
            case CATSECT_MAT_LIST_ONLY:
                show_type[TYPE_REALMATRIX] = true;
                show_type[TYPE_COMPLEXMATRIX] = true;
                show_type[TYPE_LIST] = true;
                break;
            case CATSECT_LIST:
            case CATSECT_LIST_ONLY:
                show_type[TYPE_LIST] = true;
                break;
            case CATSECT_EQN:
            case CATSECT_EQN_ONLY:
                show_type[TYPE_EQUATION] = true;
                break;
            case CATSECT_OTHER:
                show_type[TYPE_UNIT] = true;
                show_type[TYPE_DIR_REF] = true;
                show_type[TYPE_PGM_REF] = true;
                show_type[TYPE_VAR_REF] = true;
                break;
            case CATSECT_LIST_STR_ONLY:
                show_type[TYPE_STRING] = true;
                show_type[TYPE_LIST] = true;
                break;
            default:
                for (int i = TYPE_REAL; i < TYPE_SENTINEL; i++)
                    show_type[i] = true;
                break;
        }

        bool show_nonlocal = show_nonlocal_vars(catsect);

        try {
            directory *dir = cwd;
            vartype_list *path = show_nonlocal ? get_path() : NULL;
            int path_index = -1;
            std::vector<std::string> names;
            std::vector<int4> dirs;
            std::vector<int> items;
            int nlocal = 0;

            for (int i = local_vars_count - 1; i >= 0; i--) {
                if ((local_vars[i].flags & VAR_PRIVATE) != 0)
                    continue;
                int type = local_vars[i].value->type;
                if (!show_type[type])
                    continue;
                std::string n(local_vars[i].name, local_vars[i].length);
                for (int j = 0; j < names.size(); j++)
                    if (names[j] == n)
                        goto found_dup1;
                names.push_back(n);
                dirs.push_back(0);
                items.push_back(i);
                if (dir == cwd && path_index == -1)
                    nlocal++;
                found_dup1:;
            }

            do {
                for (int i = dir->vars_count - 1; i >= 0; i--) {
                    int type = dir->vars[i].value->type;
                    if (!show_type[type])
                        continue;
                    std::string n(dir->vars[i].name, dir->vars[i].length);
                    for (int j = 0; j < names.size(); j++)
                        if (names[j] == n)
                            goto found_dup2;
                    names.push_back(n);
                    dirs.push_back(dir->id);
                    items.push_back(i);
                    if (dir == cwd && path_index == -1)
                        nlocal++;
                    found_dup2:;
                }
                if (!show_nonlocal)
                    break;
                if (path_index == -1) {
                    dir = dir->parent;
                    if (dir == NULL) {
                        if (path != NULL) {
                            path_index = 0;
                            goto do_path;
                        }
                    }
                } else {
                    do_path:
                    dir = NULL;
                    while (path_index < path->size) {
                        vartype *v = path->array->data[path_index++];
                        if (v->type != TYPE_DIR_REF)
                            continue;
                        dir = get_dir(((vartype_dir_ref *) v)->dir);
                        if (dir != NULL)
                            break;
                    }
                }
            } while (dir != NULL);

            if (items.size() == 0) {
                /* We should only get here if the 'plainmenu' catalog is
                * in operation; the other catalogs only operate during
                * command entry mode, or are label catalogs -- so in those
                * cases, it is possible to prevent empty catalogs from
                * being displayed *in advance* (i.e., check if any real
                * variables exist before enabling MENU_CATALOG with
                * catalogmenu_section = CATSECT_REAL, etc.).
                * When a catalog becomes empty while displayed, we move
                * to the top level silently. The "No XXX Variables" message
                * is only displayed if the user actively tries to enter
                * an empty catalog section.
                */
                if (catsect == CATSECT_LIST || catsect == CATSECT_EQN || catsect == CATSECT_OTHER) {
                    set_cat_section(CATSECT_MORE);
                    goto draw_other;
                } else {
                    set_cat_section(CATSECT_TOP);
                    goto draw_top;
                }
            }

            catalogmenu_rows[catindex] = (items.size() + 5) / 6;
            if (catalogmenu_row[catindex] >= catalogmenu_rows[catindex])
                catalogmenu_row[catindex] = catalogmenu_rows[catindex] - 1;
            int row = catalogmenu_row[catindex];
            for (int k = 0; k < 6; k++) {
                int n = k + row * 6;
                if (n < names.size()) {
                    draw_key(k, 0, 0, names[n].c_str(), names[n].length(), n >= nlocal);
                    catalogmenu_dir[catindex][k] = dirs[n];
                    catalogmenu_item[catindex][k] = items[n];
                } else {
                    draw_key(k, 0, 0, "", 0);
                    catalogmenu_item[catindex][k] = -1;
                }
            }
        } catch (std::bad_alloc &) {
            catalogmenu_rows[catindex] = 1;
            catalogmenu_row[catindex] = 0;
            mode_updown = 0;
            for (int k = 0; k < 6; k++) {
                draw_key(k, 0, 0, "", 0, true);
                catalogmenu_item[catindex][k] = -1;
            }
        }

        mode_updown = catalogmenu_rows[catindex] > 1;
        set_annunciators(mode_updown, -1, -1, -1, -1, -1);
    }
}

int draw_eqn_catalog(int section, int row, int *item) {
    int saved_mode_commandmenu = mode_commandmenu;
    mode_commandmenu = MENU_CATALOG;
    int saved_section = catalogmenu_section[0];
    catalogmenu_section[0] = section;
    int saved_row = catalogmenu_row[0];
    catalogmenu_row[0] = row;
    int saved_rows = catalogmenu_rows[0];
    int4 saved_dirs[6];
    int saved_items[6];
    for (int i = 0; i < 6; i++) {
        saved_dirs[i] = catalogmenu_dir[0][i];
        saved_items[i] = catalogmenu_item[0][i];
    }

    draw_catalog();
    for (int i = 0; i < 6; i++)
        item[i] = catalogmenu_item[0][i];
    int rows = catalogmenu_rows[0];

    mode_commandmenu = saved_mode_commandmenu;
    catalogmenu_section[0] = saved_section;
    catalogmenu_row[0] = saved_row;
    catalogmenu_rows[0] = saved_rows;
    for (int i = 0; i < 6; i++) {
        catalogmenu_dir[0][i] = saved_dirs[i];
        catalogmenu_item[0][i] = saved_items[i];
    }
    if (section == CATSECT_TOP) {
        rows = 2;
        set_annunciators(1, -1, -1, -1, -1, -1);
    }
    return rows;
}

const char *unit_menu_text[] = {
    /* LENG  */ "m\0cm\0mm\0yd\0ft\0in\0Mpc\0pc\0lyr\0au\0km\0mi\0nmi\0miUS\0chain\0rd\0fath\0ftUS\0mil\0\21\0\24\0fermi\0",
    /* AREA  */ "m\0362\0cm\0362\0b\0yd\0362\0ft\0362\0in\0362\0km\0362\0ha\0a\0mi\0362\0miUS\0362\0acre\0",
    /* VOL   */ "m\0363\0st\0cm\0363\0yd\0363\0ft\0363\0in\0363\0l\0galUK\0galC\0gal\0qt\0pt\0ml\0cu\0ozfl\0ozUK\0tbsp\0tsp\0bbl\0bu\0pk\0fbm\0",
    /* TIME  */ "yr\0d\0h\0min\0s\0Hz\0",
    /* SPEED */ "m/s\0cm/s\0ft/s\0kph\0mph\0knot\0c\0ga\0",
    /* MASS  */ "kg\0g\0lb\0oz\0slug\0lbt\0ton\0tonUK\0t\0ozt\0ct\0grain\0u\0mol\0",
    /* FORCE */ "N\0dyn\0gf\0kip\0lbf\0pdl\0",
    /* ENRG  */ "J\0erg\0kcal\0cal\0Btu\0ft*lbf\0therm\0MeV\0eV\0",
    /* POWR  */ "W\0hp\0",
    /* PRESS */ "Pa\0atm\0bar\0psi\0torr\0mmHg\0inHg\0inH2O\0",
    /* TEMP  */ "\23C\0\23F\0K\0\23R\0",
    /* ELEC  */ "V\0A\0C\0\202\0F\0W\0Fdy\0H\0mho\0S\0T\0Wb\0",
    /* ANGL  */ "\23\0r\0grad\0arcmin\0arcs\0sr\0",
    /* LIGHT */ "fc\0flam\0lx\0ph\0sb\0lm\0cd\0lam\0",
    /* RAD   */ "Gy\0rad\0rem\0Sv\0Bq\0Ci\0R\0",
    /* VISC  */ "P\0St\0"
};

void get_units_cat_row(int catsect, const char *text[6], int length[6], int *row, int *rows) {
    const char *mtext = unit_menu_text[catsect - CATSECT_UNITS_LENG];
    retry:
    int n = *row * 6;
    int i = 0;
    int off = 0;
    while (true) {
        int len = strlen(mtext + off);
        if (len == 0)
            break;
        if (i >= n && i < n + 6) {
            text[i - n] = mtext + off;
            length[i - n] = len;
        }
        off += len + 1;
        i++;
    }
    *rows = (i + 5) / 6;
    if (i < n) {
        *row = 0;
        goto retry;
    }
    while (i < n + 6) {
        text[i - n] = "";
        length[i - n] = 0;
        i++;
    }
}

void display_mem() {
    uint8 bytes = shell_get_mem();
    char buf[20];
    int buflen;
    clear_display();
    draw_string(0, 0, "Available Memory:", 17);
    buflen = ulong2string(bytes, buf, 20);
    draw_string(0, 1, buf, buflen);
    draw_string(buflen + 1, 1, "Bytes", 5);
    flush_display();
}

static int procrustean_phloat2string(phloat d, char *buf, int buflen) {
    char tbuf[100];
    int tbuflen = phloat2string(d, tbuf, 100, 0, 0, 3,
                                flags.f.thousands_separators, MAX_MANT_DIGITS);
    if (tbuflen <= buflen) {
        memcpy(buf, tbuf, tbuflen);
        return tbuflen;
    }
    if (flags.f.thousands_separators) {
        tbuflen = phloat2string(d, tbuf, 100, 0, 0, 3, 0, MAX_MANT_DIGITS);
        if (tbuflen <= buflen) {
            memcpy(buf, tbuf, tbuflen);
            return tbuflen;
        }
    }
    int epos = 0;
    while (epos < tbuflen && tbuf[epos] != 24)
        epos++;
    if (epos == tbuflen) {
        int dpos = buflen - 2;
        char dec = flags.f.decimal_point ? '.' : ',';
        while (dpos >= 0 && tbuf[dpos] != dec)
            dpos--;
        if (dpos != -1) {
            memcpy(buf, tbuf, buflen - 1);
            buf[buflen - 1] = 26;
            return buflen;
        }
        tbuflen = phloat2string(d, tbuf, 100, 0, MAX_MANT_DIGITS - 1, 1, 0, MAX_MANT_DIGITS);
        epos = 0;
        int zero_since = -1;
        while (epos < tbuflen && tbuf[epos] != 24) {
            if (tbuf[epos] == '0') {
                if (zero_since == -1)
                    zero_since = epos;
            } else {
                zero_since = -1;
            }
            epos++;
        }
        if (zero_since != -1) {
            memmove(tbuf + zero_since, tbuf + epos, tbuflen - epos);
            tbuflen -= epos - zero_since;
            epos = zero_since;
        }
        if (tbuflen <= buflen) {
            memcpy(buf, tbuf, tbuflen);
            return tbuflen;
        }
    }
    int expsize = tbuflen - epos;
    memcpy(buf, tbuf, buflen - expsize - 1);
    buf[buflen - expsize - 1] = 26;
    memcpy(buf + buflen - expsize, tbuf + epos, expsize);
    return buflen;
}

void show() {
    clear_display();
    char *buf = NULL;

    if (flags.f.prgm_mode) {
        display_prgm_line(INT_MAX);
        done:
        flush_display();
        free(buf);
        return;
    }

    if (alpha_active()) {
        if (reg_alpha_length <= disp_c)
            draw_string(0, 0, reg_alpha, reg_alpha_length);
        else {
            draw_string(0, 0, reg_alpha, disp_c);
            draw_string(0, 1, reg_alpha + disp_c, reg_alpha_length - disp_c);
        }
        goto done;
    }

    if (sp < 0)
        goto done;

    int sz = disp_c * disp_r;
    buf = (char *) malloc(sz + 1);
    if (buf == NULL) {
        draw_string(0, 0, "<Low Mem>", 9);
        goto done;
    }
    int bufptr = 0;
    vartype *rx = stack[sp];
    int real_space;
    vartype_complex c;

    switch (rx->type) {
        case TYPE_REAL: {
            bufptr = phloat2string(((vartype_real *) rx)->x, buf, sz + 1,
                                    2, 0, 3,
                                    flags.f.thousands_separators, MAX_MANT_DIGITS);
            if (bufptr == sz + 1)
                bufptr = phloat2string(((vartype_real *) rx)->x, buf,
                                        sz, 2, 0, 3, 0, MAX_MANT_DIGITS);
            show_one_or_more_lines:
            int row = 0;
            char *p = buf;
            while (bufptr > 0) {
                int n = bufptr < disp_c ? bufptr : disp_c;
                draw_string(0, row, p, n);
                p += n;
                bufptr -= n;
                row++;
            }
            goto done;
        }
        case TYPE_COMPLEX: {
            real_space = ((disp_r + 1) / 2) * disp_c;
            c = *((vartype_complex *) rx);
            phloat x, y;
            if (flags.f.polar) {
                generic_r2p(c.re, c.im, &x, &y);
                if (p_isinf(x))
                    x = POS_HUGE_PHLOAT;
            } else {
                x = c.re;
                y = c.im;
            }
            bufptr += procrustean_phloat2string(x, buf + bufptr, real_space);
            while (bufptr % disp_c != 0)
                buf[bufptr++] = ' ';
            int p = bufptr++;
            bufptr += procrustean_phloat2string(y, buf + bufptr, sz - bufptr);
            if (flags.f.polar) {
                buf[p] = 23;
            } else {
                if (buf[p + 1] == '-') {
                    buf[p] = '-';
                    buf[p + 1] = 'i';
                } else {
                    buf[p] = 'i';
                }
            }
            goto show_one_or_more_lines;
        }
        case TYPE_REALMATRIX: {
            if (disp_r == 2) {
                vartype_realmatrix *rm = (vartype_realmatrix *) rx;
                phloat *d = rm->array->data;
                bufptr = vartype2string(rx, buf, disp_c);
                while (bufptr < disp_c)
                    buf[bufptr++] = ' ';
                string2buf(buf, sz, &bufptr, "1:1=", 4);
                if (rm->array->is_string[0] != 0) {
                    char *text;
                    int4 len;
                    get_matrix_string(rm, 0, &text, &len);
                    char2buf(buf, sz, &bufptr, '"');
                    string2buf(buf, sz, &bufptr, text, len);
                    if (bufptr < sz)
                        char2buf(buf, sz, &bufptr, '"');
                } else {
                    bufptr += procrustean_phloat2string(*d, buf + bufptr, sz - bufptr);
                }
            } else {
                try {
                    std::string s;
                    full_real_matrix_to_string(rx, &s, disp_r);
                    string_copy(buf, &bufptr, s.c_str(), s.length());
                } catch (std::bad_alloc &) {
                    string_copy(buf, &bufptr, "<Low Mem>", 9);
                }
            }
            goto show_one_or_more_lines;
        }
        case TYPE_COMPLEXMATRIX: {
            if (disp_r == 2) {
                vartype_complexmatrix *cm = (vartype_complexmatrix *) rx;
                vartype_complex c;
                bufptr = vartype2string(rx, buf, disp_c);
                while (bufptr < disp_c)
                    buf[bufptr++] = ' ';
                string2buf(buf, sz, &bufptr, "1:1=", 4);
                c.type = TYPE_COMPLEX;
                c.re = cm->array->data[0];
                c.im = cm->array->data[1];
                bufptr += vartype2string((vartype *) &c, buf + bufptr, sz - bufptr);
            } else {
                try {
                    std::string s;
                    full_complex_matrix_to_string(rx, &s, disp_r);
                    string_copy(buf, &bufptr, s.c_str(), s.length());
                } catch (std::bad_alloc &) {
                    string_copy(buf, &bufptr, "<Low Mem>", 9);
                }
            }
            goto show_one_or_more_lines;
        }
        case TYPE_STRING:
        case TYPE_EQUATION: {
            int length;
            const char *text;
            char d;
            if (rx->type == TYPE_STRING) {
                vartype_string *s = (vartype_string *) rx;
                length = s->length;
                text = s->txt();
                d = '"';
            } else {
                vartype_equation *eq = (vartype_equation *) rx;
                length = eq->data->length;
                text = eq->data->text;
                d = eq->data->compatMode ? '`' : '\'';
            }
            char2buf(buf, sz, &bufptr, d);
            string2buf(buf, sz, &bufptr, text, length);
            if (bufptr < sz)
                char2buf(buf, sz, &bufptr, d);
            goto show_one_or_more_lines;
        }
        case TYPE_LIST: {
            if (disp_r == 2) {
                vartype_list *list = (vartype_list *) rx;
                bufptr = vartype2string(rx, buf, disp_c);
                if (list->size > 0) {
                    while (bufptr < disp_c)
                        buf[bufptr++] = ' ';
                    string2buf(buf, sz, &bufptr, "1=", 2);
                    bufptr += vartype2string(list->array->data[0], buf + bufptr, sz - bufptr);
                }
            } else {
                try {
                    int maxlen = disp_r * disp_c;
                    std::string s;
                    full_list_to_string(rx, &s, maxlen + 2);
                    s.erase(s.length() - 1);
                    if (s.length() > maxlen) {
                        s.erase(maxlen - 1);
                        s += "\32";
                    }
                    string_copy(buf, &bufptr, s.c_str(), s.length());
                } catch (std::bad_alloc &) {
                    string_copy(buf, &bufptr, "<Low Mem>", 9);
                }
            }
            goto show_one_or_more_lines;
        }
        case TYPE_UNIT: {
            vartype_unit *u = (vartype_unit *) rx;
            bufptr = phloat2string(((vartype_real *) rx)->x, buf, sz,
                                    0, 0, 3,
                                    flags.f.thousands_separators, MAX_MANT_DIGITS);
            int ulen = u->length;
            int nlen = bufptr;
            if (bufptr + ulen + 1 > sz) {
                ulen = sz - bufptr - 1;
                if (ulen < 10) {
                    ulen = 10;
                    if (ulen > u->length)
                        ulen = u->length;
                }
                int nlen = sz - ulen - 1;
                if (nlen > bufptr)
                    nlen = bufptr;
            }
            if (bufptr > nlen) {
                buf[nlen - 1] = 26;
                bufptr = nlen;
            }
            buf[bufptr++] = '_';
            memcpy(buf + bufptr, u->text, ulen);
            bufptr += ulen;
            if (u->length > ulen)
                buf[bufptr - 1] = 26;
            goto show_one_or_more_lines;
        }
    }
    goto done;
}

int tvm_message(char *buf, int buflen) {
    int pos = 0;
    string2buf(buf, buflen, &pos, "P/YR: ", 6);
    vartype *v = recall_var("P/YR", 4);
    if (v == NULL || v->type != TYPE_REAL)
        string2buf(buf, buflen, &pos, "N/A", 3);
    else
        pos += phloat2string(((vartype_real *) v)->x, buf + pos, buflen - pos, 0, 0, 3, flags.f.thousands_separators);
    char2buf(buf, buflen, &pos, ' ');
    v = recall_var("BEGIN", 5);
    phloat m;
    if (v == NULL || v->type != TYPE_REAL || ((m = ((vartype_real *) v)->x) != 0 && m != 1))
        string2buf(buf, buflen, &pos, "N/A", 3);
    else if (m == 0)
        string2buf(buf, buflen, &pos, "END", 3);
    else
        string2buf(buf, buflen, &pos, "BEGIN", 5);
    return pos;
}

bool display_header() {
    if (!mode_header || disp_r < 4)
        return false;

    clear_row(0);
    for (int x = 0; x < disp_w; x++)
        draw_pixel(x, 6);

    char buf[50];
    int pos = 0;
    if (mode_header_flags) {
        for (int i = 0; i <= 10; i++)
            if (flags.farray[i])
                pos += int2string(i, buf + pos, 50 - pos);
    }
    if (mode_header_polar && flags.f.polar) {
        if (pos != 0)
            char2buf(buf, 50, &pos, ' ');
        char2buf(buf, 50, &pos, '\27');
    }
    int app_w = 0;
    if (mode_appmenu >= MENU_BASE && mode_appmenu <= MENU_BASE_LOGIC || mode_plainmenu == MENU_MODES3) {
        if (pos != 0)
            char2buf(buf, 50, &pos, ' ');
        string2buf(buf, 50, &pos, "WS: ", 4);
        pos += int2string(mode_wsize, buf + pos, 50 - pos);
        char2buf(buf, 50, &pos, ' ');
        char2buf(buf, 50, &pos, flags.f.base_signed ? 'S' : 'U');
        if (flags.f.base_wrap)
            string2buf(buf, 50, &pos, " WRAP", 5);
    } else if (mode_appmenu >= MENU_TVM_APP1 && mode_appmenu <= MENU_TVM_TABLE) {
        if (pos != 0)
            char2buf(buf, 50, &pos, ' ');
        pos += tvm_message(buf + pos, 50);
    }
    app_w = draw_small_string(0, -2, buf, pos, disp_w, true);
    if (app_w != 0)
        app_w += 2;

    try {
        std::string path("}", 1);
        directory *dir = cwd;
        while (dir->parent != NULL) {
            directory *parent = dir->parent;
            for (int i = 0; i < parent->children_count; i++) {
                if (parent->children[i].dir == dir) {
                    path = std::string(parent->children[i].name, parent->children[i].length) + " " + path;
                    goto found;
                }
            }
            path = std::string("INTERNAL ERROR");
            goto done;
            found:
            dir = parent;
        }
        path = std::string("{ HOME ", 7) + path;
        done:
        draw_small_string(0, -2, path.c_str(), path.length(), disp_w - app_w, false, true);
    } catch (std::bad_alloc &) {
        draw_small_string(0, -2, "<Low Mem>", 9, disp_w - app_w);
    }

    return true;
}

static int column_width(vartype *m, int4 imin, int4 imax, int4 j) {
    int pixel_width, max_width;
    vartype_realmatrix *rm;
    vartype_complexmatrix *cm;
    int width;
    char buf[100], *b;
    int4 slen;

    /* Make sure to set aside at least enough width for the column label */
    pixel_width = to_int(log10(j + 1)) * 4 + 3;

    if (m->type == TYPE_REALMATRIX) {
        rm = (vartype_realmatrix *) m;
        cm = NULL;
        width = rm->columns;
        max_width = 41;
        if (!flags.f.decimal_point)
            max_width++;
    } else {
        rm = NULL;
        cm = (vartype_complexmatrix *) m;
        width = cm->columns;
        max_width = 83 + (flags.f.polar ? 10 : 6);
        if (!flags.f.decimal_point)
            max_width += 2;
    }
    int4 n = imin * width + j;
    for (int i = imin; i <= imax; i++) {
        int sw;
        if (m->type == TYPE_REALMATRIX) {
            if (rm->array->is_string[n] == 0) {
                vartype_real r;
                r.type = TYPE_REAL;
                r.x = rm->array->data[n];
                slen = vartype2string((vartype *) &r, buf, 100);
                b = buf;
                sw = 0;
            } else {
                get_matrix_string(rm, n, &b, &slen);
                sw = small_string_width("\"\"", 2);
            }
        } else {
            vartype_complex c;
            c.type = TYPE_COMPLEX;
            c.re = cm->array->data[2 * n];
            c.im = cm->array->data[2 * n + 1];
            slen = vartype2string((vartype *) &c, buf, 100);
            b = buf;
            sw = 0;
        }
        sw += small_string_width(b, slen);
        if (sw > pixel_width) {
            pixel_width = sw;
            if (pixel_width > max_width)
                return max_width;
        }
        n += width;
    }
    return pixel_width;
}

static int var2str_limited(vartype *v, char *buf, int buflen, int pixel_width) {
    char saved_disp[6];
    memcpy(saved_disp, flags.farray + 36, 6);
    int len;

    while (true) {
        len = vartype2string(v, buf, buflen);
        int p = small_string_width(buf, len);
        if (p <= pixel_width)
            goto done;
        int digits = ((flags.f.digits_bit3 << 1 | flags.f.digits_bit2) << 1 | flags.f.digits_bit1) << 1 | flags.f.digits_bit0;
        if (!flags.f.fix_or_all && digits <= 2)
            // SCI or ENG with 2 or fewer digits: should always fit,
            // because the column width calculation caps column width
            // at 41 pixels for a real, at 89 pixels for a rectangular
            // complex, and at 93 pixels for a polar complex, and those
            // are enough pixels for even the longest possible real
            // or complex numbers in SCI or ENG notation with 2 digits.
            // (This means we should never even get here.)
            goto done;
        if (flags.f.fix_or_all && flags.f.eng_or_all) {
            // Current mode is ALL; try FIX
            memcpy(flags.farray + 36, "\1\0\1\1\1\0", 6); // FIX 11
        } else if (digits == 0) {
            // FIX 00 still too large; try SCI
            memcpy(flags.farray + 36, "\1\0\1\1\0\0", 6); // SCI 11
        } else {
            digits--;
            flags.f.digits_bit3 = (digits & 8) != 0;
            flags.f.digits_bit2 = (digits & 4) != 0;
            flags.f.digits_bit1 = (digits & 2) != 0;
            flags.f.digits_bit0 = digits & 1;
        }
    }

    done:
    memcpy(flags.farray + 36, saved_disp, 6);
    return len;
}

#if defined(ANDROID) || defined(IPHONE)
void show_alpha_keyboard(bool show) {
    static bool alpha_keyboard_visible;
    if (mode_popup_unknown || alpha_keyboard_visible != show) {
        mode_popup_unknown = false;
        alpha_keyboard_visible = show;
        shell_show_alpha_keyboard(show);
    }
}
#endif

void redisplay(int mode) {
    if (eqn_draw())
        return;

#if defined(ANDROID) || defined(IPHONE)
    show_alpha_keyboard(core_alpha_menu() != 0);
#endif

    if (mode_clall) {
        clear_display();
        draw_string(0, 0, "Clear All Memory?", 17);
        draw_key(0, 0, 0, "YES", 3);
        draw_key(1, 0, 0, "", 0);
        draw_key(2, 0, 0, "", 0);
        draw_key(3, 0, 0, "", 0);
        draw_key(4, 0, 0, "", 0);
        draw_key(5, 0, 0, "NO", 2);
        flush_display();
        return;
    }

    int headers = mode_message_lines;
    int footers = 0;
    bool showing_hdr = false;

    if (mode_message_lines != ALL_LINES)
        for (int i = 0; i < mode_message_lines && i < disp_r; i++) {
            clear_row(i);
            if (i < messages.size()) {
                std::string s = messages[i];
                draw_string(0, i, s.c_str(), s.length());
            }
        }

    if (headers >= disp_r) {
        flush_display();
        return;
    }
    for (int i = headers; i < disp_r; i++)
        clear_row(i);

    int menu_id;
    if (mode_commandmenu != MENU_NONE)
        menu_id = mode_commandmenu;
    else if (mode_alphamenu != MENU_NONE)
        menu_id = mode_alphamenu;
    else if (mode_transientmenu != MENU_NONE)
        menu_id = mode_transientmenu;
    else if (mode_plainmenu != MENU_NONE)
        menu_id = mode_plainmenu;
    else if (mode_auxmenu != MENU_NONE)
        menu_id = mode_auxmenu;
    else if (mode_appmenu != MENU_NONE)
        menu_id = mode_appmenu;
    else
        menu_id = MENU_NONE;
    
    int catsect;
    if (mode == 2 &&
            !((pending_command == CMD_XEQ || pending_command == CMD_GTO)
              && menu_id == MENU_CATALOG
              && ((catsect = get_cat_section()) == CATSECT_PGM
                  || catsect == CATSECT_PGM_ONLY)))
        menu_id = MENU_NONE;

    if (menu_id == MENU_CATALOG) {
        draw_catalog();
        footers = 1;
    } else if (menu_id == MENU_VARMENU) {
        draw_varmenu();
        if (varmenu_length == 0 && varmenu_eqn == NULL) {
            redisplay();
            return;
        }
        footers = 1;
    } else if (menu_id == MENU_CUSTOM1 || menu_id == MENU_CUSTOM2
            || menu_id == MENU_CUSTOM3) {
        int r = menu_id - MENU_CUSTOM1;
        if (flags.f.local_label
                && !(mode_command_entry && incomplete_argtype == ARG_CKEY)) {
            for (int i = 0; i < 5; i++) {
                char c = (r == 0 ? 'A' : 'F') + i;
                draw_key(i, 0, 0, &c, 1);
            }
            draw_key(5, 0, 0, "XEQ", 3);
        } else {
            for (int i = 0; i < 6; i++) {
                draw_key(i, 0, 1, custommenu_label[r][i],
                                  custommenu_length[r][i]);
            }
        }
        footers = 1;
    } else if (menu_id == MENU_PROGRAMMABLE) {
        for (int i = 0; i < 6; i++)
            draw_key(i, 0, 0, progmenu_label[i], progmenu_length[i]);
        footers = 1;
    } else if (menu_id != MENU_NONE) {
        const menu_spec *m = menus + menu_id;
        for (int i = 0; i < 6; i++) {
            const menu_item_spec *mi = m->child + i;
            if (mi->menuid == MENU_NONE || (mi->menuid & 0x3000) == 0)
                draw_key(i, 0, 0, mi->title, mi->title_length, mi->menuid == 1);
            else {
                int cmd_id = mi->menuid & 0xfff;
                const command_spec *cmd = &cmd_array[cmd_id];
                int is_flag = (mi->menuid & 0x2000) != 0;
                if (is_flag)
                    /* Take a closer look at the command ID and highlight
                     * the menu item if appropriate -- that is, clear 'is_flag'
                     * if highlighting is *not* appropriate
                     */
                    is_flag = should_highlight(cmd_id);
                draw_key(i, is_flag, 1, cmd->name, cmd->name_length);
            }
        }
        footers = 1;
    }

    int available = disp_r - headers - footers;
    if (available <= 0)
        goto done;

    if (!flags.f.prgm_mode) {
        if (mode_command_entry || pending_command != CMD_NONE && mode == 1) {
            int cmd_row = disp_r - (menu_id == MENU_NONE ? 1 : 2);
            int lines_used;
            if (mode_command_entry)
                lines_used = display_incomplete_command(cmd_row, available);
            else
                lines_used = display_command(cmd_row, available);
            footers += lines_used;
            available -= lines_used;
        } else if (pending_command > CMD_NONE) {
            headers = display_command(0, available);
            available = disp_r - headers - footers;
        }
    }

    if (headers == 0 && display_header()) {
        headers = 1;
        available = disp_r - headers - footers;
        showing_hdr = true;
    }

    if (available <= 0)
        goto done;

    if (flags.f.prgm_mode) {
        int lines = pc2line(dir_list[current_prgm.dir]->prgms[current_prgm.idx].size);
        if (prgm_highlight_row > lines)
            prgm_highlight_row = lines;
        int saved_prgm_highlight = prgm_highlight_row;
        int lines_used = display_prgm_line(0, headers, footers);
        for (int i = 0; i < prgm_highlight_row; i++)
            display_prgm_line(i - prgm_highlight_row, headers);
        prgm_highlight_row += lines_used - 1;
        for (int i = prgm_highlight_row + 1; i < disp_r - headers - footers; i++)
            display_prgm_line(i - prgm_highlight_row, headers);
        prgm_highlight_row = saved_prgm_highlight ;
    } else if (alpha_active() && !mode_number_entry && !mode_command_entry && mode == 0) {
        int avail_c = disp_c * available;
        int len = reg_alpha_length;
        if (mode_alpha_entry)
            avail_c--;
        int ellipsis = 0;
        if (len > avail_c) {
            len = avail_c - 1;
            ellipsis = 1;
        }
        int pos = reg_alpha_length - len;
        int lines = (len + ellipsis + mode_alpha_entry + disp_c - 1) / disp_c;
        if (lines == 0)
            lines = 1;
        for (int i = 0; i < lines; i++) {
            int row = disp_r - footers - lines + i;
            int seg = len;
            if (seg > disp_c)
                seg = disp_c;
            if (ellipsis) {
                draw_char(0, row, 26);
                if (seg == disp_c)
                    seg--;
            }
            draw_string(ellipsis, row, reg_alpha + pos, seg);
            len -= seg;
            if (len == 0 && mode_alpha_entry && ellipsis + seg < disp_c)
                draw_char(ellipsis + seg, row, '_');
            ellipsis = 0;
            pos += seg;
        }
    } else if (!mode_matedit_stk && (matedit_mode & 2) != 0 && disp_r >= 4) {
        /* Figure out how to allocate screen space */
        int msg_lines = showing_hdr ? 0 : headers;
        if (showing_hdr)
            clear_row(0);
        vartype *m;
        int err = matedit_get(&m);
        if (err != ERR_NONE)
            goto do_run_mode;
        int mrows;
        switch (m->type) {
            case TYPE_REALMATRIX: mrows = ((vartype_realmatrix *) m)->rows + 1; break;
            case TYPE_COMPLEXMATRIX: mrows = ((vartype_complexmatrix *) m)->rows + 1; break;
            default: /* TYPE_LIST */ mrows = ((vartype_list *) m)->size; break;
        }
        int xlines = disp_r - footers - mrows;
        if (xlines <= 1)
            xlines = disp_r >= 5 ? 2 : 1;
        xlines = display_x(disp_r - footers - 1, xlines);
        if (xlines + footers + mrows > disp_r) {
            mrows = disp_r - footers - xlines;
            if (mrows < 0) {
                xlines += mrows;
                mrows = 0;
            }
        }
        int space = disp_r - footers - xlines;
        if (mrows > space)
            mrows = space;

        char *buf = (char *) malloc(disp_c);
        if (buf == NULL)
            goto do_run_mode;
        freer f(buf);

        if (m->type == TYPE_LIST) {
            /* Draw list segment */
            vartype_list *list = (vartype_list *) m;
            if (matedit_view_i == -1)
                matedit_view_i = matedit_i - mrows / 2;
            else if (matedit_i < matedit_view_i)
                matedit_view_i = matedit_i;
            else if (matedit_i >= matedit_view_i + mrows)
                matedit_view_i = matedit_i - mrows + 1;
            if (matedit_view_i < 0)
                matedit_view_i = 0;
            else if (matedit_view_i + mrows > list->size)
                matedit_view_i = list->size - mrows;

            int digits = to_int(log10(matedit_view_i + mrows)) + 1;
            for (int r = msg_lines; r < mrows; r++) {
                int rn = r + matedit_view_i + 1;
                int d = to_int(log10(rn)) + 1;
                int bufptr = 0;
                for (int sp = digits - d; sp > 0; sp--)
                    char2buf(buf, disp_c, &bufptr, ' ');
                bufptr += int2string(rn, buf + bufptr, disp_c - bufptr);
                rn--;
                char2buf(buf, disp_c, &bufptr, rn == matedit_i ? 6 : ' ');
                bufptr += vartype2string(list->array->data[rn], buf + bufptr, disp_c - bufptr);
                draw_string(0, r, buf, bufptr);
            }
        } else {
            /* Draw matrix segment */
            vartype_realmatrix *rm;
            vartype_complexmatrix *cm;
            int4 rows, columns;
            if (m->type == TYPE_REALMATRIX) {
                rm = (vartype_realmatrix *) m;
                cm = NULL;
                rows = rm->rows;
                columns = rm->columns;
            } else {
                rm = NULL;
                cm = (vartype_complexmatrix *) m;
                rows = cm->rows;
                columns = cm->columns;
            }
            int mrows1 = mrows - 1; // need 1 line for header

            if (matedit_view_i == -1)
                matedit_view_i = matedit_i - mrows1 / 2;
            else if (matedit_i < matedit_view_i)
                matedit_view_i = matedit_i;
            else if (matedit_i >= matedit_view_i + mrows1)
                matedit_view_i = matedit_i - mrows1 + 1;
            if (matedit_view_i < 0)
                matedit_view_i = 0;
            else if (matedit_view_i + mrows1 > rows)
                matedit_view_i = rows - mrows1;

            try {
                std::vector<int> widths;
                int header_width = 4 * (to_int(log10(matedit_view_i + mrows1)) + 1) + 1;
                int avail = disp_w - header_width;
                int w = column_width(m, matedit_view_i, matedit_view_i + mrows1 - 1, matedit_j) + 3;
                avail -= w;
                widths.push_back(w);

                if (matedit_view_j == -1 || matedit_j < matedit_view_j)
                    matedit_view_j = matedit_j;
                int4 min_j = matedit_j;
                int4 imin = matedit_view_i;
                int4 imax = matedit_view_i + mrows1 - 1;

                while (min_j > matedit_view_j) {
                    w = column_width(m, imin, imax, min_j - 1) + 3;
                    if (avail < w)
                        break;
                    widths.insert(widths.begin(), w);
                    avail -= w;
                    min_j--;
                }
                int4 max_j = matedit_j;
                while (max_j < columns - 1) {
                    w = column_width(m, imin, imax, max_j + 1) + 3;
                    if (avail < w)
                        break;
                    widths.push_back(w);
                    avail -= w;
                    max_j++;
                }
                while (min_j > 0) {
                    w = column_width(m, imin, imax, min_j - 1) + 3;
                    if (avail < w)
                        break;
                    widths.insert(widths.begin(), w);
                    avail -= w;
                    min_j--;
                }
                matedit_view_j = min_j;

                /* Row headers & horizontal lines */
                for (int i = msg_lines == 0 ? 1 : msg_lines; i < mrows; i++) {
                    int v = i * 8 - 1;
                    int off = i == msg_lines;
                    fill_rect(0, v + off, header_width, 7 - off, 1);
                    char numbuf[10];
                    int numlen = int2string(matedit_view_i + i, numbuf, 10);
                    draw_small_string(1, v - 1, numbuf, numlen, header_width - 2, true, false, true);
                    for (int j = header_width + 1; j < disp_w - avail; j += 2)
                        draw_pixel(j, v + 7);
                }
                
                /* Columns */
                int h = header_width;
                int4 j = min_j;
                for (std::vector<int>::iterator iter = widths.begin(); iter < widths.end(); iter++) {
                    int cw = *iter;
                    if (msg_lines == 0) {
                        fill_rect(h, 0, cw - 1, 7, 1);
                        char numbuf[10];
                        int numlen = int2string(j + 1, numbuf, 10);
                        int indent = (cw - 2 - small_string_width(numbuf, numlen)) / 2;
                        draw_small_string(h + indent + 1, -1, numbuf, numlen, cw - indent - 3, false, false, true);
                    }
                    for (int i = 8 * (msg_lines == 0 ? 1 : msg_lines); i < mrows * 8; i += 2)
                        draw_pixel(h + cw - 1, i);
                    for (int i = msg_lines == 0 ? 1 : msg_lines; i < mrows; i++) {
                        int4 n = (matedit_view_i + i - 1) * columns + j;
                        if (m->type == TYPE_REALMATRIX) {
                            if (rm->array->is_string[n] == 0) {
                                vartype_real r;
                                r.type = TYPE_REAL;
                                r.x = rm->array->data[n];
                                char numbuf[50];
                                int numlen = var2str_limited((vartype *) &r, numbuf, 50, cw - 3);
                                draw_small_string(h + 1, i * 8 - 2, numbuf, numlen, cw - 3, true);
                            } else {
                                char *txt;
                                int4 len;
                                get_matrix_string(rm, n, &txt, &len);
                                std::string s("\"");
                                s += std::string(txt, len);
                                s += "\"";
                                draw_small_string(h + 1, i * 8 - 2, s.c_str(), len + 2, cw - 3);
                            }
                        } else {
                            vartype_complex c;
                            c.type = TYPE_COMPLEX;
                            c.re = cm->array->data[2 * n];
                            c.im = cm->array->data[2 * n + 1];
                            char numbuf[100];
                            int numlen = var2str_limited((vartype *) &c, numbuf, 100, cw - 3);
                            draw_small_string(h + 1, i * 8 - 2, numbuf, numlen, cw - 3, true);
                        }
                        /* Draw solid lines around current cell */
                        if (matedit_view_i + i - 1 == matedit_i && j == matedit_j) {
                            int x1 = h - 1;
                            int x2 = h + cw - 1;
                            int y1 = i * 8 - 2;
                            int y2 = i * 8 + 6;
                            if (i == msg_lines)
                                y1 += 2;
                            else
                                draw_line(x1, y1, x2, y1);
                            draw_line(x2, y1, x2, y2);
                            draw_line(x2, y2, x1, y2);
                            draw_line(x1, y2, x1, y1);
                        }
                    }
                    h += cw;
                    j++;
                }
            } catch (std::bad_alloc &) {
                goto do_run_mode;
            }
        }

        /* Draw stack */
        for (int r = mrows; r < space; r++)
            display_level(space - r, r);
    } else {
        do_run_mode:
        bool lastx_shown = false;
        if (mode_lastx_top) {
            int lastx_line = mode_header && disp_r >= 4 ? 1 : 0;
            if (lastx_line >= headers && available > 1) {
                display_level(-1, lastx_line);
                headers++;
                available--;
                lastx_shown = true;
            }
        }
        if (mode_alpha_top) {
            int alpha_line = mode_header && disp_r >= 4 ? 1 : 0;
            if (lastx_shown)
                alpha_line++;
            if (alpha_line >= headers && available > 1) {
                clear_row(alpha_line);
                draw_string(0, alpha_line, "\205\200\"", 3);
                int len = reg_alpha_length;
                bool ellipsis = false;
                if (len > disp_c - 4) {
                    len = disp_c - 4;
                    ellipsis = true;
                }
                draw_string(3, alpha_line, reg_alpha, len);
                draw_char(len + 3, alpha_line, ellipsis ? 26 : '"');
                headers++;
                available--;
            }
        }
        int space = disp_r - headers - footers;
        space -= display_x(disp_r - footers - 1, space);
        for (int i = headers; i < headers + space; i++)
            display_level(headers + space - i, i);
    }

    done:
    flush_display();
}

int print_display() {
    if (disp_w <= 143) {
        // Not too wide to print horizontally
        shell_print(NULL, 0, display, disp_bpl, 0, 0, disp_w, disp_h);
        return ERR_NONE;
    } else if (disp_h > 143) {
        // Too wide to print horizontally *and* vertically:
        // Print horizontally and just clip to the right margin
        shell_print(NULL, 0, display, disp_bpl, 0, 0, 143, disp_h);
        return ERR_NONE;
    } else {
        // Too wide to print horizontally, but fits vertically:
        // print in landscape orientation
        int bpl = (disp_h + 7) / 8;
        int size = bpl * disp_w;
        char *disp = (char *) malloc(size);
        if (disp == NULL)
            return ERR_INSUFFICIENT_MEMORY;
        memset(disp, 0, size);
        for (int i = 0; i < disp_h; i++) {
            char m = 1 << (i & 7);
            for (int j = 0; j < disp_w; j++)
                if ((display[i * disp_bpl + (j >> 3)] & (1 << (j & 7))) != 0)
                    disp[(disp_w - j - 1) * bpl + (i >> 3)] |= m;
        }
        shell_print(NULL, 0, disp, bpl, 0, 0, disp_h, disp_w);
        free(disp);
        return ERR_NONE;
    }
}

struct prp_data_struct {
    char buf[100];
    int len;
    pgm_index saved_prgm;
    int cmd;
    arg_struct arg;
    int4 line;
    int4 pc;
    int4 lines;
    int width;
    int first;
    bool trace;
    bool normal;
    bool full_xstr;
    std::set<int4> *target_lines;

    prp_data_struct() : target_lines(NULL) {}
    ~prp_data_struct() { delete target_lines; }
};

static prp_data_struct *prp_data;
static int print_program_worker(bool interrupted);

int print_program(pgm_index prgm, int4 pc, int4 lines, bool normal) {
    prp_data_struct *dat = new (std::nothrow) prp_data_struct;
    if (dat == NULL)
        return ERR_INSUFFICIENT_MEMORY;

    set_annunciators(-1, -1, 1, -1, -1, -1);
    dat->len = 0;
    dat->saved_prgm = current_prgm;
    dat->cmd = CMD_NONE;
    dat->line = pc2line(pc);
    dat->pc = pc;
    dat->lines = lines;
    dat->width = flags.f.double_wide_print ? 12 : 24;
    dat->first = 1;
    if (normal) {
        dat->trace = false;
        dat->normal = true;
        dat->full_xstr = false;
    } else {
        dat->trace = flags.f.trace_print;
        dat->normal = flags.f.normal_print;
        dat->full_xstr = true;
        if (flags.f.trace_print) {
            std::set<int4> *target_lines = new (std::nothrow) std::set<int4>;
            if (target_lines == NULL) {
                free(dat);
                return ERR_INSUFFICIENT_MEMORY;
            }
            pgm_index saved_prgm = current_prgm;
            current_prgm = prgm;
            int4 tmppc = 0;
            int cmd;
            arg_struct arg;
            while (true) {
                get_next_command(&tmppc, &cmd, &arg, 0, NULL);
                if (cmd == CMD_END)
                    break;
                if (cmd == CMD_GTOL || cmd == CMD_XEQL)
                    target_lines->insert(arg.val.num);
            }
            current_prgm = saved_prgm;
            if (target_lines->empty())
                delete target_lines;
            else
                dat->target_lines = target_lines;
        }
    }

    current_prgm = prgm;
    prp_data = dat;

    if (normal) {
        /* Printing just one line for NORM and TRACE mode;
         * we don't do the 'interruptible' thing in this case.
         */
        int err;
        while ((err = print_program_worker(false)) == ERR_INTERRUPTIBLE);
        return err;
    } else {
        print_text(NULL, 0, true);
        mode_interruptible = print_program_worker;
        mode_stoppable = true;
        return ERR_INTERRUPTIBLE;
    }
}

static int print_program_worker(bool interrupted) {
    prp_data_struct *dat = prp_data;
    int printed = 0;

    if (interrupted)
        goto done;

    do {
        const char *orig_num;
        if (dat->line == 0)
            dat->pc = 0;
        else
            get_next_command(&dat->pc, &dat->cmd, &dat->arg, 0, &orig_num);

        char *xstr = NULL;
        if (dat->trace) {
            if (dat->cmd == CMD_LBL || dat->first
                    || dat->target_lines != NULL && dat->target_lines->find(dat->line) != dat->target_lines->end()) {
                if (dat->len > 0) {
                    print_lines(dat->buf, dat->len, true);
                    printed = 1;
                }
                if (!dat->first)
                    print_text(NULL, 0, true);
                dat->first = 0;
                dat->buf[0] = ' ';
                dat->len = 1 + prgmline2buf(dat->buf + 1, 100 - 1, dat->line,
                                            dat->cmd == CMD_LBL, dat->cmd,
                                            &dat->arg, orig_num);
                if (dat->cmd == CMD_LBL || dat->cmd == CMD_END
                        || dat->lines == 1) {
                    print_lines(dat->buf, dat->len, true);
                    printed = 1;
                    dat->len = 0;
                }
            } else {
                int i, len2;
                if (dat->len > 0) {
                    dat->buf[dat->len++] = ' ';
                    dat->buf[dat->len++] = ' ';
                }
                len2 = prgmline2buf(dat->buf + dat->len, 100 - dat->len, -1, 0,
                                    dat->cmd, &dat->arg, orig_num,
                                    false, true, dat->full_xstr ? &xstr : NULL);
                if (dat->len > 0 && dat->len + len2 > dat->width) {
                    /* Break line before current instruction */
                    print_lines(dat->buf, dat->len - 2, true);
                    printed = 1;
                    if (xstr == NULL) {
                        for (i = 0; i < len2; i++)
                            dat->buf[i] = dat->buf[dat->len + i];
                        dat->len = len2;
                    } else {
                        goto print_xstr;
                    }
                } else if (xstr != NULL) {
                    print_xstr:
                    int plen = (len2 / dat->width) * dat->width;
                    print_lines(xstr, plen, true);
                    memcpy(dat->buf, xstr + plen, len2 - plen);
                    dat->len = len2 - plen;
                } else
                    dat->len += len2;
                if (dat->lines == 1 || dat->cmd == CMD_END) {
                    print_lines(dat->buf, dat->len, true);
                    printed = 1;
                } else if (dat->len >= dat->width) {
                    len2 = (dat->len / dat->width) * dat->width;
                    print_lines(dat->buf, len2, true);
                    printed = 1;
                    for (i = len2; i < dat->len; i++)
                        dat->buf[i - len2] = dat->buf[i];
                    dat->len -= len2;
                }
            }
        } else {
            dat->len = prgmline2buf(dat->buf, 100, dat->line,
                                    dat->cmd == CMD_LBL, dat->cmd, &dat->arg,
                                    orig_num, false, true,
                                    dat->full_xstr ? &xstr : NULL);
            char *buf2 = xstr == NULL ? dat->buf : xstr;
            if (dat->normal) {
                /* In normal mode, programs are printed right-justified;
                 * we pad the instuctions to a minimum of 8 characters so
                 * the listing won't look too ragged.
                 * First, find the beginning of the instruction -- it starts
                 * right after the first space or 'goose' (6) character.
                 */
                int p = 0;
                while (buf2[p] != ' ' && buf2[p] != 6)
                    p++;
                while (dat->len < p + 9)
                    buf2[dat->len++] = ' ';
                /* Insert blank line above LBLs */
                if (dat->cmd == CMD_LBL && !dat->first)
                    print_text(NULL, 0, true);
                dat->first = 0;
            }
            print_lines(buf2, dat->len, !dat->normal);
            printed = 1;
        }
        free(xstr);
        dat->line++;
        dat->lines--;

    } while (!printed);

    if (dat->lines != 0 && dat->cmd != CMD_END)
        return ERR_INTERRUPTIBLE;

    done:
    current_prgm = dat->saved_prgm;
    delete dat;
    set_annunciators(-1, -1, 0, -1, -1, -1);
    return ERR_STOP;
}

void print_program_line(pgm_index prgm, int4 pc) {
    print_program(prgm, pc, 1, true);
}

int command2buf(char *buf, int len, int cmd, const arg_struct *arg) {
    int bufptr = 0;

    int4 xrom_arg;
    if ((cmd_array[cmd].code1 & 0xf8) == 0xa0 && (cmd_array[cmd].flags & FLAG_HIDDEN) != 0) {
        xrom_arg = (cmd_array[cmd].code1 << 8) | cmd_array[cmd].code2;
        cmd = CMD_XROM;
    } else if (cmd == CMD_XROM) {
        if (arg->type == ARGTYPE_NUM) {
            xrom_arg = arg->val.num;
        } else {
            string2buf(buf, len, &bufptr, "XROM 0x", 7);
            for (int i = 0; i < arg->length; i++) {
                char2buf(buf, len, &bufptr, "0123456789abcdef"[(arg->val.text[i] >> 4) & 15]);
                char2buf(buf, len, &bufptr, "0123456789abcdef"[arg->val.text[i] & 15]);
            }
            return bufptr;
        }
    }

    const command_spec *cmdspec = &cmd_array[cmd];
    if (cmd >= CMD_ASGN01 && cmd <= CMD_ASGN18) {
        string2buf(buf, len, &bufptr, "ASSIGN ", 7);
    } else {
        for (int i = 0; i < cmdspec->name_length; i++) {
            int c = (unsigned char) cmdspec->name[i];
            if (undefined_char(c))
                c &= 127;
            char2buf(buf, len, &bufptr, c);
        }
    }

    if (cmd == CMD_XROM) {
        int n = xrom_arg & 0x7ff;
        int rom = n >> 6;
        int instr = n & 63;
        char2buf(buf, len, &bufptr, ' ');
        char2buf(buf, len, &bufptr, '0' + rom / 10);
        char2buf(buf, len, &bufptr, '0' + rom % 10);
        char2buf(buf, len, &bufptr, ',');
        char2buf(buf, len, &bufptr, '0' + instr / 10);
        char2buf(buf, len, &bufptr, '0' + instr % 10);
    } else if (cmd == CMD_EMBED) {
        if (arg->type == ARGTYPE_IND_NUM)
            string2buf(buf, len, &bufptr, "EVAL ", 5);
        equation_data *eqd = eq_dir->prgms[arg->val.num].eq_data;
        char quot = eqd->compatMode ? '`' : '\'';
        char2buf(buf, len, &bufptr, quot);
        string2buf(buf, len, &bufptr, eqd->text, eqd->length);
        char2buf(buf, len, &bufptr, quot);
    } else if (cmdspec->argtype != ARG_NONE) {
        if (cmdspec->name_length > 0)
            char2buf(buf, len, &bufptr, ' ');
        if (arg->type == ARGTYPE_IND_NUM
                || arg->type == ARGTYPE_IND_STK
                || arg->type == ARGTYPE_IND_STR)
            string2buf(buf, len, &bufptr, "IND ", 4);
        if (arg->type == ARGTYPE_NUM
                || arg->type == ARGTYPE_IND_NUM) {
            int digits = arg->type == ARGTYPE_IND_NUM ? 2
                        : cmdspec->argtype == ARG_NUM9 ? 1
                        : 2;
            int d = 1, i;
            for (i = 0; i < digits - 1; i++)
                d *= 10;
            while (arg->val.num >= d * 10) {
                d *= 10;
                digits++;
            }
            for (i = 0; i < digits; i++) {
                char2buf(buf, len, &bufptr,
                                (char) ('0' + (arg->val.num / d) % 10));
                d /= 10;
            }
        } else if (arg->type == ARGTYPE_STK
                || arg->type == ARGTYPE_IND_STK) {
            string2buf(buf, len, &bufptr, "ST ", 3);
            char2buf(buf, len, &bufptr, arg->val.stk);
        } else if (arg->type == ARGTYPE_STR
                || arg->type == ARGTYPE_IND_STR) {
            char2buf(buf, len, &bufptr, '"');
            string2buf(buf, len, &bufptr, arg->val.text,
                                            arg->length);
            char2buf(buf, len, &bufptr, '"');
        } else if (arg->type == ARGTYPE_LCLBL) {
            char2buf(buf, len, &bufptr, arg->val.lclbl);
        } else if (arg->type == ARGTYPE_LBLINDEX) {
            directory *dir = get_dir(arg->target);
            label_struct *lbl = dir->labels + arg->val.num;
            if (lbl->length == 0) {
                if (arg->val.num == dir->labels_count - 1)
                    string2buf(buf, len, &bufptr, ".END.", 5);
                else
                    string2buf(buf, len, &bufptr, "END", 3);
            } else {
                char2buf(buf, len, &bufptr, '"');
                string2buf(buf, len, &bufptr, lbl->name, lbl->length);
                char2buf(buf, len, &bufptr, '"');
            }
        } else if (arg->type == ARGTYPE_XSTR) {
            char2buf(buf, len, &bufptr, '"');
            string2buf(buf, len, &bufptr, arg->val.xstr,
                                            arg->length);
            char2buf(buf, len, &bufptr, '"');
        } else if (arg->type == ARGTYPE_EQN) {
            equation_data *eqd = eq_dir->prgms[arg->val.num].eq_data;
            char d = eqd->compatMode ? '`' : '\'';
            char2buf(buf, len, &bufptr, d);
            string2buf(buf, len, &bufptr, eqd->text,
                                            eqd->length);
            char2buf(buf, len, &bufptr, d);
        }
    }
    if (cmd >= CMD_ASGN01 && cmd <= CMD_ASGN18) {
        int keynum = cmd - CMD_ASGN01 + 1;
        string2buf(buf, len, &bufptr, " TO ", 4);
        char2buf(buf, len, &bufptr, (char) ('0' + keynum / 10));
        char2buf(buf, len, &bufptr, (char) ('0' + keynum % 10));
    }

    return bufptr;
}

static int get_cat_index() {
    if (mode_commandmenu != MENU_NONE)
        return MENULEVEL_COMMAND;
    else if (mode_alphamenu != MENU_NONE)
        return MENULEVEL_ALPHA;
    else if (mode_transientmenu != MENU_NONE)
        return MENULEVEL_TRANSIENT;
    else if (mode_plainmenu != MENU_NONE)
        return MENULEVEL_PLAIN;
    else if (mode_auxmenu != MENU_NONE)
        return MENULEVEL_AUX;
    else if (mode_appmenu != MENU_NONE)
        return MENULEVEL_APP;
    else
        return -1;
}

void set_menu(int level, int menuid) {
    int err = set_menu_return_err(level, menuid, false);
    if (err != ERR_NONE) {
        display_error(err);
        flush_display();
    }
}

int set_menu_return_err(int level, int menuid, bool exitall) {
    int err;

    switch (level) {
        case MENULEVEL_COMMAND:
            mode_commandmenu = menuid;
            goto lbl_04;
        case MENULEVEL_ALPHA:
            mode_alphamenu = menuid;
            goto lbl_03;
        case MENULEVEL_TRANSIENT:
            mode_transientmenu = menuid;
            goto lbl_02;
        case MENULEVEL_PLAIN:
            mode_plainmenu = menuid;
            goto lbl_01;
        case MENULEVEL_AUX:
            mode_auxmenu = menuid;
            goto lbl_00;
        case MENULEVEL_APP:
            err = set_appmenu(menuid, exitall);
            if (err != ERR_NONE)
                return err;
    }
            mode_auxmenu = MENU_NONE;
    lbl_00: mode_plainmenu = MENU_NONE;
    lbl_01: mode_transientmenu = MENU_NONE;
    lbl_02: mode_alphamenu = MENU_NONE;
    lbl_03: mode_commandmenu = MENU_NONE;
    lbl_04:

    int newmenu = get_front_menu();
    if (newmenu != MENU_NONE) {
        if (newmenu == MENU_CATALOG) {
            int index = get_cat_index();
            mode_updown = index != -1 && catalogmenu_rows[index] > 1;
        } else if (newmenu == MENU_PROGRAMMABLE) {
            /* The programmable menu's up/down annunciator is on if the UP
             * and/or DOWN keys have been assigned to.
             * This is something the original HP-42S doesn't do, but I couldn't
             * resist this little improvement, perfect compatibility be damned.
             * In my defense, the Programming Examples and Techniques book,
             * bottom of page 34, does state that this should work.
             * Can't say whether the fact that it doesn't work on the real
             * HP-42S is a bug, or whether the coders and the documentation
             * writers just had a misunderstanding.
             */
            mode_updown = progmenu_arg[6].type != ARGTYPE_NONE
                        || progmenu_arg[7].type != ARGTYPE_NONE;
        } else {
            /* The up/down annunciator for catalogs depends on how many
             * items they contain; this is handled in draw_catalog().
             */
            mode_updown = newmenu == MENU_VARMENU
                                ? varmenu_rows > 1
                                : menus[newmenu].next != MENU_NONE;
        }
    } else
        mode_updown = false;
    set_annunciators(mode_updown, -1, -1, -1, -1, -1);
    catsect_when_units_key_was_pressed = -1;
    return ERR_NONE;
}

void set_appmenu_exitcallback(int callback_id) {
    appmenu_exitcallback = callback_id;
}

void set_plainmenu(int menuid, const char *name, int length) {
    if (name != NULL)
        print_menu_trace(name, length);

    mode_commandmenu = MENU_NONE;
    mode_alphamenu = MENU_NONE;
    mode_transientmenu = MENU_NONE;

    if (menuid == mode_plainmenu) {
        mode_plainmenu_sticky = 1;
        redisplay();
    } else if (menuid == MENU_CUSTOM1
            || menuid == MENU_CUSTOM2
            || menuid == MENU_CUSTOM3) {
        mode_plainmenu = menuid;
        mode_plainmenu_sticky = 1;
        redisplay();
        mode_updown = 1;
        set_annunciators(1, -1, -1, -1, -1, -1);
    } else {
        /* Even if it's a different menu than the current one, it should
         * still stick if it belongs to the same group.
         */
        if (mode_plainmenu != MENU_NONE) {
            int menu1 = mode_plainmenu;
            int menu2 = menuid;
            int parent;
            while ((parent = menus[menu1].parent) != MENU_NONE)
                menu1 = parent;
            while ((parent = menus[menu2].parent) != MENU_NONE)
                menu2 = parent;
            if (menu1 == menu2)
                mode_plainmenu_sticky = 1;
            else if (menus[menu1].next == MENU_NONE)
                mode_plainmenu_sticky = 0;
            else {
                int m = menu1;
                mode_plainmenu_sticky = 0;
                do {
                    m = menus[m].next;
                    if (m == menu2) {
                        mode_plainmenu_sticky = 1;
                        break;
                    }
                } while (m != menu1);
            }
        } else
            mode_plainmenu_sticky = 0;
        if (!mode_plainmenu_sticky) {
            mode_plainmenu = menuid;
            if (mode_plainmenu == MENU_CATALOG)
                set_cat_section(CATSECT_TOP);
            redisplay();
        }
        mode_updown = mode_plainmenu == MENU_CATALOG
                || mode_plainmenu != MENU_NONE
                        && menus[mode_plainmenu].next != MENU_NONE;
        set_annunciators(mode_updown, -1, -1, -1, -1, -1);
    }
}

void set_catalog_menu(int section) {
    mode_commandmenu = MENU_CATALOG;
    move_cat_row(0);
    if (section == CATSECT_VARS_ONLY && incomplete_command == CMD_HEAD)
        section = CATSECT_LIST_STR_ONLY;
    set_cat_section(section);
    if (section >= CATSECT_UNITS_1 && section <= CATSECT_UNITS_VISC)
        return;
    switch (section) {
        case CATSECT_TOP:
        case CATSECT_FCN:
        case CATSECT_PGM:
        case CATSECT_PGM_ONLY:
        case CATSECT_MORE:
        case CATSECT_DIRS:
        case CATSECT_EXT_1:
        case CATSECT_EXT_TIME:
        case CATSECT_EXT_XFCN:
        case CATSECT_EXT_BASE:
        case CATSECT_EXT_PRGM:
        case CATSECT_EXT_STR:
        case CATSECT_EXT_STK:
        case CATSECT_EXT_2:
        case CATSECT_EXT_EQN:
        case CATSECT_EXT_UNIT:
        case CATSECT_EXT_STAT:
        case CATSECT_EXT_DIR:
        case CATSECT_EXT_DISP:
        case CATSECT_EXT_MISC:
        case CATSECT_EXT_0_CMP:
        case CATSECT_EXT_X_CMP:
            return;
        case CATSECT_REAL:
        case CATSECT_REAL_ONLY:
        case CATSECT_CPX:
        case CATSECT_MAT:
        case CATSECT_MAT_ONLY:
        case CATSECT_MAT_LIST:
        case CATSECT_MAT_LIST_ONLY:
        case CATSECT_EQN:
        case CATSECT_EQN_ONLY:
        case CATSECT_OTHER:
        case CATSECT_VARS_ONLY:
        case CATSECT_LIST_STR_ONLY:
        case CATSECT_LIST:
        case CATSECT_LIST_ONLY:
            if (!vars_exist(section))
                mode_commandmenu = MENU_NONE;
            return;
        case CATSECT_DIRS_ONLY:
            if (cwd->children_count == 0)
                mode_commandmenu = MENU_NONE;
            return;
        case CATSECT_PGM_SOLVE:
        case CATSECT_PGM_INTEG:
        case CATSECT_PGM_MENU:
        default:
            mode_commandmenu = MENU_NONE;
            return;
    }
}

int get_front_menu() {
    if (mode_commandmenu != MENU_NONE)
        return mode_commandmenu;
    if (mode_alphamenu != MENU_NONE)
        return mode_alphamenu;
    if (mode_transientmenu != MENU_NONE)
        return mode_transientmenu;
    if (mode_plainmenu != MENU_NONE)
        return mode_plainmenu;
    if (mode_auxmenu != MENU_NONE)
        return mode_auxmenu;
    return mode_appmenu;
}

void set_cat_section(int section) {
    int index = get_cat_index();
    if (index == -1)
        return;
    if (catsect_when_units_key_was_pressed != -1) {
        if (section < CATSECT_UNITS_1 || section > CATSECT_UNITS_VISC) {
            if (section == CATSECT_TOP || section == CATSECT_MORE)
                section = catsect_when_units_key_was_pressed;
            catsect_when_units_key_was_pressed = -1;
        }
    }
    if (index == MENULEVEL_AUX && catalog_no_top) {
        int old_section = catalogmenu_section[index];
        bool going_to_top = section == CATSECT_TOP || section == CATSECT_MORE;
        if (old_section == CATSECT_DIRS) {
            if (going_to_top) {
                skip_top:
                if (get_front_menu() == MENU_CATALOG)
                    set_menu(MENULEVEL_AUX, MENU_NONE);
                catalog_no_top = false;
                return;
            } else if (section != old_section)
                catalog_no_top = false;
        } else if (old_section >= CATSECT_UNITS_1 && old_section <= CATSECT_UNITS_VISC) {
            if (old_section >= CATSECT_UNITS_1 && old_section <= CATSECT_UNITS_3 && going_to_top)
                goto skip_top;
            else if (section < CATSECT_UNITS_1 || section > CATSECT_UNITS_VISC)
                catalog_no_top = false;
        } else
            catalog_no_top = false;
    }
    catalogmenu_section[index] = section;
}

void set_cat_section_no_top(int section) {
    int index = get_cat_index();
    if (index == -1)
        return;
    catalogmenu_section[index] = section;
    catalog_no_top = true;
}

void set_cat_section_using_units_key() {
    int oldsect = get_cat_section();
    set_cat_section(CATSECT_UNITS_1);
    catsect_when_units_key_was_pressed = oldsect;
}

int get_cat_section() {
    int index = get_cat_index();
    if (index != -1)
        return catalogmenu_section[index];
    else
        return CATSECT_TOP;
}

void move_cat_row(int direction) {
    int index = get_cat_index();
    if (index == -1)
        return;
    if (direction == 0)
        catalogmenu_row[index] = 0;
    else if (direction == -1) {
        catalogmenu_row[index]--;
        if (catalogmenu_row[index] < 0)
            catalogmenu_row[index] = catalogmenu_rows[index] - 1;
    } else {
        catalogmenu_row[index]++;
        if (catalogmenu_row[index] >= catalogmenu_rows[index])
            catalogmenu_row[index] = 0;
    }
}

void set_cat_row(int row) {
    int index = get_cat_index();
    if (index == -1)
        return;
    catalogmenu_row[index] = row;
}

int get_cat_row() {
    int index = get_cat_index();
    if (index == -1)
        return 0;
    else
        return catalogmenu_row[index];
}

bool get_cat_item(int menukey, int4 *dir, int *item) {
    int index = get_cat_index();
    if (index == -1)
        return false;
    int it = catalogmenu_item[index][menukey];
    if (it == -1)
        return false;
    if (dir != NULL)
        *dir = catalogmenu_dir[index][menukey];
    *item = it;
    return true;
}

void update_catalog() {
    int *the_menu;
    if (mode_commandmenu != MENU_NONE)
        the_menu = &mode_commandmenu;
    else if (mode_alphamenu != MENU_NONE)
        the_menu = &mode_alphamenu;
    else if (mode_transientmenu != MENU_NONE)
        the_menu = &mode_transientmenu;
    else if (mode_plainmenu != MENU_NONE)
        the_menu = &mode_plainmenu;
    else if (mode_auxmenu != MENU_NONE)
        the_menu = &mode_auxmenu;
    else if (mode_appmenu != MENU_NONE)
        the_menu = &mode_appmenu;
    else
        return;
    if (*the_menu != MENU_CATALOG)
        return;
    int section = get_cat_section();
    switch (section) {
        case CATSECT_TOP:
        case CATSECT_FCN:
        case CATSECT_MORE:
        case CATSECT_EXT_1:
        case CATSECT_EXT_TIME:
        case CATSECT_EXT_XFCN:
        case CATSECT_EXT_BASE:
        case CATSECT_EXT_PRGM:
        case CATSECT_EXT_STR:
        case CATSECT_EXT_STK:
        case CATSECT_EXT_2:
        case CATSECT_EXT_EQN:
        case CATSECT_EXT_UNIT:
        case CATSECT_EXT_STAT:
        case CATSECT_EXT_DIR:
        case CATSECT_EXT_DISP:
        case CATSECT_EXT_MISC:
        case CATSECT_EXT_0_CMP:
        case CATSECT_EXT_X_CMP:
            return;
        case CATSECT_PGM:
        case CATSECT_PGM_ONLY:
        case CATSECT_DIRS:
            break;
        case CATSECT_REAL:
        case CATSECT_CPX:
        case CATSECT_MAT:
            if (!vars_exist(section))
                set_cat_section(CATSECT_TOP);
            break;
        case CATSECT_MAT_LIST:
            if (!vars_exist(section))
                set_cat_section(CATSECT_TOP);
            break;
        case CATSECT_LIST:
        case CATSECT_EQN:
        case CATSECT_OTHER:
            if (!vars_exist(section))
                set_cat_section(CATSECT_MORE);
            break;
        case CATSECT_REAL_ONLY:
        case CATSECT_MAT_ONLY:
        case CATSECT_MAT_LIST_ONLY:
        case CATSECT_EQN_ONLY:
        case CATSECT_VARS_ONLY:
        case CATSECT_LIST_STR_ONLY:
        case CATSECT_LIST_ONLY:
            if (!vars_exist(section)) {
                *the_menu = MENU_NONE;
                redisplay();
                return;
            }
            break;
        case CATSECT_DIRS_ONLY:
            if (cwd->children_count == 0) {
                *the_menu = MENU_NONE;
                redisplay();
                return;
            }
            break;
        case CATSECT_PGM_SOLVE:
        case CATSECT_PGM_INTEG:
        case CATSECT_PGM_MENU:
            /* No longer applicable now that these menus
             * are never empty, thanks to the equation
             * editor's [=] key.
            if (!mvar_prgms_exist()) {
                *the_menu = MENU_NONE;
                display_error(ERR_NO_MENU_VARIABLES);
                redisplay();
                return;
            }
            */
            break;
    }
    draw_catalog();
}

void clear_custom_menu() {
    int row, key;
    for (row = 0; row < 3; row++)
        for (key = 0; key < 6; key++)
            custommenu_length[row][key] = 0;
}

void assign_custom_key(int keynum, const char *name, int length) {
    int row = (keynum - 1) / 6;
    int key = (keynum - 1) % 6;
    int i;
    custommenu_length[row][key] = length;
    for (i = 0; i < length; i++)
        custommenu_label[row][key][i] = name[i];
}

void get_custom_key(int keynum, char *name, int *length) {
    int row = (keynum - 1) / 6;
    int key = (keynum - 1) % 6;
    string_copy(name, length, custommenu_label[row][key],
                              custommenu_length[row][key]);
}

void clear_prgm_menu() {
    int i;
    for (i = 0; i < 9; i++)
        progmenu_arg[i].type = ARGTYPE_NONE;
    for (i = 0; i < 6; i++)
        progmenu_length[i] = 0;
}

void assign_prgm_key(int keynum, bool is_gto, const arg_struct *arg) {
    int length, i;
    keynum--;
    progmenu_arg[keynum] = (arg_struct) *arg;
    progmenu_is_gto[keynum] = is_gto;
    length = reg_alpha_length;
    if (keynum < 6) {
        if (length > 7)
            length = 7;
        for (i = 0; i < length; i++)
            progmenu_label[keynum][i] = reg_alpha[i];
        progmenu_length[keynum] = length;
    }
}

void do_prgm_menu_key(int keynum) {
    int err;
    pgm_index oldprgm;
    int4 oldpc;
    keynum--;
    if (keynum == 8)
        set_menu(MENULEVEL_PLAIN, MENU_NONE);
    if (progmenu_arg[keynum].type == ARGTYPE_NONE) {
        if (keynum < 6)
            pending_command = CMD_NULL;
        else if (keynum == 8)
            pending_command = CMD_CANCELLED;
        return;
    }
    if ((flags.f.trace_print || flags.f.normal_print) && flags.f.printer_exists)
        print_command(progmenu_is_gto[keynum] ? CMD_GTO : CMD_XEQ,
                                                &progmenu_arg[keynum]);
    oldprgm = current_prgm;
    oldpc = pc;
    set_running(true);
    progmenu_arg[keynum].target = -1; /* Force docmd_gto() to search */
    err = docmd_gto(&progmenu_arg[keynum]);
    if (err != ERR_NONE) {
        set_running(false);
        display_error(err);
        flush_display();
        return;
    }
    if (!progmenu_is_gto[keynum]) {
        err = push_rtn_addr(oldprgm, oldpc == -1 ? 0 : oldpc);
        if (err != ERR_NONE) {
            current_prgm = oldprgm;
            pc = oldpc;
            set_running(false);
            display_error(err);
            flush_display();
            return;
        } else
            save_csld();
    }
}

int docmd_life(arg_struct *arg) {
    char *dest = (char *) malloc(disp_h * disp_bpl + 3);
    if (dest == NULL)
        return ERR_INSUFFICIENT_MEMORY;

    int hwords = (disp_w + 31) >> 5;
    int excess = hwords * 4 - disp_bpl;
    int index = 0;
    int aboveindex = -disp_bpl;
    int belowindex = disp_bpl;
    uint4 rightedgemask = 0xffffffff >> (31 - (disp_w - 1 & 31));
    for (int y = 0; y < disp_h; y++) {
        bool notattop = y != 0;
        bool notatbottom = y != disp_h - 1;
        for (int x = 0; x < hwords; x++) {
            uint4 s0, s1, s2, s3, r;
            uint4 w00, w01, w02, w10, w11, w12, w20, w21, w22;
            w10 = notattop ? *((uint4 *) (display + aboveindex)) : 0;
            w11 = *((uint4 *) (display + index));
            w12 = notatbottom ? *((uint4 *) (display + belowindex)) : 0;
            if (x == hwords - 1) {
                w10 &= rightedgemask;
                w11 &= rightedgemask;
                w12 &= rightedgemask;
            }
            w00 = w10 << 1;
            w01 = w11 << 1;
            w02 = w12 << 1;
            if (x != 0) {
                if (notattop && *((uint4 *) (display + aboveindex - 4)) & 0x80000000)
                    w00 |= 1;
                if (*((uint4 *) (display + index - 4)) & 0x80000000)
                    w01 |= 1;
                if (notatbottom && *((uint4 *) (display + belowindex - 4)) & 0x80000000)
                    w02 |= 1;
            }
            w20 = w10 >> 1;
            w21 = w11 >> 1;
            w22 = w12 >> 1;
            if (x != hwords - 1) {
                if (notattop && *((uint4 *) (display + aboveindex + 4)) & 1)
                    w20 |= 0x80000000;
                if (*((uint4 *) (display + index + 4)) & 1)
                    w21 |= 0x80000000;
                if (notatbottom && *((uint4 *) (display + belowindex + 4)) & 1)
                    w22 |= 0x80000000;
            }

            s1 =      w00;
            s0 =                 ~w00;

            s2 = s1 & w01;
            s1 = s0 & w01 | s1 & ~w01;
            s0 =            s0 & ~w01;

            s3 = s2 & w02;
            s2 = s1 & w02 | s2 & ~w02;
            s1 = s0 & w02 | s1 & ~w02;
            s0 =            s0 & ~w02;

            s3 = s2 & w10 | s3 & ~w10;
            s2 = s1 & w10 | s2 & ~w10;
            s1 = s0 & w10 | s1 & ~w10;
            s0 =            s0 & ~w10;

            s3 = s2 & w12 | s3 & ~w12;
            s2 = s1 & w12 | s2 & ~w12;
            s1 = s0 & w12 | s1 & ~w12;
            s0 =            s0 & ~w12;

            s3 = s2 & w20 | s3 & ~w20;
            s2 = s1 & w20 | s2 & ~w20;
            s1 = s0 & w20 | s1 & ~w20;
            s0 =            s0 & ~w20;

            s3 = s2 & w21 | s3 & ~w21;
            s2 = s1 & w21 | s2 & ~w21;
            s1 = s0 & w21 | s1 & ~w21;

            s3 = s2 & w22 | s3 & ~w22;
            s2 = s1 & w22 | s2 & ~w22;

            r = s3 | s2 & w11;
            if (x == hwords - 1)
                r &= rightedgemask;
            *((uint4 *) (dest + index)) = r;

            index += 4;
            aboveindex += 4;
            belowindex += 4;
        }
        index -= excess;
        aboveindex -= excess;
        belowindex -= excess;
    }

    memcpy(display, dest, disp_h * disp_bpl);
    free(dest);
    repaint_display();
    is_dirty = false;
    mode_message_lines = ALL_LINES;
    return ERR_NONE;
}
