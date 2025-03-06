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
#include <stdint.h>
#include <string>

#include "core_globals.h"
#include "core_commands2.h"
#include "core_commands4.h"
#include "core_commands7.h"
#include "core_commandsa.h"
#include "core_display.h"
#include "core_equations.h"
#include "core_helpers.h"
#include "core_main.h"
#include "core_math1.h"
#include "core_parser.h"
#include "core_tables.h"
#include "core_variables.h"
#include "shell.h"
#include "shell_spool.h"

#ifndef BCD_MATH
// We need these locally for BID128->double conversion
#include "bid_conf.h"
#include "bid_functions.h"
#endif

// File used for reading and writing the state file, and for importing and
// exporting programs. Since only one of these operations can be active at one
// time, having one FILE pointer for all of them is sufficient.
FILE *gfile = NULL;

const error_spec errors[] = {
    { /* NONE */                   NULL,                       0 },
    { /* ALPHA_DATA_IS_INVALID */  "Alpha Data Is Invalid",   21 },
    { /* OUT_OF_RANGE */           "Out of Range",            12 },
    { /* DIVIDE_BY_0 */            "Divide by 0",             11 },
    { /* INVALID_TYPE */           "Invalid Type",            12 },
    { /* INVALID_DATA */           "Invalid Data",            12 },
    { /* NONEXISTENT */            "Nonexistent",             11 },
    { /* DIMENSION_ERROR */        "Dimension Error",         15 },
    { /* TOO_FEW_ARGUMENTS */      "Too Few Arguments",       17 },
    { /* SIZE_ERROR */             "Size Error",              10 },
    { /* STACK_DEPTH_ERROR */      "Stack Depth Error",       17 },
    { /* RESTRICTED_OPERATION */   "Restricted Operation",    20 },
    { /* YES */                    "Yes",                      3 },
    { /* NO */                     "No",                       2 },
    { /* STOP */                   NULL,                       0 },
    { /* LABEL_NOT_FOUND */        "Label Not Found",         15 },
    { /* NO_REAL_VARIABLES */      "No Real Variables",       17 },
    { /* NO_COMPLEX_VARIABLES */   "No Complex Variables",    20 },
    { /* NO_MATRIX_VARIABLES */    "No Matrix Variables",     19 },
    { /* NO_LIST_VARIABLES */      "No List Variables",       17 },
    { /* NO_EQUATION_VARIABLES */  "No Equation Variables",   21 },
    { /* NO_NAMED_EQUATIONS */     "No Named Equations",      18 },
    { /* NO_OTHER_VARIABLES */     "No Other Variables",      18 },
    { /* NO_MENU_VARIABLES */      "No Menu Variables",       17 },
    { /* STAT_MATH_ERROR */        "Stat Math Error",         15 },
    { /* INVALID_FORECAST_MODEL */ "Invalid Forecast Model",  22 },
    { /* SINGULAR_MATRIX */        "Singular Matrix",         15 },
    { /* SOLVE_SOLVE */            "Solve(Solve)",            12 },
    { /* INTEG_INTEG */            "Integ(Integ)",            12 },
    { /* RUN */                    NULL,                       0 },
    { /* INTERRUPTED */            "Interrupted",             11 },
    { /* PRINTING_IS_DISABLED */   "Printing Is Disabled",    20 },
    { /* INTERRUPTIBLE */          NULL,                       0 },
    { /* NO_VARIABLES */           "No Variables",            12 },
    { /* INSUFFICIENT_MEMORY */    "Insufficient Memory",     19 },
    { /* NOT_YET_IMPLEMENTED */    "Not Yet Implemented",     19 },
    { /* INTERNAL_ERROR */         "Internal Error",          14 },
    { /* SUSPICIOUS_OFF */         "Suspicious OFF",          14 },
    { /* RTN_STACK_FULL */         "RTN Stack Full",          14 },
    { /* NUMBER_TOO_LARGE */       "Number Too Large",        16 },
    { /* NUMBER_TOO_SMALL */       "Number Too Small",        16 },
    { /* INVALID_CONTEXT */        "Invalid Context",         15 },
    { /* NAME_TOO_LONG */          "Name Too Long",           13 },
    { /* PARSE_ERROR */            "Parse Error",             11 },
    { /* INVALID_EQUATION */       "Invalid Equation",        16 },
    { /* INCONSISTENT_UNITS */     "Inconsistent Units",      18 },
    { /* INVALID_UNIT */           "Invalid Unit",            12 },
    { /* VARIABLE_NOT_WRITABLE */  "Variable Not Writable",   21 },
    { /* DIRECTORY_EXISTS */       "Directory Exists",        16 },
    { /* VARIABLE_EXISTS */        "Variable Exists",         15 },
    { /* TOO_MANY_ARGUMENTS */     "Too Many Arguments",      18 },
    { /* NO_SOLUTION_FOUND */      "No Solution Found",       17 },
    { /* PROGRAM_LOCKED */         "Program Locked",          14 },
    { /* NEXT_PROGRAM_LOCKED */    "Next Program Locked",     19 },
};


const menu_spec menus[] = {
    { /* MENU_ALPHA1 */ MENU_NONE, MENU_ALPHA2, MENU_ALPHA2,
                      { { MENU_ALPHA_ABCDE1, 5, "ABCDE" },
                        { MENU_ALPHA_FGHI,   4, "FGHI"  },
                        { MENU_ALPHA_JKLM,   4, "JKLM"  },
                        { MENU_ALPHA_NOPQ1,  4, "NOPQ"  },
                        { MENU_ALPHA_RSTUV1, 5, "RSTUV" },
                        { MENU_ALPHA_WXYZ,   4, "WXYZ"  } } },
    { /* MENU_ALPHA2 */ MENU_NONE, MENU_ALPHA1, MENU_ALPHA1,
                      { { MENU_ALPHA_PAREN, 5, "( [ {"   },
                        { MENU_ALPHA_ARROW, 3, "\20^\16" },
                        { MENU_ALPHA_COMP,  5, "< = >"   },
                        { MENU_ALPHA_MATH1, 4, "MATH"    },
                        { MENU_ALPHA_PUNC1, 4, "PUNC"    },
                        { MENU_ALPHA_MISC1, 4, "MISC"    } } },
    { /* MENU_ALPHA_ABCDE1 */ MENU_ALPHA1, MENU_ALPHA_ABCDE2, MENU_ALPHA_ABCDE2,
                      { { MENU_NONE, 1, "A" },
                        { MENU_NONE, 1, "B" },
                        { MENU_NONE, 1, "C" },
                        { MENU_NONE, 1, "D" },
                        { MENU_NONE, 1, "E" },
                        { MENU_NONE, 1, " " } } },
    { /* MENU_ALPHA_ABCDE2 */ MENU_ALPHA1, MENU_ALPHA_ABCDE1, MENU_ALPHA_ABCDE1,
                      { { MENU_NONE, 1, "\26" },
                        { MENU_NONE, 1, "\24" },
                        { MENU_NONE, 1, "\31" },
                        { MENU_NONE, 1, " "   },
                        { MENU_NONE, 1, " "   },
                        { MENU_NONE, 1, " "   } } },
    { /* MENU_ALPHA_FGHI */ MENU_ALPHA1, MENU_NONE, MENU_NONE,
                      { { MENU_NONE, 1, "F" },
                        { MENU_NONE, 1, "G" },
                        { MENU_NONE, 1, "H" },
                        { MENU_NONE, 1, "I" },
                        { MENU_NONE, 1, " " },
                        { MENU_NONE, 1, " " } } },
    { /* MENU_ALPHA_JKLM */ MENU_ALPHA1, MENU_NONE, MENU_NONE,
                      { { MENU_NONE, 1, "J" },
                        { MENU_NONE, 1, "K" },
                        { MENU_NONE, 1, "L" },
                        { MENU_NONE, 1, "M" },
                        { MENU_NONE, 1, " " },
                        { MENU_NONE, 1, " " } } },
    { /* MENU_ALPHA_NOPQ1 */ MENU_ALPHA1, MENU_ALPHA_NOPQ2, MENU_ALPHA_NOPQ2,
                      { { MENU_NONE, 1, "N" },
                        { MENU_NONE, 1, "O" },
                        { MENU_NONE, 1, "P" },
                        { MENU_NONE, 1, "Q" },
                        { MENU_NONE, 1, " " },
                        { MENU_NONE, 1, " " } } },
    { /* MENU_ALPHA_NOPQ2 */ MENU_ALPHA1, MENU_ALPHA_NOPQ1, MENU_ALPHA_NOPQ1,
                      { { MENU_NONE, 1, "\25" },
                        { MENU_NONE, 1, "\34" },
                        { MENU_NONE, 1, " "   },
                        { MENU_NONE, 1, " "   },
                        { MENU_NONE, 1, " "   },
                        { MENU_NONE, 1, " "   } } },
    { /* MENU_ALPHA_RSTUV1 */ MENU_ALPHA1, MENU_ALPHA_RSTUV2, MENU_ALPHA_RSTUV2,
                      { { MENU_NONE, 1, "R" },
                        { MENU_NONE, 1, "S" },
                        { MENU_NONE, 1, "T" },
                        { MENU_NONE, 1, "U" },
                        { MENU_NONE, 1, "V" },
                        { MENU_NONE, 1, " " } } },
    { /* MENU_ALPHA_RSTUV2 */ MENU_ALPHA1, MENU_ALPHA_RSTUV1, MENU_ALPHA_RSTUV1,
                      { { MENU_NONE, 1, " "   },
                        { MENU_NONE, 1, " "   },
                        { MENU_NONE, 1, " "   },
                        { MENU_NONE, 1, "\35" },
                        { MENU_NONE, 1, " "   },
                        { MENU_NONE, 1, " "   } } },
    { /* MENU_ALPHA_WXYZ */ MENU_ALPHA1, MENU_NONE, MENU_NONE,
                      { { MENU_NONE, 1, "W" },
                        { MENU_NONE, 1, "X" },
                        { MENU_NONE, 1, "Y" },
                        { MENU_NONE, 1, "Z" },
                        { MENU_NONE, 1, " " },
                        { MENU_NONE, 1, " " } } },
    { /* MENU_ALPHA_PAREN */ MENU_ALPHA2, MENU_NONE, MENU_NONE,
                      { { MENU_NONE, 1, "(" },
                        { MENU_NONE, 1, ")" },
                        { MENU_NONE, 1, "[" },
                        { MENU_NONE, 1, "]" },
                        { MENU_NONE, 1, "{" },
                        { MENU_NONE, 1, "}" } } },
    { /* MENU_ALPHA_ARROW */ MENU_ALPHA2, MENU_NONE, MENU_NONE,
                      { { MENU_NONE, 1, "\20" },
                        { MENU_NONE, 1, "^"   },
                        { MENU_NONE, 1, "\16" },
                        { MENU_NONE, 1, "\17" },
                        { MENU_NONE, 1, " "   },
                        { MENU_NONE, 1, "\36" } } },
    { /* MENU_ALPHA_COMP */ MENU_ALPHA2, MENU_NONE, MENU_NONE,
                      { { MENU_NONE, 1, "="   },
                        { MENU_NONE, 1, "\14" },
                        { MENU_NONE, 1, "<"   },
                        { MENU_NONE, 1, ">"   },
                        { MENU_NONE, 1, "\11" },
                        { MENU_NONE, 1, "\13" } } },
    { /* MENU_ALPHA_MATH1 */ MENU_ALPHA2, MENU_ALPHA_MATH2, MENU_ALPHA_MATH2,
                      { { MENU_NONE, 1, "\5"  },
                        { MENU_NONE, 1, "\3"  },
                        { MENU_NONE, 1, "\2"  },
                        { MENU_NONE, 1, "\27" },
                        { MENU_NONE, 1, "\23" },
                        { MENU_NONE, 1, "\21" } } },
    { /* MENU_ALPHA_MATH2 */ MENU_ALPHA2, MENU_ALPHA_MATH1, MENU_ALPHA_MATH1,
                      { { MENU_NONE, 1, "\202" },
                        { MENU_NONE, 1, "\7"   },
                        { MENU_NONE, 1, " "    },
                        { MENU_NONE, 1, " "    },
                        { MENU_NONE, 1, " "    },
                        { MENU_NONE, 1, " "    } } },
    { /* MENU_ALPHA_PUNC1 */ MENU_ALPHA2, MENU_ALPHA_PUNC2, MENU_ALPHA_PUNC3,
                      { { MENU_NONE, 1, ","  },
                        { MENU_NONE, 1, ";"  },
                        { MENU_NONE, 1, ":"  },
                        { MENU_NONE, 1, "!"  },
                        { MENU_NONE, 1, "?"  },
                        { MENU_NONE, 1, "\"" } } },
    { /* MENU_ALPHA_PUNC2 */ MENU_ALPHA2, MENU_ALPHA_PUNC3, MENU_ALPHA_PUNC1,
                      { { MENU_NONE, 1, "\32" },
                        { MENU_NONE, 1, "_"   },
                        { MENU_NONE, 1, "`"   },
                        { MENU_NONE, 1, "'"   },
                        { MENU_NONE, 1, "\10" },
                        { MENU_NONE, 1, "\12" } } },
    { /* MENU_ALPHA_PUNC3 */ MENU_ALPHA2, MENU_ALPHA_PUNC1, MENU_ALPHA_PUNC2,
                      { { MENU_NONE, 1, "\210" },
                        { MENU_NONE, 1, "\211" },
                        { MENU_NONE, 1, " "    },
                        { MENU_NONE, 1, " "    },
                        { MENU_NONE, 1, " "    },
                        { MENU_NONE, 1, " "    } } },
    { /* MENU_ALPHA_MISC1 */ MENU_ALPHA2, MENU_ALPHA_MISC2, MENU_ALPHA_MISC2,
                      { { MENU_NONE, 1, "$"   },
                        { MENU_NONE, 1, "*"   },
                        { MENU_NONE, 1, "#"   },
                        { MENU_NONE, 1, "/"   },
                        { MENU_NONE, 1, "\37" },
                        { MENU_NONE, 1, " "   } } },
    { /* MENU_ALPHA_MISC2 */ MENU_ALPHA2, MENU_ALPHA_MISC1, MENU_ALPHA_MISC1,
                      { { MENU_NONE, 1, "\22" },
                        { MENU_NONE, 1, "&"   },
                        { MENU_NONE, 1, "@"   },
                        { MENU_NONE, 1, "\\"  },
                        { MENU_NONE, 1, "~"   },
                        { MENU_NONE, 1, "|"   } } },
    { /* MENU_ST */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { MENU_NONE, 4, "ST L" },
                        { MENU_NONE, 4, "ST X" },
                        { MENU_NONE, 4, "ST Y" },
                        { MENU_NONE, 4, "ST Z" },
                        { MENU_NONE, 4, "ST T" },
                        { MENU_NONE, 0, ""     } } },
    { /* MENU_IND_ST */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { MENU_NONE, 3, "IND"  },
                        { MENU_NONE, 4, "ST L" },
                        { MENU_NONE, 4, "ST X" },
                        { MENU_NONE, 4, "ST Y" },
                        { MENU_NONE, 4, "ST Z" },
                        { MENU_NONE, 4, "ST T" } } },
    { /* MENU_IND */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { MENU_NONE, 3, "IND"  },
                        { MENU_NONE, 0, "" },
                        { MENU_NONE, 0, "" },
                        { MENU_NONE, 0, "" },
                        { MENU_NONE, 0, "" },
                        { MENU_NONE, 0, "" } } },
    { /* MENU_MODES1 */ MENU_NONE, MENU_MODES2, MENU_MODES5,
                      { { 0x2000 + CMD_DEG,   0, "" },
                        { 0x2000 + CMD_RAD,   0, "" },
                        { 0x2000 + CMD_GRAD,  0, "" },
                        { 0x1000 + CMD_NULL,  0, "" },
                        { 0x2000 + CMD_RECT,  0, "" },
                        { 0x2000 + CMD_POLAR, 0, "" } } },
    { /* MENU_MODES2 */ MENU_NONE, MENU_MODES3, MENU_MODES1,
                      { { 0x1000 + CMD_SIZE,    0, "" },
                        { 0x2000 + CMD_QUIET,   0, "" },
                        { 0x2000 + CMD_CPXRES,  0, "" },
                        { 0x2000 + CMD_REALRES, 0, "" },
                        { 0x2000 + CMD_KEYASN,  0, "" },
                        { 0x2000 + CMD_LCLBL,   0, "" } } },
    { /* MENU_MODES3 */ MENU_NONE, MENU_MODES4, MENU_MODES2,
                      { { 0x1000 + CMD_WSIZE,   0, "" },
                        { 0x1000 + CMD_WSIZE_T, 0, "" },
                        { 0x2000 + CMD_BSIGNED, 0, "" },
                        { 0x2000 + CMD_BWRAP,   0, "" },
                        { 0x1000 + CMD_NULL,    0, "" },
                        { 0x1000 + CMD_BRESET,  0, "" } } },
    { /* MENU_MODES4 */ MENU_NONE, MENU_MODES5, MENU_MODES3,
                      { { 0x2000 + CMD_MDY,     0, "" },
                        { 0x2000 + CMD_DMY,     0, "" },
                        { 0x2000 + CMD_YMD,     0, "" },
                        { 0x1000 + CMD_NULL,    0, "" },
                        { 0x2000 + CMD_CLK12,   0, "" },
                        { 0x2000 + CMD_CLK24,   0, "" } } },
    { /* MENU_MODES5 */ MENU_NONE, MENU_MODES1, MENU_MODES4,
                      { { 0x2000 + CMD_4STK,    0, "" },
                        { 0x2000 + CMD_NSTK,    0, "" },
                        { 0x2000 + CMD_STD,     0, "" },
                        { 0x2000 + CMD_COMP,    0, "" },
                        { 0x2000 + CMD_DIRECT,  0, "" },
                        { 0x2000 + CMD_NUMERIC, 0, "" } } },
    { /* MENU_DISP1 */ MENU_NONE, MENU_DISP2, MENU_DISP4,
                      { { 0x2000 + CMD_FIX,      0, "" },
                        { 0x2000 + CMD_SCI,      0, "" },
                        { 0x2000 + CMD_ENG,      0, "" },
                        { 0x2000 + CMD_ALL,      0, "" },
                        { 0x2000 + CMD_RDXDOT,   0, "" },
                        { 0x2000 + CMD_RDXCOMMA, 0, "" } } },
    { /* MENU_DISP2 */ MENU_NONE, MENU_DISP3, MENU_DISP1,
                      { { 0x1000 + CMD_ROW_PLUS,  0, "" },
                        { 0x1000 + CMD_ROW_MINUS, 0, "" },
                        { 0x1000 + CMD_COL_PLUS,  0, "" },
                        { 0x1000 + CMD_COL_MINUS, 0, "" },
                        { 0x1000 + CMD_GETDS,     0, "" },
                        { 0x1000 + CMD_SETDS,     0, "" } } },
    { /* MENU_DISP3 */ MENU_NONE, MENU_DISP4, MENU_DISP2,
                      { { 0x2000 + CMD_HEADER, 0, "" },
                        { 0x2000 + CMD_HFLAGS, 0, "" },
                        { 0x2000 + CMD_HPOLAR, 0, "" },
                        { 0x1000 + CMD_NULL,   0, "" },
                        { 0x2000 + CMD_LTOP,   0, "" },
                        { 0x2000 + CMD_ATOP,   0, "" } } },
    { /* MENU_DISP4 */ MENU_NONE, MENU_DISP1, MENU_DISP3,
                      { { 0x2000 + CMD_1LINE,  0, "" },
                        { 0x2000 + CMD_NLINE,  0, "" },
                        { 0x1000 + CMD_NULL,   0, "" },
                        { 0x1000 + CMD_WIDTH,  0, "" },
                        { 0x1000 + CMD_HEIGHT, 0, "" },
                        { 0x1000 + CMD_NULL,   0, "" } } },
    { /* MENU_CLEAR1 */ MENU_NONE, MENU_CLEAR2, MENU_CLEAR2,
                      { { 0x1000 + CMD_CLSIGMA, 0, "" },
                        { 0x1000 + CMD_CLP,     0, "" },
                        { 0x1000 + CMD_CLV,     0, "" },
                        { 0x1000 + CMD_CLST,    0, "" },
                        { 0x1000 + CMD_CLA,     0, "" },
                        { 0x1000 + CMD_CLX,     0, "" } } },
    { /* MENU_CLEAR2 */ MENU_NONE, MENU_CLEAR1, MENU_CLEAR1,
                      { { 0x1000 + CMD_CLRG,   0, "" },
                        { 0x1000 + CMD_DEL,    0, "" },
                        { 0x1000 + CMD_CLKEYS, 0, "" },
                        { 0x1000 + CMD_CLLCD,  0, "" },
                        { 0x1000 + CMD_CLMENU, 0, "" },
                        { 0x1000 + CMD_CLALLa, 0, "" } } },
    { /* MENU_CONVERT1 */ MENU_NONE, MENU_CONVERT2, MENU_CONVERT2,
                      { { 0x1000 + CMD_TO_DEG, 0, "" },
                        { 0x1000 + CMD_TO_RAD, 0, "" },
                        { 0x1000 + CMD_TO_HR,  0, "" },
                        { 0x1000 + CMD_TO_HMS, 0, "" },
                        { 0x1000 + CMD_TO_REC, 0, "" },
                        { 0x1000 + CMD_TO_POL, 0, "" } } },
    { /* MENU_CONVERT2 */ MENU_NONE, MENU_CONVERT1, MENU_CONVERT1,
                      { { 0x1000 + CMD_IP,   0, "" },
                        { 0x1000 + CMD_FP,   0, "" },
                        { 0x1000 + CMD_RND,  0, "" },
                        { 0x1000 + CMD_ABS,  0, "" },
                        { 0x1000 + CMD_SIGN, 0, "" },
                        { 0x1000 + CMD_MOD,  0, "" } } },
    { /* MENU_FLAGS */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { 0x1000 + CMD_SF,    0, "" },
                        { 0x1000 + CMD_CF,    0, "" },
                        { 0x1000 + CMD_FS_T,  0, "" },
                        { 0x1000 + CMD_FC_T,  0, "" },
                        { 0x1000 + CMD_FSC_T, 0, "" },
                        { 0x1000 + CMD_FCC_T, 0, "" } } },
    { /* MENU_PROB */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { 0x1000 + CMD_COMB,  0, "" },
                        { 0x1000 + CMD_PERM,  0, "" },
                        { 0x1000 + CMD_FACT,  0, "" },
                        { 0x1000 + CMD_GAMMA, 0, "" },
                        { 0x1000 + CMD_RAN,   0, "" },
                        { 0x1000 + CMD_SEED,  0, "" } } },
    { /* MENU_CUSTOM1 */ MENU_NONE, MENU_CUSTOM2, MENU_CUSTOM3,
                      { { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" } } },
    { /* MENU_CUSTOM2 */ MENU_NONE, MENU_CUSTOM3, MENU_CUSTOM1,
                      { { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" } } },
    { /* MENU_CUSTOM3 */ MENU_NONE, MENU_CUSTOM1, MENU_CUSTOM2,
                      { { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" } } },
    { /* MENU_PGM_FCN1 */ MENU_NONE, MENU_PGM_FCN2, MENU_PGM_FCN4,
                      { { 0x1000 + CMD_LBL,   0, "" },
                        { 0x1000 + CMD_RTN,   0, "" },
                        { 0x1000 + CMD_INPUT, 0, "" },
                        { 0x1000 + CMD_VIEW,  0, "" },
                        { 0x1000 + CMD_AVIEW, 0, "" },
                        { 0x1000 + CMD_XEQ,   0, "" } } },
    { /* MENU_PGM_FCN2 */ MENU_NONE, MENU_PGM_FCN3, MENU_PGM_FCN1,
                      { { MENU_PGM_XCOMP0,     3, "X?0" },
                        { MENU_PGM_XCOMPY,     3, "X?Y" },
                        { 0x1000 + CMD_PROMPT, 0, ""    },
                        { 0x1000 + CMD_PSE,    0, ""    },
                        { 0x1000 + CMD_ISG,    0, ""    },
                        { 0x1000 + CMD_DSE,    0, ""    } } },
    { /* MENU_PGM_FCN3 */ MENU_NONE, MENU_PGM_FCN4, MENU_PGM_FCN2,
                      { { 0x1000 + CMD_AIP,    0, "" },
                        { 0x1000 + CMD_XTOA,   0, "" },
                        { 0x1000 + CMD_AGRAPH, 0, "" },
                        { 0x1000 + CMD_PIXEL,  0, "" },
                        { 0x1000 + CMD_BEEP,   0, "" },
                        { 0x1000 + CMD_TONE,   0, "" } } },
    { /* MENU_PGM_FCN4 */ MENU_NONE, MENU_PGM_FCN1, MENU_PGM_FCN3,
                      { { 0x1000 + CMD_MVAR,    0, "" },
                        { 0x1000 + CMD_VARMENU, 0, "" },
                        { 0x1000 + CMD_GETKEY,  0, "" },
                        { 0x1000 + CMD_MENU,    0, "" },
                        { 0x1000 + CMD_KEYG,    0, "" },
                        { 0x1000 + CMD_KEYX,    0, "" } } },
    { /* MENU_PGM_XCOMP0 */ MENU_PGM_FCN2, MENU_NONE, MENU_NONE,
                      { { 0x1000 + CMD_X_EQ_0, 0, "" },
                        { 0x1000 + CMD_X_NE_0, 0, "" },
                        { 0x1000 + CMD_X_LT_0, 0, "" },
                        { 0x1000 + CMD_X_GT_0, 0, "" },
                        { 0x1000 + CMD_X_LE_0, 0, "" },
                        { 0x1000 + CMD_X_GE_0, 0, "" } } },
    { /* MENU_PGM_XCOMPY */ MENU_PGM_FCN2, MENU_NONE, MENU_NONE,
                      { { 0x1000 + CMD_X_EQ_Y, 0, "" },
                        { 0x1000 + CMD_X_NE_Y, 0, "" },
                        { 0x1000 + CMD_X_LT_Y, 0, "" },
                        { 0x1000 + CMD_X_GT_Y, 0, "" },
                        { 0x1000 + CMD_X_LE_Y, 0, "" },
                        { 0x1000 + CMD_X_GE_Y, 0, "" } } },
    { /* MENU_PRINT1 */ MENU_NONE, MENU_PRINT2, MENU_PRINT3,
                      { { 0x1000 + CMD_PRSIGMA, 0, "" },
                        { 0x1000 + CMD_PRP,     0, "" },
                        { 0x1000 + CMD_PRV,     0, "" },
                        { 0x1000 + CMD_PRSTK,   0, "" },
                        { 0x1000 + CMD_PRA,     0, "" },
                        { 0x1000 + CMD_PRX,     0, "" } } },
    { /* MENU_PRINT2 */ MENU_NONE, MENU_PRINT3, MENU_PRINT1,
                      { { 0x1000 + CMD_PRUSR, 0, "" },
                        { 0x1000 + CMD_LIST,  0, "" },
                        { 0x1000 + CMD_ADV,   0, "" },
                        { 0x1000 + CMD_PRLCD, 0, "" },
                        { 0x1000 + CMD_PRREG, 0, "" },
                        { 0x1000 + CMD_DELAY, 0, "" } } },
    { /* MENU_PRINT3 */ MENU_NONE, MENU_PRINT1, MENU_PRINT2,
                      { { 0x2000 + CMD_PON,    0, "" },
                        { 0x2000 + CMD_POFF,   0, "" },
                        { 0x2000 + CMD_MAN,    0, "" },
                        { 0x2000 + CMD_NORM,   0, "" },
                        { 0x2000 + CMD_TRACE,  0, "" },
                        { 0x2000 + CMD_STRACE, 0, "" } } },
    { /* MENU_TOP_FCN */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { 0x1000 + CMD_SIGMAADD, 0, "" },
                        { 0x1000 + CMD_INV,      0, "" },
                        { 0x1000 + CMD_SQRT,     0, "" },
                        { 0x1000 + CMD_LOG,      0, "" },
                        { 0x1000 + CMD_LN,       0, "" },
                        { 0x1000 + CMD_XEQ,      0, "" } } },
    { /* MENU_CATALOG */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" } } },
    { /* MENU_BLANK */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" } } },
    { /* MENU_PROGRAMMABLE */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" } } },
    { /* MENU_VARMENU */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" },
                        { 0, 0, "" } } },
    { /* MENU_STAT1 */ MENU_NONE, MENU_STAT2, MENU_STAT2,
                      { { 0x1000 + CMD_SIGMAADD, 0, ""     },
                        { 0x1000 + CMD_SUM,      0, ""     },
                        { 0x1000 + CMD_MEAN,     0, ""     },
                        { 0x1000 + CMD_WMEAN,    0, ""     },
                        { 0x1000 + CMD_SDEV,     0, ""     },
                        { MENU_STAT_CFIT,        4, "CFIT" } } },
    { /* MENU_STAT2 */ MENU_NONE, MENU_STAT1, MENU_STAT1,
                      { { 0x2000 + CMD_ALLSIGMA,   0, ""   },
                        { 0x2000 + CMD_LINSIGMA,   0, ""   },
                        { 0x1000 + CMD_NULL,       0, ""   },
                        { MENU_STAT_SUMS1,         1, "\5" },
                        { 0x1000 + CMD_SIGMAREG,   0, ""   },
                        { 0x1000 + CMD_SIGMAREG_T, 0, ""   } } },
    { /* MENU_STAT_CFIT */ MENU_STAT1, MENU_NONE, MENU_NONE,
                      { { 0x1000 + CMD_FCSTX, 0, ""     },
                        { 0x1000 + CMD_FCSTY, 0, ""     },
                        { 0x1000 + CMD_SLOPE, 0, ""     },
                        { 0x1000 + CMD_YINT,  0, ""     },
                        { 0x1000 + CMD_CORR,  0, ""     },
                        { MENU_STAT_MODL,     4, "MODL" } } },
    { /* MENU_STAT_MODL */ MENU_STAT_CFIT, MENU_NONE, MENU_NONE,
                      { { 0x2000 + CMD_LINF, 0, "" },
                        { 0x2000 + CMD_LOGF, 0, "" },
                        { 0x2000 + CMD_EXPF, 0, "" },
                        { 0x2000 + CMD_PWRF, 0, "" },
                        { 0x1000 + CMD_NULL, 0, "" },
                        { 0x1000 + CMD_BEST, 0, "" } } },
    { /* MENU_STAT_SUMS1 */ MENU_STAT2, MENU_STAT_SUMS2, MENU_STAT_SUMS3,
                      { { 0x1000 + CMD_SX,  0, "" },
                        { 0x1000 + CMD_SX2, 0, "" },
                        { 0x1000 + CMD_SY,  0, "" },
                        { 0x1000 + CMD_SY2, 0, "" },
                        { 0x1000 + CMD_SXY, 0, "" },
                        { 0x1000 + CMD_SN,  0, "" } } },
    { /* MENU_STAT_SUMS2 */ MENU_STAT2, MENU_STAT_SUMS3, MENU_STAT_SUMS1,
                      { { 0x1000 + CMD_SLNX,    0, "" },
                        { 0x1000 + CMD_SLNX2,   0, "" },
                        { 0x1000 + CMD_SLNY,    0, "" },
                        { 0x1000 + CMD_SLNY2,   0, "" },
                        { 0x1000 + CMD_SLNXLNY, 0, "" },
                        { 0x1000 + CMD_SXLNY,   0, "" } } },
    { /* MENU_STAT_SUMS3 */ MENU_STAT2, MENU_STAT_SUMS1, MENU_STAT_SUMS2,
                      { { 0x1000 + CMD_SYLNX, 0, "" },
                        { 0x1000 + CMD_NULL,  0, "" },
                        { 0x1000 + CMD_NULL,  0, "" },
                        { 0x1000 + CMD_NULL,  0, "" },
                        { 0x1000 + CMD_NULL,  0, "" },
                        { 0x1000 + CMD_NULL,  0, "" } } },
    { /* MENU_MATRIX1 */ MENU_NONE, MENU_MATRIX2, MENU_MATRIX3,
                      { { 0x1000 + CMD_NEWMAT, 0, "" },
                        { 0x1000 + CMD_INVRT,  0, "" },
                        { 0x1000 + CMD_DET,    0, "" },
                        { 0x1000 + CMD_TRANS,  0, "" },
                        { 0x1000 + CMD_SIMQ,   0, "" },
                        { 0x1000 + CMD_EDIT,   0, "" } } },
    { /* MENU_MATRIX2 */ MENU_NONE, MENU_MATRIX3, MENU_MATRIX1,
                      { { 0x1000 + CMD_DOT,   0, "" },
                        { 0x1000 + CMD_CROSS, 0, "" },
                        { 0x1000 + CMD_UVEC,  0, "" },
                        { 0x1000 + CMD_DIM,   0, "" },
                        { 0x1000 + CMD_INDEX, 0, "" },
                        { 0x1000 + CMD_EDITN, 0, "" } } },
    { /* MENU_MATRIX3 */ MENU_NONE, MENU_MATRIX1, MENU_MATRIX2,
                      { { 0x1000 + CMD_STOIJ, 0, "" },
                        { 0x1000 + CMD_RCLIJ, 0, "" },
                        { 0x1000 + CMD_STOEL, 0, "" },
                        { 0x1000 + CMD_RCLEL, 0, "" },
                        { 0x1000 + CMD_PUTM,  0, "" },
                        { 0x1000 + CMD_GETM,  0, "" } } },
    { /* MENU_MATRIX_SIMQ */ MENU_MATRIX1, MENU_NONE, MENU_NONE,
                      { { 0x1000 + CMD_MATA, 0, "" },
                        { 0x1000 + CMD_MATB, 0, "" },
                        { 0x1000 + CMD_MATX, 0, "" },
                        { 0x1000 + CMD_NULL, 0, "" },
                        { 0x1000 + CMD_NULL, 0, "" },
                        { 0x1000 + CMD_NULL, 0, "" } } },
    { /* MENU_MATRIX_EDIT1 */ MENU_NONE, MENU_MATRIX_EDIT2, MENU_MATRIX_EDIT2,
                      { { 0x1000 + CMD_LEFT,    0, "" },
                        { 0x1000 + CMD_OLD,     0, "" },
                        { 0x1000 + CMD_UP,      0, "" },
                        { 0x1000 + CMD_DOWN,    0, "" },
                        { 0x1000 + CMD_GOTOROW, 0, "" },
                        { 0x1000 + CMD_RIGHT,   0, "" } } },
    { /* MENU_MATRIX_EDIT2 */ MENU_NONE, MENU_MATRIX_EDIT1, MENU_MATRIX_EDIT1,
                      { { 0x1000 + CMD_INSR, 0, "" },
                        { 0x1000 + CMD_NULL, 0, "" },
                        { 0x1000 + CMD_DELR, 0, "" },
                        { 0x2000 + CMD_STK,  0, "" },
                        { 0x2000 + CMD_WRAP, 0, "" },
                        { 0x2000 + CMD_GROW, 0, "" } } },
    { /* MENU_BASE */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { 0x1000 + CMD_A_THRU_F, 0, ""      },
                        { 0x2000 + CMD_HEXM,     0, ""      },
                        { 0x2000 + CMD_DECM,     0, ""      },
                        { 0x2000 + CMD_OCTM,     0, ""      },
                        { 0x2000 + CMD_BINM,     0, ""      },
                        { MENU_BASE_LOGIC,       5, "LOGIC" } } },
    { /* MENU_BASE_A_THRU_F */ MENU_BASE, MENU_NONE, MENU_NONE,
                      { { 0, 1, "A" },
                        { 0, 1, "B" },
                        { 0, 1, "C" },
                        { 0, 1, "D" },
                        { 0, 1, "E" },
                        { 0, 1, "F" } } },
    { /* MENU_BASE_LOGIC */ MENU_BASE, MENU_NONE, MENU_NONE,
                      { { 0x1000 + CMD_AND,   0, "" },
                        { 0x1000 + CMD_OR,    0, "" },
                        { 0x1000 + CMD_XOR,   0, "" },
                        { 0x1000 + CMD_NOT,   0, "" },
                        { 0x1000 + CMD_BIT_T, 0, "" },
                        { 0x1000 + CMD_ROTXY, 0, "" } } },
    { /* MENU_SOLVE */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { 1,                   1, "=" },
                        { 0x1000 + CMD_MVAR,   0, ""  },
                        { 0x1000 + CMD_NULL,   0, ""  },
                        { 0x1000 + CMD_EQNSLV, 0, ""  },
                        { 0x1000 + CMD_PGMSLV, 0, ""  },
                        { 0x1000 + CMD_SOLVE,  0, ""  } } },
    { /* MENU_INTEG */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { 1,                   1, "=" },
                        { 0x1000 + CMD_MVAR,   0, ""  },
                        { 0x1000 + CMD_NULL,   0, ""  },
                        { 0x1000 + CMD_EQNINT, 0, ""  },
                        { 0x1000 + CMD_PGMINT, 0, ""  },
                        { 0x1000 + CMD_INTEG,  0, ""  } } },
    { /* MENU_INTEG_PARAMS */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { 0,                 4, "LLIM" },
                        { 0,                 4, "ULIM" },
                        { 0,                 3, "ACC"  },
                        { 0x1000 + CMD_NULL, 0, ""     },
                        { 0x1000 + CMD_NULL, 0, ""     },
                        { 0,                 1, "\3"   } } },
    { /* MENU_DIR_FCN1 */ MENU_NONE, MENU_DIR_FCN2, MENU_DIR_FCN2,
                      { { 0x1000 + CMD_CHDIR,  0, "" },
                        { 0x1000 + CMD_UPDIR,  0, "" },
                        { 0x1000 + CMD_HOME,   0, "" },
                        { 0x1000 + CMD_PATH,   0, "" },
                        { 0x1000 + CMD_CRDIR,  0, "" },
                        { 0x1000 + CMD_RENAME, 0, "" } } },
    { /* MENU_DIR_FCN2 */ MENU_NONE, MENU_DIR_FCN1, MENU_DIR_FCN1,
                      { { 0x1000 + CMD_PGDIR,   0, "" },
                        { 0x1000 + CMD_PRALL,   0, "" },
                        { 0x1000 + CMD_NULL,    0, "" },
                        { 0x1000 + CMD_REFCOPY, 0, "" },
                        { 0x1000 + CMD_REFMOVE, 0, "" },
                        { 0x1000 + CMD_REFFIND, 0, "" } } },
    { /* MENU_UNIT_FCN1 */ MENU_NONE, MENU_UNIT_FCN2, MENU_UNIT_FCN2,
                      { { 0x1000 + CMD_CONVERT,   0, "" },
                        { 0x1000 + CMD_UBASE,     0, "" },
                        { 0x1000 + CMD_UVAL,      0, "" },
                        { 0x1000 + CMD_UFACT,     0, "" },
                        { 0x1000 + CMD_TO_UNIT,   0, "" },
                        { 0x1000 + CMD_FROM_UNIT, 0, "" } } },
    { /* MENU_UNIT_FCN2 */ MENU_NONE, MENU_UNIT_FCN1, MENU_UNIT_FCN1,
                      { { 0x1000 + CMD_UNIT_T, 0, "" },
                        { 0x1000 + CMD_NULL,   0, "" },
                        { 0x1000 + CMD_NULL,   0, "" },
                        { 0x1000 + CMD_NULL,   0, "" },
                        { 0x1000 + CMD_NULL,   0, "" },
                        { 0x1000 + CMD_NULL,   0, "" } } },
    { /* MENU_TVM_APP1 */ MENU_NONE, MENU_TVM_APP2, MENU_TVM_APP2,
                      { { 0x1000 + CMD_N,        0, "" },
                        { 0x1000 + CMD_I_PCT_YR, 0, "" },
                        { 0x1000 + CMD_PV,       0, "" },
                        { 0x1000 + CMD_PMT,      0, "" },
                        { 0x1000 + CMD_FV,       0, "" },
                        { 0x1000 + CMD_TCLEAR,   0, "" } } },
    { /* MENU_TVM_APP2 */ MENU_NONE, MENU_TVM_APP1, MENU_TVM_APP1,
                      { { 0x1000 + CMD_P_PER_YR, 0, "" },
                        { 0x2000 + CMD_TBEGIN,   0, "" },
                        { 0x2000 + CMD_TEND,     0, "" },
                        { 0x1000 + CMD_TRESET,   0, "" },
                        { 0x1000 + CMD_NULL,     0, "" },
                        { 0x1000 + CMD_AMORT,    0, "" } } },
    { /* MENU_TVM_AMORT */ MENU_TVM_APP2, MENU_NONE, MENU_NONE,
                      { { 0x1000 + CMD_TNUM_P, 0, "" },
                        { 0x1000 + CMD_TINT,   0, "" },
                        { 0x1000 + CMD_TPRIN,  0, "" },
                        { 0x1000 + CMD_TBAL,   0, "" },
                        { 0x1000 + CMD_TNEXT,  0, "" },
                        { MENU_TVM_TABLE,      5, "TABLE" } } },
    { /* MENU_TVM_TABLE */ MENU_TVM_AMORT, MENU_NONE, MENU_NONE,
                      { { 0x2000 + CMD_TFIRST, 0, "" },
                        { 0x2000 + CMD_TLAST,  0, "" },
                        { 0x2000 + CMD_TINCR,  0, "" },
                        { 0x2000 + CMD_TGO,    0, "" },
                        { 0x1000 + CMD_NULL,   0, "" },
                        { 0x1000 + CMD_NULL,   0, "" } } },
    { /* MENU_TVM_PARAMS */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { 0x1000 + CMD_N,        0, "" },
                        { 0x1000 + CMD_I_PCT_YR, 0, "" },
                        { 0x1000 + CMD_PV,       0, "" },
                        { 0x1000 + CMD_PMT,      0, "" },
                        { 0x1000 + CMD_FV,       0, "" },
                        { 0x1000 + CMD_P_PER_YR, 0, "" } } },
    { /* MENU_TVM_PRGM1 */ MENU_NONE, MENU_TVM_PRGM2, MENU_TVM_PRGM2,
                      { { 0x1000 + CMD_N,        0, "" },
                        { 0x1000 + CMD_I_PCT_YR, 0, "" },
                        { 0x1000 + CMD_PV,       0, "" },
                        { 0x1000 + CMD_PMT,      0, "" },
                        { 0x1000 + CMD_FV,       0, "" },
                        { 0x1000 + CMD_NULL,     0, "" } } },
    { /* MENU_TVM_PRGM2 */ MENU_NONE, MENU_TVM_PRGM1, MENU_TVM_PRGM1,
                      { { 0x1000 + CMD_SPPV, 0, "" },
                        { 0x1000 + CMD_SPFV, 0, "" },
                        { 0x1000 + CMD_USPV, 0, "" },
                        { 0x1000 + CMD_USFV, 0, "" },
                        { 0x1000 + CMD_NULL, 0, "" },
                        { 0x1000 + CMD_NULL, 0, "" } } },
    { /* MENU_EQN_FCN */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { 0x1000 + CMD_NEWEQN,  0, "" },
                        { 0x1000 + CMD_EDITEQN, 0, "" },
                        { 0x1000 + CMD_PARSE,   0, "" },
                        { 0x1000 + CMD_UNPARSE, 0, "" },
                        { 0x1000 + CMD_EVAL,    0, "" },
                        { 0x1000 + CMD_EVALN,   0, "" } } },
    { /* MENU_GRAPH */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { 0x1000 + CMD_PGMPLOT, 0, "" },
                        { 0x1000 + CMD_EQNPLOT, 0, "" },
                        { 0x1000 + CMD_PARAM,   0, "" },
                        { 0x1000 + CMD_CONST,   0, "" },
                        { 0x1000 + CMD_VIEW_P,  0, "" },
                        { 0x1000 + CMD_PLOT,    0, "" } } },
    { /* MENU_GRAPH_AXES */ MENU_GRAPH, MENU_NONE, MENU_NONE,
                      { { 0x1000 + CMD_XAXIS,  0, "" },
                        { 0x1000 + CMD_YAXIS,  0, "" },
                        { 0x1000 + CMD_NULL,   0, "" },
                        { 0x1000 + CMD_NULL,   0, "" },
                        { 0x1000 + CMD_NULL,   0, "" },
                        { 0x1000 + CMD_NULL,   0, "" } } },
    { /* MENU_GRAPH_VIEW */ MENU_GRAPH, MENU_NONE, MENU_NONE,
                      { { 0x1000 + CMD_XMIN, 0, "" },
                        { 0x1000 + CMD_XMAX, 0, "" },
                        { 0x1000 + CMD_YMIN, 0, "" },
                        { 0x1000 + CMD_YMAX, 0, "" },
                        { 0x1000 + CMD_SCAN, 0, "" },
                        { 0x1000 + CMD_PLOT, 0, "" } } },
};


/* By how much do the variables, programs, and labels
 * arrays grow when they are full
 */
#define VARS_INCREMENT 25
#define PRGMS_INCREMENT 10
#define LABELS_INCREMENT 10

/* Registers */
vartype **stack = NULL;
int sp = -1;
int stack_capacity = 0;
vartype *lastx = NULL;
int reg_alpha_length = 0;
char reg_alpha[44];

/* Flags */
flags_struct flags;
const char *virtual_flags =
    /* 00-49 */ "00000000000000000000000000010000000000000000111111"
    /* 50-99 */ "11010000000000010000000001000000000000000000000000";

/* Local Variables (LSTO) */
int local_vars_capacity = 0;
int local_vars_count = 0;
var_struct *local_vars = NULL;

static bool dir_used(int id);

/* Hierarchical storage */
directory::directory(int id) {
    this->id = id;
    vars_capacity = 0;
    vars_count = 0;
    vars = NULL;
    prgms_capacity = 0;
    prgms_count = 0;
    prgms = NULL;
    labels_capacity = 0;
    labels_count = 0;
    labels = NULL;
    children_capacity = 0;
    children_count = 0;
    children = NULL;
    parent = NULL;
}

directory::~directory() {
    if (cwd == this)
        cwd = root;
    if (dir_used(id)) {
        set_running(false);
        clear_all_rtns();
        current_prgm.set(root->id, 0);
        pc = -1;
    }
    if (matedit_mode == 3 && matedit_dir == id)
        leave_matrix_editor();

    if (this != eq_dir) {
        // The equation directory contains no variables and no children.
        // It does contain programs, but those are generated code, which
        // gets cleaned up when the owning equation objects are deleted.
        // Thus, none of the following cleanup logic should be executed
        // for this particular directory.
        for (int i = 0; i < vars_count; i++)
            free_vartype(vars[i].value);
        free(vars);
        for (int i = 0; i < prgms_count; i++) {
            count_embed_references(this, i, false);
            delete prgms[i].eq_data;
            free(prgms[i].text);
        }
        free(prgms);
        free(labels);
        for (int i = 0; i < children_count; i++)
            delete children[i].dir;
        free(children);
    }
    unmap_dir(id);
}

directory *directory::clone() {
    int id = get_dir_id();
    directory *res = new (std::nothrow) directory(id);
    if (res == NULL)
        return NULL;
    map_dir(id, res);
    if (false) {
        error:
        delete res;
        return NULL;
    }
    res->vars = (var_struct *) malloc(vars_count * sizeof(var_struct));
    if (res->vars == NULL && vars_count != 0)
        goto error;
    res->prgms = (prgm_struct *) malloc(prgms_count * sizeof(prgm_struct));
    if (res->prgms == NULL && prgms_count != 0)
        goto error;
    res->labels = (label_struct *) malloc(labels_count * sizeof(label_struct));
    if (res->labels == NULL && labels_count != 0)
        goto error;
    res->children = (subdir_struct *) malloc(children_count * sizeof(subdir_struct));
    if (res->children == NULL && children_count != 0)
        goto error;
    res->vars_capacity = vars_count;
    res->prgms_capacity = prgms_capacity;
    res->labels_capacity = labels_capacity;
    res->children_capacity = children_capacity;
    for (int i = 0; i < vars_count; i++) {
        res->vars[i] = vars[i];
        res->vars[i].value = dup_vartype(vars[i].value);
        if (res->vars[i].value == NULL)
            goto error;
        res->vars_count++;
    }
    for (int i = 0; i < prgms_count; i++) {
        res->prgms[i] = prgms[i];
        int newsize = prgms[i].size;
        unsigned char *newtext = (unsigned char *) malloc(newsize);
        if (newtext == NULL && newsize != 0)
            goto error;
        memcpy(newtext, prgms[i].text, newsize);
        res->prgms[i].capacity = newsize;
        res->prgms[i].text = newtext;
        res->prgms_count++;
    }
    for (int i = 0; i < labels_count; i++)
        res->labels[i] = labels[i];
    res->labels_count = labels_count;
    for (int i = 0; i < children_count; i++) {
        res->children[i] = children[i];
        res->children[i].dir = children[i].dir->clone();
        if (res->children[i].dir == NULL)
            goto error;
        res->children[i].dir->parent = res;
        res->children_count++;
    }
    return res;
}

directory *root = NULL;
directory *cwd = NULL;
directory *eq_dir = NULL;
directory **dir_list = NULL;
int dir_list_capacity = 0;

int get_dir_id() {
    // Numbers <= 0 are reserved for locals, with -n corresponding to subtroutine level n;
    // 1 is reserved for the current directory; and everything >= 2 is available for
    // actual directories. Note that the root directory id is not special; it can always
    // be found since it's simply root->id.
    for (int i = 2; i < dir_list_capacity; i++)
        if (dir_list[i] == NULL)
            return i;
    return dir_list_capacity;
}

void map_dir(int id, directory *dir) {
    if (id >= dir_list_capacity) {
        int newcap = id + 11;
        directory **new_dir_list = (directory **) realloc(dir_list, newcap * sizeof(directory *));
        // TODO: Handle memory allocation failure
        for (int i = dir_list_capacity; i < newcap; i++)
            new_dir_list[i] = NULL;
        dir_list = new_dir_list;
        dir_list_capacity = newcap;
    }
    dir_list[id] = dir;
}

void unmap_dir(int id) {
    dir_list[id] = NULL;
}

directory *get_dir(int id) {
    if (id < dir_list_capacity)
        return dir_list[id];
    else
        return NULL;
}

void dir_list_clear() {
    free(dir_list);
    dir_list = NULL;
    dir_list_capacity = 0;
}


/* Programs */

pgm_index current_prgm;
int4 pc;
int prgm_highlight_row = 0;

vartype *varmenu_eqn;
int varmenu_length;
char varmenu[7];
int varmenu_rows;
int varmenu_row;
int varmenu_labellength[6];
char varmenu_labeltext[6][7];
int varmenu_role;

bool mode_clall;
int mode_message_lines;
int (*mode_interruptible)(bool) = NULL;
bool mode_stoppable;
bool mode_command_entry;
char mode_number_entry;
bool mode_alpha_entry;
bool mode_shift;
int mode_appmenu;
int mode_auxmenu;
int mode_plainmenu;
bool mode_plainmenu_sticky;
int mode_transientmenu;
int mode_alphamenu;
int mode_commandmenu;
bool mode_running;
bool mode_getkey;
bool mode_getkey1;
bool mode_pause = false;
bool mode_disable_stack_lift; /* transient */
bool mode_caller_stack_lift_disabled;
bool mode_varmenu;
int mode_varmenu_whence;
bool mode_updown;
int4 mode_sigma_reg;
int mode_goose;
bool mode_time_clktd;
bool mode_time_clk24;
int mode_wsize;
#if defined(ANDROID) || defined(IPHONE)
bool mode_popup_unknown = true;
#endif
bool mode_header;
int mode_amort_seq;
bool mode_plot_viewer;
int mode_plot_key;
int mode_plot_sp;
vartype *mode_plot_inv;
int mode_plot_result_width;
bool mode_multi_line;
bool mode_lastx_top;
bool mode_alpha_top;
bool mode_header_flags;
bool mode_header_polar;
bool mode_matedit_stk;

phloat entered_number;
int entered_string_length;
char entered_string[15];

int pending_command;
arg_struct pending_command_arg;
int xeq_invisible;

/* Multi-keystroke commands -- edit state */
/* Relevant when mode_command_entry != 0 */
int incomplete_command;
bool incomplete_ind;
bool incomplete_alpha;
int incomplete_length;
int incomplete_maxdigits;
int incomplete_argtype;
int incomplete_num;
char incomplete_str[50];
int4 incomplete_saved_pc;
int4 incomplete_saved_highlight_row;

/* Command line handling temporaries */
char cmdline[100];
int cmdline_length;
int cmdline_unit;

/* Matrix editor / matrix indexing */
int matedit_mode; /* 0=off, 1=index, 2=edit, 3=editn */
int4 matedit_dir; /* dir <= 0 is local at level -dir */
char matedit_name[7];
int matedit_length;
vartype *matedit_x;
int4 matedit_i;
int4 matedit_j;
int matedit_prev_appmenu;
matedit_stack_entry *matedit_stack = NULL;
int matedit_stack_depth = 0;
bool matedit_is_list;
int4 matedit_view_i;
int4 matedit_view_j;

/* INPUT */
char input_name[11];
int input_length;
arg_struct input_arg;

/* ERRMSG/ERRNO */
int lasterr = 0;
int lasterr_length;
char lasterr_text[22];

/* BASE application */
int baseapp = 0;

/* Random number generator */
int8 random_number_low, random_number_high;

/* NORM & TRACE mode: number waiting to be printed */
int deferred_print = 0;

/* Keystroke buffer - holds keystrokes received while
 * there is a program running.
 */
int keybuf_head = 0;
int keybuf_tail = 0;
int keybuf[16];

int remove_program_catalog = 0;

int state_file_number_format;

/* No user interaction: we keep track of whether or not the user
 * has pressed any keys since powering up, and we don't allow
 * programmatic OFF until they have. The reason is that we want
 * to prevent nastiness like
 *
 *   LBL "YIKES"  SF 11  OFF  GTO "YIKES"
 *
 * from locking the user out.
 */
bool no_keystrokes_yet;


/* Version number for the state file.
 * State file versions correspond to application releases as follows:
 *
 * Version  0: 1.0    First alpha release
 * Version  1: 1.0    Cursor left, cursor right, del key handling
 * Version  2: 1.0    Return to user code after interactive EVAL
 * Version  3: 1.0    Save T during SOLVE and INTEG
 * Version  4: 1.0    Catalog improvements in equation editor
 * Version  5: 1.0    MODES & DISP in equation editor
 * Version  6: 1.0    Attached units
 * Version  7: 1.0    New format, because of too many problems with the old one
 * Version  8: 1.0    SOLVE and INTEG unit support
 * Version  9: 1.0    Directories
 * Version 10: 1.0    Directory and program references
 * Version 11: 1.0    Non-local items in catalogs
 * Version 12: 1.0    More robust EDITN and INDEX target tracking
 * Version 13: 1.0    Big display
 * Version 14: 1.0    Catalog no TOP
 * Version 15: 1.0    Skin flags
 * Version 16: 1.0    MENULEVEL_AUX
 * Version 17: 1.0    Persistent requested display size
 * Version 18: 1.0    AMORT sequence
 * Version 19: 1.0    VARMENU whence
 * Version 20: 1.0    Plot mode
 * Version 21: 1.0    Plot number width; direct solver flag
 * Version 22: 1.0    UNITS skip-top in equation editor
 * Version 23: 1.0    Interactive XSTR max length raised to 50
 * Version 24: 1.0.3  SOLVE secant impatience
 * Version 25: 1.0.20 Saved equation error position
 * Version 26: 1.0.20 Second row for UNIT.FCN menu
 * Version 27: 1.0.20 1LINE/NLINE
 * Version 28: 1.1    CSLD?
 * Version 29: 1.1    LTOP
 * Version 30: 1.1    ATOP
 * Version 31: 1.1    Flags & Polar header indicators
 * Version 32: 1.1    FSTACK replacing FDEPTH/FLASTX; requires eqn reparse
 * Version 33: 1.1    Matrix editor nested lists
 * Version 34: 1.1    No more redundant FUNC 01 and LNSTK in generated code
 * Version 35: 1.1    FSTART; requires eqn reparse
 * Version 36: 1.1    Matrix editor full screen: save offsets
 * Version 37: 1.1    Matrix editor full screen: globals for offsets
 * Version 38: 1.1    Matrix editor full screen: STK toggle
 * Version 39: 1.1.5  Code mapping fix for N+U; requires eqn reparse
 * Version 40: 1.1.11 Use local var for Integ() param; requires eqn reparse
 * Version 41: 1.1.15 Global visibility fix for VIEW(); requires eqn reparse
 * Version 42: 1.1.17a Remember cat position for UNITS key in ASSIGN
 * Version 43: 1.1.18 Program locking
 * Version 44: 1.2.3  Switching character codes 30 and 94
 * Version 45: 1.2.4  Added guillemets to ALPHA menu
 * Version 46: 1.2.4  No more lazy equation saving
 * Version 47: 1.2.5  Fixed EQN mode exit behavior
 * Version 48: 1.2.6  EQN no-arg eval error reporting
 * Version 49: 1.2.6  EQN PRGM mode handling
 */
#define PLUS42_VERSION 49


/*******************/
/* Private globals */
/*******************/

struct rtn_stack_entry {
    int4 dir;
    int4 prgm;
    int4 pc;
    int4 get_prgm() {
        int4 p = prgm & 0x1fffffff;
        if ((p & 0x10000000) != 0)
            p |= 0xe0000000;
        return p;
    }
    void set_prgm(int4 prgm) {
        this->prgm = prgm & 0x1fffffff;
    }
    bool has_matrix() {
        return (prgm & 0x80000000) != 0;
    }
    void set_has_matrix(bool state) {
        if (state)
            prgm |= 0x80000000;
        else
            prgm &= 0x7fffffff;
    }
    bool has_func() {
        return (prgm & 0x40000000) != 0;
    }
    void set_has_func(bool state) {
        if (state)
            prgm |= 0x40000000;
        else
            prgm &= 0xbfffffff;
    }
    bool is_csld() {
        return (prgm & 0x20000000) != 0;
    }
    void set_csld() {
        if (flags.f.stack_lift_disable)
            prgm |= 0x20000000;
        else
            prgm &= 0xdfffffff;
    }
    bool is_special() {
        return (prgm & 0x10000000) != 0;
    }
};

#define MAX_RTN_LEVEL 1024
static int rtn_stack_capacity = 0;
static rtn_stack_entry *rtn_stack = NULL;
static int rtn_level = 0;
static bool rtn_level_0_has_matrix_entry;
static bool rtn_level_0_has_func_state;
static int4 rtn_after_last_rtn_dir = -1;
static int4 rtn_after_last_rtn_prgm = -1;
static int4 rtn_after_last_rtn_pc = -1;
static int rtn_stop_level = -1;
static bool rtn_solve_active = false;
static bool rtn_integ_active = false;
static bool rtn_plot_active = false;

#ifdef IPHONE
/* For iPhone, we disable OFF by default, to satisfy App Store
 * policy, but we allow users to enable it using a magic value
 * in the X register. This flag determines OFF behavior.
 */
bool off_enable_flag = false;
#endif

struct matrix_persister {
    int type;
    int4 rows;
    int4 columns;
};

static int shared_data_count;
static int shared_data_capacity;
static void **shared_data;


static bool shared_data_grow();
static int shared_data_search(void *data);
static void update_label_table(pgm_index prgm, int4 pc, int inserted);
static void invalidate_lclbls(pgm_index idx, bool force);
static int pc_line_convert(int4 loc, int loc_is_pc);

#ifdef BCD_MATH
#define bin_dec_mode_switch() ( state_file_number_format == NUMBER_FORMAT_BINARY )
#else
#define bin_dec_mode_switch() ( state_file_number_format != NUMBER_FORMAT_BINARY )
#endif


void vartype_string::trim1() {
    if (length > SSLENV + 1) {
        memmove(t.ptr, t.ptr + 1, --length);
    } else if (length == SSLENV + 1) {
        char temp[SSLENV];
        memcpy(temp, t.ptr + 1, --length);
        free(t.ptr);
        memcpy(t.buf, temp, length);
    } else if (length > 0) {
        memmove(t.buf, t.buf + 1, --length);
    }
}

static bool shared_data_grow() {
    if (shared_data_count < shared_data_capacity)
        return true;
    shared_data_capacity += 10;
    void **p = (void **) realloc(shared_data,
                                 shared_data_capacity * sizeof(void *));
    if (p == NULL)
        return false;
    shared_data = p;
    return true;
}

static int shared_data_search(void *data) {
    for (int i = 0; i < shared_data_count; i++)
        if (shared_data[i] == data)
            return i;
    return -1;
}

bool persist_vartype(vartype *v) {
    if (v == NULL)
        return write_char(TYPE_NULL);
    if (!write_char(v->type))
        return false;
    switch (v->type) {
        case TYPE_REAL: {
            vartype_real *r = (vartype_real *) v;
            return write_phloat(r->x);
        }
        case TYPE_COMPLEX: {
            vartype_complex *c = (vartype_complex *) v;
            return write_phloat(c->re) && write_phloat(c->im);
        }
        case TYPE_STRING: {
            vartype_string *s = (vartype_string *) v;
            return write_int4(s->length)
                && fwrite(s->txt(), 1, s->length, gfile) == s->length;
        }
        case TYPE_REALMATRIX: {
            vartype_realmatrix *rm = (vartype_realmatrix *) v;
            int4 rows = rm->rows;
            int4 columns = rm->columns;
            bool must_write = true;
            if (rm->array->refcount > 1) {
                int n = shared_data_search(rm->array);
                if (n == -1) {
                    // A negative row count signals a new shared matrix
                    rows = -rows;
                    if (!shared_data_grow())
                        return false;
                    shared_data[shared_data_count++] = rm->array;
                } else {
                    // A zero row count means this matrix shares its data
                    // with a previously written matrix
                    rows = 0;
                    columns = n;
                    must_write = false;
                }
            }
            write_int4(rows);
            write_int4(columns);
            if (must_write) {
                int size = rm->rows * rm->columns;
                if (fwrite(rm->array->is_string, 1, size, gfile) != size)
                    return false;
                for (int i = 0; i < size; i++) {
                    if (rm->array->is_string[i] == 0) {
                        if (!write_phloat(rm->array->data[i]))
                            return false;
                    } else {
                        char *text;
                        int4 len;
                        get_matrix_string(rm, i, &text, &len);
                        if (!write_int4(len))
                            return false;
                        if (fwrite(text, 1, len, gfile) != len)
                            return false;
                    }
                }
            }
            return true;
        }
        case TYPE_COMPLEXMATRIX: {
            vartype_complexmatrix *cm = (vartype_complexmatrix *) v;
            int4 rows = cm->rows;
            int4 columns = cm->columns;
            bool must_write = true;
            if (cm->array->refcount > 1) {
                int n = shared_data_search(cm->array);
                if (n == -1) {
                    // A negative row count signals a new shared matrix
                    rows = -rows;
                    if (!shared_data_grow())
                        return false;
                    shared_data[shared_data_count++] = cm->array;
                } else {
                    // A zero row count means this matrix shares its data
                    // with a previously written matrix
                    rows = 0;
                    columns = n;
                    must_write = false;
                }
            }
            write_int4(rows);
            write_int4(columns);
            if (must_write) {
                int size = 2 * cm->rows * cm->columns;
                for (int i = 0; i < size; i++)
                    if (!write_phloat(cm->array->data[i]))
                        return false;
            }
            return true;
        }
        case TYPE_LIST: {
            vartype_list *list = (vartype_list *) v;
            int4 size = list->size;
            int data_index = -1;
            bool must_write = true;
            if (list->array->refcount > 1) {
                int n = shared_data_search(list->array);
                if (n == -1) {
                    // data_index == -2 indicates a new shared list
                    data_index = -2;
                    if (!shared_data_grow())
                        return false;
                    shared_data[shared_data_count++] = list->array;
                } else {
                    // data_index >= 0 refers to a previously shared list
                    data_index = n;
                    must_write = false;
                }
            }
            write_int4(size);
            write_int(data_index);
            if (must_write) {
                for (int4 i = 0; i < list->size; i++)
                    if (!persist_vartype(list->array->data[i]))
                        return false;
            }
            return true;
        }
        case TYPE_EQUATION: {
            vartype_equation *eq = (vartype_equation *) v;
            return write_int4(eq->data->eqn_index);
        }
        case TYPE_UNIT: {
            vartype_unit *u = (vartype_unit *) v;
            if (!write_phloat(u->x))
                return false;
            return write_int4(u->length)
                && fwrite(u->text, 1, u->length, gfile) == u->length;
        }
        case TYPE_DIR_REF: {
            vartype_dir_ref *r = (vartype_dir_ref *) v;
            return write_int4(r->dir);
        }
        case TYPE_PGM_REF: {
            vartype_pgm_ref *r = (vartype_pgm_ref *) v;
            return write_int4(r->dir) && write_int4(r->pgm);
        }
        case TYPE_VAR_REF: {
            vartype_var_ref *r = (vartype_var_ref *) v;
            if (!write_int4(r->dir))
                return false;
            if (!write_char(r->length))
                return false;
            return fwrite(r->name, 1, r->length, gfile) == r->length;
        }
        default:
            /* Should not happen */
            return false;
    }
}


// Using global for 'ver' so we don't have to pass it around all the time

int4 ver;

static equation_data *unpersist_equation_data() {
    int4 eqn_index;
    directory *saved_cwd = cwd;
    pgm_index saved_prgm = current_prgm;
    int4 saved_pc = pc;
    cwd = eq_dir;
    current_prgm.set(1, 0);
    core_import_programs(1, NULL);
    cwd = saved_cwd;
    current_prgm = saved_prgm;
    pc = saved_pc;
    if (!read_int4(&eqn_index))
        return NULL;
    /* The equation code was loaded as the last program in the eq_dir
     * directory, which means its effective equation index is now
     * prgms_count - 1. If that happens to match the saved equation id,
     * great; if not, we'll have to rearrange things to make the program
     * index agree with the equation id.
     */
    if (eqn_index < eq_dir->prgms_count - 1) {
        prgm_struct *lprgm = eq_dir->prgms + --(eq_dir->prgms_count);
        eq_dir->prgms[eqn_index] = *lprgm;
        lprgm->text = NULL;
        lprgm->eq_data = NULL;
    } else if (eqn_index > eq_dir->prgms_count - 1) {
        if (eqn_index + 1 > eq_dir->prgms_capacity) {
            int oc = eq_dir->prgms_capacity;
            int nc = eqn_index + 11;
            prgm_struct *newprgms = (prgm_struct *) realloc(eq_dir->prgms, nc * sizeof(prgm_struct));
            if (newprgms == NULL)
                return NULL;
            eq_dir->prgms = newprgms;
            eq_dir->prgms_capacity = nc;
            for (int i = oc; i < eq_dir->prgms_capacity; i++) {
                eq_dir->prgms[i].text = NULL;
                eq_dir->prgms[i].eq_data = NULL;
            }
        }
        prgm_struct *lprgm = eq_dir->prgms + (eq_dir->prgms_count - 1);
        eq_dir->prgms_count = eqn_index + 1;
        eq_dir->prgms[eqn_index] = *lprgm;
        lprgm->text = NULL;
        lprgm->eq_data = NULL;
    }
    equation_data *eqd = new (std::nothrow) equation_data;
    if (eqd == NULL)
        return NULL;
    eqd->refcount = 0;
    eqd->eqn_index = eqn_index;
    if (!read_int4(&eqd->length)) {
        eq_fail:
        delete eqd;
        return NULL;
    }
    if (eqd->length > 0) {
        eqd->text = (char *) malloc(eqd->length);
        if (eqd->text == NULL)
            goto eq_fail;
        if (fread(eqd->text, 1, eqd->length, gfile) != eqd->length)
            goto eq_fail;
        if (ver < 44)
            switch_30_and_94(eqd->text, eqd->length);
    }
    int cmsize;
    if (!read_int(&cmsize))
        goto eq_fail;
    if (cmsize > 0) {
        char *cmdata = (char *) malloc(cmsize);
        if (cmdata == NULL)
            goto eq_fail;
        if (fread(cmdata, 1, cmsize, gfile) != cmsize) {
            cm_fail:
            free(cmdata);
            goto eq_fail;
        }
        eqd->map = new (std::nothrow) CodeMap(cmdata, cmsize);
        if (eqd->map == NULL)
            goto cm_fail;
    }
    if (!read_bool(&eqd->compatMode))
        goto eq_fail;
    eq_dir->prgms[eqn_index].eq_data = eqd;
    if (eqd->length > 0) {
        int errpos;
        eqd->ev = Parser::parse(std::string(eqd->text, eqd->length), &eqd->compatMode, &eqd->compatModeEmbedded, &errpos);
    }
    return eqd;
}

bool unpersist_vartype(vartype **v) {
    char type;
    if (!read_char(&type))
        return false;
    switch (type) {
        case TYPE_NULL: {
            *v = NULL;
            return true;
        }
        case TYPE_REAL: {
            vartype_real *r = (vartype_real *) new_real(0);
            if (r == NULL)
                return false;
            if (!read_phloat(&r->x)) {
                free_vartype((vartype *) r);
                return false;
            }
            *v = (vartype *) r;
            return true;
        }
        case TYPE_COMPLEX: {
            vartype_complex *c = (vartype_complex *) new_complex(0, 0);
            if (c == NULL)
                return false;
            if (!read_phloat(&c->re) || !read_phloat(&c->im)) {
                free_vartype((vartype *) c);
                return false;
            }
            *v = (vartype *) c;
            return true;
        }
        case TYPE_STRING: {
            int4 len;
            if (!read_int4(&len))
                return false;
            vartype_string *s = (vartype_string *) new_string(NULL, len);
            if (s == NULL)
                return false;
            if (fread(s->txt(), 1, len, gfile) != len) {
                free_vartype((vartype *) s);
                return false;
            }
            if (ver < 44)
                switch_30_and_94(s->txt(), len);
            *v = (vartype *) s;
            return true;
        }
        case TYPE_REALMATRIX: {
            int4 rows, columns;
            if (!read_int4(&rows) || !read_int4(&columns))
                return false;
            if (rows == 0) {
                // Shared matrix
                vartype *m = dup_vartype((vartype *) shared_data[columns]);
                if (m == NULL)
                    return false;
                else {
                    *v = m;
                    return true;
                }
            }
            bool shared = rows < 0;
            if (shared)
                rows = -rows;
            vartype_realmatrix *rm = (vartype_realmatrix *) new_realmatrix(rows, columns);
            if (rm == NULL)
                return false;
            int4 size = rows * columns;
            if (fread(rm->array->is_string, 1, size, gfile) != size) {
                free_vartype((vartype *) rm);
                return false;
            }
            bool success = true;
            int4 i;
            for (i = 0; i < size; i++) {
                success = false;
                if (rm->array->is_string[i] == 0) {
                    if (!read_phloat(&rm->array->data[i]))
                        break;
                } else {
                    rm->array->is_string[i] = 1;
                    // 4-byte length followed by n bytes of text
                    int4 len;
                    if (!read_int4(&len))
                        break;
                    if (len > SSLENM) {
                        int4 *p = (int4 *) malloc(len + 4);
                        if (p == NULL)
                            break;
                        if (fread(p + 1, 1, len, gfile) != len) {
                            free(p);
                            break;
                        }
                        if (ver < 44)
                            switch_30_and_94((char *) (p + 1), len);
                        *p = len;
                        *(int4 **) &rm->array->data[i] = p;
                        rm->array->is_string[i] = 2;
                    } else {
                        char *t = (char *) &rm->array->data[i];
                        *t = len;
                        if (fread(t + 1, 1, len, gfile) != len)
                            break;
                        if (ver < 44)
                            switch_30_and_94(t + 1, len);
                    }
                }
                success = true;
            }
            if (!success) {
                memset(rm->array->is_string + i, 0, size - i);
                free_vartype((vartype *) rm);
                return false;
            }
            if (shared) {
                if (!shared_data_grow()) {
                    free_vartype((vartype *) rm);
                    return false;
                }
                shared_data[shared_data_count++] = rm;
            }
            *v = (vartype *) rm;
            return true;
        }
        case TYPE_COMPLEXMATRIX: {
            int4 rows, columns;
            if (!read_int4(&rows) || !read_int4(&columns))
                return false;
            if (rows == 0) {
                // Shared matrix
                vartype *m = dup_vartype((vartype *) shared_data[columns]);
                if (m == NULL)
                    return false;
                else {
                    *v = m;
                    return true;
                }
            }
            bool shared = rows < 0;
            if (shared)
                rows = -rows;
            vartype_complexmatrix *cm = (vartype_complexmatrix *) new_complexmatrix(rows, columns);
            if (cm == NULL)
                return false;
            int4 size = 2 * rows * columns;
            for (int4 i = 0; i < size; i++) {
                if (!read_phloat(&cm->array->data[i])) {
                    free_vartype((vartype *) cm);
                    return false;
                }
            }
            if (shared) {
                if (!shared_data_grow()) {
                    free_vartype((vartype *) cm);
                    return false;
                }
                shared_data[shared_data_count++] = cm;
            }
            *v = (vartype *) cm;
            return true;
        }
        case TYPE_LIST: {
            int4 size;
            int data_index;
            if (!read_int4(&size) || !read_int(&data_index))
                return false;
            if (data_index >= 0) {
                // Shared list
                vartype *m = dup_vartype((vartype *) shared_data[data_index]);
                if (m == NULL)
                    return false;
                else {
                    *v = m;
                    return true;
                }
            }
            bool shared = data_index == -2;
            vartype_list *list = (vartype_list *) new_list(size);
            if (list == NULL)
                return false;
            if (shared) {
                if (!shared_data_grow()) {
                    free_vartype((vartype *) list);
                    return false;
                }
                shared_data[shared_data_count++] = list;
            }
            for (int4 i = 0; i < size; i++) {
                if (!unpersist_vartype(&list->array->data[i])) {
                    free_vartype((vartype *) list);
                    return false;
                }
            }
            *v = (vartype *) list;
            return true;
        }
        case TYPE_EQUATION: {
            if (ver >= 46) {
                int4 id;
                if (!read_int4(&id))
                    return false;
                equation_data *eqd;
                if (id >= eq_dir->prgms_count || (eqd = eq_dir->prgms[id].eq_data) == NULL)
                    *v = new_string("<Missing Equation>", 18);
                else if (eqd->length > 0 && eqd->ev == NULL)
                    *v = new_string(eqd->text, eqd->length);
                else
                    *v = new_equation(eqd);
                return *v != NULL;
            }

            int data_index;
            if (!read_int(&data_index))
                return false;
            if (data_index >= 0) {
                // Shared equation
                vartype *m = dup_vartype((vartype *) shared_data[data_index]);
                if (m == NULL)
                    return false;
                else {
                    *v = m;
                    return true;
                }
            }
            equation_data *eqd = unpersist_equation_data();
            if (eqd == NULL)
                return false;
            if (eqd->length > 0 && eqd->ev == NULL) {
                // Parse error while everything else looked OK; this is
                // probably an equation that was valid at some point but
                // no longer is, because of a parser change. In a perfect
                // world, that would never happen, but sometimes, bug fixes
                // break equations that relied on those bugs.
                // When this happens, we return a string object instead,
                // and hope for the best. It's better than the dreaded
                // State File Corrupt, anyway.
                *v = new_string(eqd->text, eqd->length);
            } else
                *v = new_equation(eqd);
            if (*v == NULL)
                return false;

            bool shared = data_index == -2;
            if (shared) {
                if (!shared_data_grow()) {
                    free_vartype(*v);
                    return false;
                }
                shared_data[shared_data_count++] = *v;
            }
            return true;
        }
        case TYPE_UNIT: {
            vartype_unit *u = (vartype_unit *) malloc(sizeof(vartype_unit));
            if (u == NULL)
                return false;
            if (!read_phloat(&u->x)) {
                unit_fail:
                free_vartype((vartype *) u);
                return false;
            }
            int4 len;
            if (!read_int4(&len))
                goto unit_fail;
            u->text = (char *) malloc(len);
            if (u->text == NULL && len != 0)
                goto unit_fail;
            if (fread(u->text, 1, len, gfile) != len) {
                free(u->text);
                goto unit_fail;
            }
            if (ver < 44)
                switch_30_and_94(u->text, len);
            u->type = TYPE_UNIT;
            u->length = len;
            *v = (vartype *) u;
            return true;
        }
        case TYPE_DIR_REF: {
            int4 dir;
            if (!read_int4(&dir))
                return false;
            *v = new_dir_ref(dir);
            return *v != NULL;
        }
        case TYPE_PGM_REF: {
            int4 dir, pgm;
            if (!read_int4(&dir))
                return false;
            if (!read_int4(&pgm))
                return false;
            *v = new_pgm_ref(dir, pgm);
            return *v != NULL;
        }
        case TYPE_VAR_REF: {
            int4 dir;
            if (!read_int4(&dir))
                return false;
            char length;
            if (!read_char(&length))
                return false;
            char name[7];
            if (fread(name, 1, length, gfile) != length)
                return false;
            if (ver < 44)
                switch_30_and_94(name, length);
            *v = new_var_ref(dir, name, length);
            return *v != NULL;
        }
        default:
            return false;
    }
}

static bool persist_directory(directory *dir) {
    bool success = false;
    directory *oldcwd = cwd;
    cwd = dir;
    if (!write_int(dir->id))
        goto fail;
    if (!write_int(dir->vars_count))
        goto fail;
    for (int i = 0; i < dir->vars_count; i++) {
        if (!write_char(dir->vars[i].length))
            goto fail;
        if (fwrite(dir->vars[i].name, 1, dir->vars[i].length, gfile) != dir->vars[i].length)
            goto fail;
        if (!persist_vartype(dir->vars[i].value))
            goto fail;
    }
    if (!write_int(dir->prgms_count))
        goto fail;
    for (int i = 0; i < dir->prgms_count; i++)
        core_export_programs(1, &i, NULL);
    for (int i = 0; i < dir->prgms_count; i++)
        if (!write_bool(dir->prgms[i].locked))
            goto fail;
    if (!write_int(dir->children_count))
        goto fail;
    for (int i = 0; i < dir->children_count; i++) {
        if (!write_char(dir->children[i].length))
            goto fail;
        if (fwrite(dir->children[i].name, 1, dir->children[i].length, gfile) != dir->children[i].length)
            goto fail;
        if (!persist_directory(dir->children[i].dir))
            goto fail;
    }
    success = true;
    fail:
    cwd = oldcwd;
    return success;
}

static bool unpersist_directory(directory **d) {
    directory *dir = new (std::nothrow) directory(0);
    directory *oldcwd = cwd;
    cwd = dir;
    if (dir == NULL) {
        fail:
        delete dir;
        *d = NULL;
        cwd = oldcwd;
        return false;
    }
    if (ver >= 9) {
        if (!read_int(&dir->id))
            goto fail;
    } else {
        dir->id = 2;
    }
    map_dir(dir->id, dir);
    int vc;
    if (!read_int(&vc))
        goto fail;
    dir->vars = (var_struct *) malloc(vc * sizeof(var_struct));
    if (dir->vars == NULL && vc != 0)
        goto fail;
    dir->vars_capacity = vc;
    dir->vars_count = 0;
    for (int i = 0; i < vc; i++) {
        var_struct vs;
        if (!read_char((char *) &vs.length))
            goto fail;
        if (fread(vs.name, 1, vs.length, gfile) != vs.length)
            goto fail;
        if (ver < 44)
            switch_30_and_94(vs.name, vs.length);
        if (ver < 9) {
            if (!read_int2(&vs.level))
                goto fail;
            if (!read_int2(&vs.flags))
                goto fail;
            if (ver < 9)
                /* The 'hidden' and 'hiding' flags were made obsolete
                 * by the introduction of directories. A local that hides
                 * a global when it is created may no longer be hiding
                 * anything if the current directory is subsequently
                 * changed. So this neat trick is no longer usable.
                 */
                vs.flags &= ~(VAR_HIDDEN | VAR_HIDING);
        } else
            vs.flags = 0;
        if (!unpersist_vartype(&vs.value))
            goto fail;
        dir->vars[dir->vars_count++] = vs;
    }

    if (ver >= 9) {
        cwd = dir;
        int nprogs;
        if (!read_int(&nprogs))
            goto fail;
        core_import_programs(nprogs, NULL);
        rebuild_label_table();
        if (ver >= 43)
            for (int i = 0; i < dir->prgms_count; i++)
                if (!read_bool(&dir->prgms[i].locked))
                    goto fail;
    }

    if (ver >= 9) {
        int nc;
        if (!read_int(&nc))
            goto fail;
        dir->children = (subdir_struct *) malloc(nc * sizeof(subdir_struct));
        if (dir->children == NULL && nc != 0)
            goto fail;
        dir->children_capacity = nc;
        dir->children_count = 0;
        for (int i = 0; i < nc; i++) {
            if (!read_char((char *) &dir->children[i].length))
                goto fail;
            if (fread(dir->children[i].name, 1, dir->children[i].length, gfile) != dir->children[i].length)
                goto fail;
            if (ver < 44)
                switch_30_and_94(dir->children[i].name, dir->children[i].length);
            directory *child;
            if (!unpersist_directory(&child))
                goto fail;
            child->parent = dir;
            dir->children[i].dir = child;
            dir->children_count = i + 1;
        }
    }

    *d = dir;
    cwd = oldcwd;
    return true;
}

static bool persist_globals() {
    int i;
    bool ret = false;
    pgm_index saved_prgm;

    if (!write_int(reg_alpha_length))
        goto done;
    if (fwrite(reg_alpha, 1, 44, gfile) != 44)
        goto done;
    if (!write_int4(mode_sigma_reg))
        goto done;
    if (!write_int(mode_goose))
        goto done;
    if (!write_bool(mode_time_clktd))
        goto done;
    if (!write_bool(mode_time_clk24))
        goto done;
    if (!write_int(mode_wsize))
        goto done;
    if (!write_bool(mode_header))
        goto done;
    if (!write_int(mode_amort_seq))
        goto done;
    if (fwrite(&flags, 1, sizeof(flags_struct), gfile) != sizeof(flags_struct))
        goto done;
    if (!write_int(mode_message_lines))
        goto done;

    {
        int n_eq = 0;
        for (int i = 0; i < eq_dir->prgms_count; i++)
            if (eq_dir->prgms[i].eq_data != NULL)
                n_eq++;
        if (!write_int(n_eq))
            goto done;
        for (int i = 0; i < eq_dir->prgms_count; i++) {
            equation_data *eqd = eq_dir->prgms[i].eq_data;
            if (eqd == NULL)
                continue;
            directory *saved_cwd = cwd;
            cwd = eq_dir;
            core_export_programs(1, &i, NULL);
            cwd = saved_cwd;
            if (!write_int(eqd->eqn_index))
                return false;
            if (!write_int4(eqd->length))
                return false;
            if (fwrite(eqd->text, 1, eqd->length, gfile) != eqd->length)
                return false;
            int cmsize = eqd->map == NULL ? 0 : eqd->map->getSize();
            if (!write_int(cmsize))
                return false;
            if (cmsize > 0)
                if (fwrite(eqd->map->getData(), 1, cmsize, gfile) != cmsize)
                    return false;
            if (!write_bool(eqd->compatMode))
                return false;
        }
    }

    if (!write_int(sp))
        goto done;
    for (int i = 0; i <= sp; i++)
        if (!persist_vartype(stack[i]))
            goto done;
    if (!persist_vartype(lastx))
        goto done;
    if (!write_int4(current_prgm.dir))
        goto done;
    if (!write_int4(current_prgm.idx))
        goto done;
    if (!write_int4(pc2line(pc)))
        goto done;
    if (!write_int(prgm_highlight_row))
        goto done;
    if (!persist_directory(root))
        goto done;
    if (!write_int(local_vars_count))
        goto done;
    for (i = 0; i < local_vars_count; i++) {
        if (!write_char(local_vars[i].length)
            || fwrite(local_vars[i].name, 1, local_vars[i].length, gfile) != local_vars[i].length
            || !write_int2(local_vars[i].level)
            || !write_int2(local_vars[i].flags)
            || !persist_vartype(local_vars[i].value))
            goto done;
    }
    if (!write_bool(mode_plot_viewer))
        goto done;
    if (!write_int(mode_plot_key))
        goto done;
    if (!write_int(mode_plot_sp))
        goto done;
    if (!persist_vartype(mode_plot_inv))
        goto done;
    if (!write_int(mode_plot_result_width))
        goto done;
    if (!write_bool(mode_multi_line))
        goto done;
    if (!write_bool(mode_lastx_top))
        goto done;
    if (!write_bool(mode_alpha_top))
        goto done;
    if (!write_bool(mode_header_flags))
        goto done;
    if (!write_bool(mode_header_polar))
        goto done;
    if (!write_bool(mode_matedit_stk))
        goto done;
    if (!persist_vartype(varmenu_eqn))
        goto done;
    if (!write_int(varmenu_length))
        goto done;
    if (fwrite(varmenu, 1, 7, gfile) != 7)
        goto done;
    if (!write_int(varmenu_rows))
        goto done;
    if (!write_int(varmenu_row))
        goto done;
    for (i = 0; i < 6; i++)
        if (!write_char(varmenu_labellength[i])
                || fwrite(varmenu_labeltext[i], 1, varmenu_labellength[i], gfile) != varmenu_labellength[i])
            goto done;
    if (!write_int(varmenu_role))
        goto done;
    if (!write_int(rtn_level))
        goto done;
    if (!write_bool(rtn_level_0_has_matrix_entry))
        goto done;
    if (!write_bool(rtn_level_0_has_func_state))
        goto done;
    if (!write_int4(rtn_after_last_rtn_dir))
        goto done;
    if (!write_int4(rtn_after_last_rtn_prgm))
        goto done;
    if (!write_int4(rtn_after_last_rtn_pc))
        goto done;
    saved_prgm = current_prgm;
    for (i = rtn_level - 1; i >= 0; i--) {
        current_prgm.set(rtn_stack[i].dir, rtn_stack[i].get_prgm());
        int4 line = rtn_stack[i].pc;
        if (current_prgm.idx >= 0)
            line = pc2line(line);
        if (!write_int4(rtn_stack[i].dir)
                || !write_int4(rtn_stack[i].prgm)
                || !write_int4(line))
            goto done;
    }
    current_prgm = saved_prgm;
    if (!write_bool(rtn_solve_active))
        goto done;
    if (!write_bool(rtn_integ_active))
        goto done;
    if (!write_bool(rtn_plot_active))
        goto done;
    if (!write_int4(cwd->id))
        goto done;
    ret = true;

    done:
    return ret;
}

bool loading_state = false;
bool saving_state = false;

static bool unpersist_globals() {
    int i;
    bool ret = false;

    if (!read_int(&reg_alpha_length)) {
        reg_alpha_length = 0;
        goto done;
    }
    if (fread(reg_alpha, 1, 44, gfile) != 44) {
        reg_alpha_length = 0;
        goto done;
    }
    if (ver < 44)
        switch_30_and_94(reg_alpha, reg_alpha_length);
    if (!read_int4(&mode_sigma_reg)) {
        mode_sigma_reg = 11;
        goto done;
    }
    if (!read_int(&mode_goose)) {
        mode_goose = -1;
        goto done;
    }
    if (!read_bool(&mode_time_clktd)) {
        mode_time_clktd = false;
        goto done;
    }
    if (!read_bool(&mode_time_clk24)) {
        mode_time_clk24 = false;
        goto done;
    }
    if (!read_int(&mode_wsize)) {
        mode_wsize = 36;
        goto done;
    }
    if (ver >= 13) {
        if (!read_bool(&mode_header)) {
            mode_header = true;
            goto done;
        }
    } else
        mode_header = true;
    if (ver >= 18) {
        if (!read_int(&mode_amort_seq)) {
            mode_amort_seq = 0;
            goto done;
        }
    } else
        mode_amort_seq = 0;
    if (ver < 12) {
        bool dummy;
        if (!read_bool(&dummy))
            goto done;
    }
    if (fread(&flags, 1, sizeof(flags_struct), gfile)
            != sizeof(flags_struct))
        goto done;

    if (ver < 21)
        flags.f.direct_solver = 1;

    if (ver < 13) {
        mode_message_lines = flags.farray[51] ? 2 : flags.farray[50] ? 1 : 0;
        flags.farray[50] = flags.farray[51] = 0;
    } else {
        if (!read_int(&mode_message_lines))
            goto done;
    }

    if (ver < 9) {
        int nprogs;
        if (!read_int(&nprogs))
            goto done;
        root = new directory(0);
        map_dir(0, root);
        cwd = root;
        core_import_programs(nprogs, NULL);
        rebuild_label_table();
    }

    dir_list_clear();
    eq_dir = new directory(1);
    map_dir(1, eq_dir);

    if (ver >= 46) {
        int n_eq;
        if (!read_int(&n_eq))
            goto done;
        for (int i = 0; i < n_eq; i++)
            if (unpersist_equation_data() == NULL)
                goto done;
    }

    if (!read_int(&sp)) {
        sp = -1;
        goto done;
    }
    stack_capacity = sp + 1;
    if (stack_capacity < 4)
        stack_capacity = 4;
    stack = (vartype **) malloc(stack_capacity * sizeof(vartype *));
    if (stack == NULL) {
        stack_capacity = 0;
        sp = -1;
        goto done;
    }
    for (int i = 0; i <= sp; i++) {
        if (!unpersist_vartype(&stack[i]) || stack[i] == NULL) {
            for (int j = 0; j < i; j++)
                free_vartype(stack[j]);
            free(stack);
            stack = NULL;
            sp = -1;
            stack_capacity = 0;
            goto done;
        }
    }

    free_vartype(lastx);
    if (!unpersist_vartype(&lastx))
        goto done;

    int4 currdir, currprgm, currpc;
    if (ver >= 9) {
        if (!read_int4(&currdir)) {
            current_prgm.set(-1, 0);
            goto done;
        }
    }
    if (!read_int4(&currprgm)) {
        current_prgm.set(-1, 0);
        goto done;
    }
    if (!read_int4(&currpc)) {
        pc = -1;
        goto done;
    }
    if (!read_int(&prgm_highlight_row)) {
        prgm_highlight_row = 0;
        goto done;
    }

    directory *r;
    if (!unpersist_directory(&r))
        goto done;
    if (ver >= 9) {
        root = r;
    } else {
        free(r->prgms);
        r->prgms = root->prgms;
        r->prgms_count = root->prgms_count;
        r->prgms_capacity = root->prgms_capacity;
        root->prgms = NULL;
        root->prgms_count = 0;
        delete root;
        root = r;
    }

    if (local_vars != NULL) {
        free(local_vars);
        local_vars = NULL;
    }
    local_vars_count = 0;
    local_vars_capacity = 0;

    if (ver >= 9) {
        int lc;
        if (!read_int(&lc))
            goto done;
        local_vars = (var_struct *) malloc(lc * sizeof(var_struct));
        if (local_vars == NULL && lc != 0)
            goto done;
        local_vars_capacity = lc;
        for (int i = 0; i < lc; i++) {
            if (!read_char((char *) &local_vars[i].length))
                goto done;
            if (fread(local_vars[i].name, 1, local_vars[i].length, gfile) != local_vars[i].length)
                goto done;
            if (ver < 44)
                switch_30_and_94(local_vars[i].name, local_vars[i].length);
            if (!read_int2(&local_vars[i].level))
                goto done;
            if (!read_int2(&local_vars[i].flags))
                goto done;
            if (!unpersist_vartype(&local_vars[i].value))
                goto done;
            local_vars_count++;
        }
        // Just to have something valid for now; we'll set cwd properly at the end.
        cwd = root;
    } else {
        // Separate locals from the globals
        local_vars = (var_struct *) malloc(root->vars_count * sizeof(var_struct));
        if (local_vars == NULL && root->vars_count != 0)
            goto done;
        local_vars_capacity = root->vars_count;
        int gi = 0, li = 0;
        for (int i = 0; i < root->vars_count; i++) {
            if (root->vars[i].level == -1)
                root->vars[gi++] = root->vars[i];
            else
                local_vars[li++] = root->vars[i];
        }
        root->vars_count = gi;
        local_vars_count = li;
        cwd = root;
    }

    if (ver >= 20) {
        if (!read_bool(&mode_plot_viewer)) {
            mode_plot_viewer = false;
            goto done;
        }
        if (!read_int(&mode_plot_key)) {
            mode_plot_key = 0;
            goto done;
        }
        if (!read_int(&mode_plot_sp)) {
            mode_plot_sp = 0;
            goto done;
        }
        if (!unpersist_vartype(&mode_plot_inv)) {
            mode_plot_inv = NULL;
            goto done;
        }
    } else {
        mode_plot_viewer = false;
        mode_plot_key = 0;
        mode_plot_sp = 0;
        mode_plot_inv = NULL;
    }

    if (ver >= 21) {
        if (!read_int(&mode_plot_result_width)) {
            mode_plot_result_width = 0;
            goto done;
        }
    } else {
        mode_plot_result_width = 0;
    }

    if (ver >= 27) {
        if (!read_bool(&mode_multi_line)) {
            mode_multi_line = true;
            goto done;
        }
    } else {
        mode_multi_line = true;
    }

    if (ver >= 29) {
        if (!read_bool(&mode_lastx_top)) {
            mode_lastx_top = false;
            goto done;
        }
    } else {
        mode_lastx_top = false;
    }

    if (ver >= 30) {
        if (!read_bool(&mode_alpha_top)) {
            mode_alpha_top = false;
            goto done;
        }
    } else {
        mode_alpha_top = false;
    }

    if (ver >= 31) {
        if (!read_bool(&mode_header_flags) || !read_bool(&mode_header_polar)) {
            mode_header_flags = false;
            mode_header_polar = false;
            goto done;
        }
    } else {
        mode_header_flags = false;
        mode_header_polar = false;
    }

    if (ver >= 38) {
        if (!read_bool(&mode_matedit_stk)) return false;
    } else {
        mode_matedit_stk = false;
    }

    if (!unpersist_vartype(&varmenu_eqn)) {
        varmenu_eqn = NULL;
        goto done;
    }
    if (!read_int(&varmenu_length))
        goto varmenu_fail;
    if (fread(varmenu, 1, 7, gfile) != 7)
        goto varmenu_fail;
    if (ver < 44)
        switch_30_and_94(varmenu, varmenu_length);
    if (!read_int(&varmenu_rows))
        goto varmenu_fail;
    if (!read_int(&varmenu_row)) {
        varmenu_fail:
        free_vartype(varmenu_eqn);
        varmenu_eqn = NULL;
        varmenu_length = 0;
        goto done;
    }
    char c;
    for (i = 0; i < 6; i++) {
        if (!read_char(&c)
                || fread(varmenu_labeltext[i], 1, c, gfile) != c)
            goto done;
        varmenu_labellength[i] = c;
        if (ver < 44)
            switch_30_and_94(varmenu_labeltext[i], varmenu_labellength[i]);
    }
    if (!read_int(&varmenu_role))
        goto done;
    if (ver < 9) {
        // rtn_sp, obsolete
        int dummy;
        if (!read_int(&dummy))
            goto done;
    }
    if (!read_int(&rtn_level))
        goto done;
    if (!read_bool(&rtn_level_0_has_matrix_entry))
        goto done;
    if (!read_bool(&rtn_level_0_has_func_state))
        goto done;
    if (ver < 9) {
        rtn_after_last_rtn_dir = cwd->id;
    } else {
        if (!read_int4(&rtn_after_last_rtn_dir))
            goto done;
    }
    if (!read_int4(&rtn_after_last_rtn_prgm))
        goto done;
    if (!read_int4(&rtn_after_last_rtn_pc))
        goto done;
    rtn_stack_capacity = 16;
    while (rtn_level > rtn_stack_capacity)
        rtn_stack_capacity <<= 1;
    rtn_stack = (rtn_stack_entry *) realloc(rtn_stack, rtn_stack_capacity * sizeof(rtn_stack_entry));
    if (ver >= 9) {
        for (i = rtn_level - 1; i >= 0; i--) {
            int4 line;
            if (!read_int4(&rtn_stack[i].dir)) goto done;
            if (!read_int4(&rtn_stack[i].prgm)) goto done;
            if (!read_int4(&line)) goto done;
            current_prgm.set(rtn_stack[i].dir, rtn_stack[i].get_prgm());
            if (current_prgm.idx >= 0)
                line = line2pc(line);
            rtn_stack[i].pc = line;
        }
    } else {
        for (i = rtn_level - 1; i >= 0; i--) {
            bool matrix_entry_follows;
            if (i == 0) {
                matrix_entry_follows = rtn_level_0_has_matrix_entry;
            } else {
                int4 prgm, line;
                if (!read_int4(&prgm) || !read_int4(&line))
                    goto done;
                if (prgm >= root->prgms_count) {
                    rtn_stack[i].dir = eq_dir->id;
                    rtn_stack[i].prgm = prgm - root->prgms_count;
                } else {
                    rtn_stack[i].dir = root->id;
                    rtn_stack[i].prgm = prgm;
                }
                matrix_entry_follows = rtn_stack[i].has_matrix();
                current_prgm.set(rtn_stack[i].dir, rtn_stack[i].get_prgm());
                if (current_prgm.idx >= 0)
                    line = line2pc(line);
                rtn_stack[i].pc = line;
            }
            if (matrix_entry_follows) {
                char dummy1;
                char dummy2[7];
                int4 dummy3, dummy4;
                if (!read_char(&dummy1)
                        || fread(dummy2, 1, dummy1, gfile) != dummy1
                        || !read_int4(&dummy3)
                        || !read_int4(&dummy4))
                    goto done;
                // Not doing anything with these old matrix stack entries,
                // since they don't contain the directory id / stack level.
                // Without that information, we can't reliably use them.
            }
        }
    }
    if (!read_bool(&rtn_solve_active))
        goto done;
    if (!read_bool(&rtn_integ_active))
        goto done;
    if (ver >= 20) {
        if (!read_bool(&rtn_plot_active))
            goto done;
    } else
        rtn_plot_active = false;
    if (ver >= 9) {
        int4 cwd_id;
        if (!read_int4(&cwd_id))
            goto done;
        cwd = dir_list[cwd_id];
    } else {
        cwd = root;
    }

    if (ver < 9) {
        if (currprgm >= root->prgms_count)
            current_prgm.set(eq_dir->id, currprgm - root->prgms_count);
        else
            current_prgm.set(root->id, currprgm);
    } else {
        current_prgm.set(currdir, currprgm);
    }
    pc = currpc;

    // Deferring the line2pc() conversion for pc and incomplete_saved_pc.
    // It is possible that the current program is generated code that is
    // referenced only by private SOLVE or INTEG variables, in which case
    // it hasn't been loaded yet. So, to be safe, these conversions must
    // be performed after unpersist_math().

    ret = true;

    done:
    return ret;
}

static bool make_prgm_space(directory *dir, int n) {
    if (dir->prgms_count + n <= dir->prgms_capacity)
        return true;
    int new_prgms_capacity = dir->prgms_capacity + n + 10;
    prgm_struct *new_prgms = (prgm_struct *) realloc(dir->prgms, new_prgms_capacity * sizeof(prgm_struct));
    if (new_prgms == NULL)
        return false;
    for (int i = dir->prgms_capacity; i < new_prgms_capacity; i++) {
        new_prgms[i].text = NULL;
        new_prgms[i].eq_data = NULL;
        new_prgms[i].locked = false;
    }
    dir->prgms = new_prgms;
    dir->prgms_capacity = new_prgms_capacity;
    return true;
}

int4 new_eqn_idx() {
    for (int4 i = 0; i < eq_dir->prgms_capacity; i++) {
        if (eq_dir->prgms[i].text == NULL) {
            if (i + 1 > eq_dir->prgms_count)
                eq_dir->prgms_count = i + 1;
            return i;
        }
    }
    if (!make_prgm_space(eq_dir, 1))
        return -1;
    return eq_dir->prgms_count++;
}

void clear_rtns_vars_and_prgms() {
    clear_all_rtns();
    current_prgm.set(-1, 0);

    delete root;
    root = NULL;
    cwd = NULL;
    delete eq_dir;
    eq_dir = NULL;
    dir_list_clear();
}

static int clear_prgm_by_index(pgm_index prgm);

int clear_prgm(const arg_struct *arg) {
    pgm_index prgm;
    if (arg->type == ARGTYPE_LBLINDEX) {
        directory *dir = get_dir(arg->target);
        prgm.set(dir->id, dir->labels[arg->val.num].prgm);
    } else if (arg->type == ARGTYPE_STR) {
        if (arg->length == 0) {
            if (current_prgm.idx < 0)
                return ERR_INTERNAL_ERROR;
            if (!current_prgm.is_editable())
                return ERR_RESTRICTED_OPERATION;
            prgm = current_prgm;
        } else {
            int i;
            for (i = cwd->labels_count - 1; i >= 0; i--)
                if (string_equals(arg->val.text, arg->length,
                                  cwd->labels[i].name, cwd->labels[i].length))
                    goto found;
            return ERR_LABEL_NOT_FOUND;
            found:
            prgm.set(cwd->id, cwd->labels[i].prgm);
        }
    }
    return clear_prgm_by_index(prgm);
}

static int clear_prgm_by_index(pgm_index prgm) {
    int i, j;
    if (prgm.dir == eq_dir->id || prgm.idx < 0)
        return ERR_LABEL_NOT_FOUND;
    clear_all_rtns();
    directory *dir = dir_list[prgm.dir];
    count_embed_references(dir, prgm.idx, false);
    if (prgm == current_prgm)
        pc = -1;
    else if (current_prgm.dir == prgm.dir && current_prgm.idx > prgm.idx)
        current_prgm.set(current_prgm.dir, current_prgm.idx - 1);

    free(dir->prgms[prgm.idx].text);
    for (i = prgm.idx; i < dir->prgms_count - 1; i++)
        dir->prgms[i] = dir->prgms[i + 1];
    dir->prgms[dir->prgms_count - 1].text = NULL;
    dir->prgms_count--;
    i = j = 0;
    while (j < dir->labels_count) {
        if (j > i)
            dir->labels[i] = dir->labels[j];
        j++;
        if (dir->labels[i].prgm > prgm.idx) {
            dir->labels[i].prgm--;
            i++;
        } else if (dir->labels[i].prgm < prgm.idx)
            i++;
    }
    dir->labels_count = i;
    if (dir->prgms_count == 0 || prgm.idx == dir->prgms_count) {
        pgm_index saved_prgm = current_prgm;
        int saved_pc = pc;
        goto_dot_dot(false);
        current_prgm = saved_prgm;
        pc = saved_pc;
    }
    update_catalog();
    return ERR_NONE;
}

int clear_prgm_by_int_index(int prgm) {
    pgm_index idx;
    idx.set(cwd->id, prgm);
    return clear_prgm_by_index(idx);
}

void clear_prgm_lines(int4 count) {
    int4 frompc, deleted, i, j;
    if (pc == -1)
        pc = 0;
    frompc = pc;
    while (count > 0) {
        int command;
        arg_struct arg;
        get_next_command(&pc, &command, &arg, 0, NULL);
        if (command == CMD_END) {
            pc -= 2;
            break;
        }
        if (command == CMD_EMBED)
            remove_equation_reference(arg.val.num);
        count--;
    }
    deleted = pc - frompc;

    int4 idx = current_prgm.idx;
    for (i = pc; i < cwd->prgms[idx].size; i++)
        cwd->prgms[idx].text[i - deleted] = cwd->prgms[idx].text[i];
    cwd->prgms[idx].size -= deleted;
    pc = frompc;

    i = j = 0;
    while (j < cwd->labels_count) {
        if (j > i)
            cwd->labels[i] = cwd->labels[j];
        j++;
        if (cwd->labels[i].prgm == current_prgm.idx) {
            if (cwd->labels[i].pc < frompc)
                i++;
            else if (cwd->labels[i].pc >= frompc + deleted) {
                cwd->labels[i].pc -= deleted;
                i++;
            }
        } else
            i++;
    }
    cwd->labels_count = i;

    invalidate_lclbls(current_prgm, false);
    clear_all_rtns();
}

void goto_dot_dot(bool force_new) {
    if (!loading_state) {
        clear_all_rtns();
        if (flags.f.prgm_mode && current_prgm.dir != eq_dir->id && current_prgm.dir != cwd->id) {
            directory *dir = get_dir(current_prgm.dir);
            if (dir != NULL) {
                cwd = dir;
                return;
            }
        }
    }
    int command;
    arg_struct arg;
    if (cwd->prgms_count != 0 && !force_new) {
        /* Check if last program is empty */
        pc = 0;
        current_prgm.set(cwd->id, cwd->prgms_count - 1);
        get_next_command(&pc, &command, &arg, 0, NULL);
        if (command == CMD_END) {
            pc = -1;
            return;
        }
    }
    if (cwd->prgms_count == cwd->prgms_capacity) {
        prgm_struct *newprgms;
        int i;
        cwd->prgms_capacity += 10;
        newprgms = (prgm_struct *) malloc(cwd->prgms_capacity * sizeof(prgm_struct));
        // TODO - handle memory allocation failure
        for (i = cwd->prgms_capacity - 10; i < cwd->prgms_capacity; i++) {
            newprgms[i].text = NULL;
            newprgms[i].eq_data = NULL;
        }
        for (i = 0; i < cwd->prgms_count; i++)
            newprgms[i] = cwd->prgms[i];
        if (cwd->prgms != NULL)
            free(cwd->prgms);
        cwd->prgms = newprgms;
    }
    int4 idx = cwd->prgms_count++;
    current_prgm.set(cwd->id, idx);
    cwd->prgms[idx].capacity = 0;
    cwd->prgms[idx].size = 0;
    cwd->prgms[idx].lclbl_invalid = true;
    cwd->prgms[idx].locked = false;
    cwd->prgms[idx].text = NULL;
    command = CMD_END;
    arg.type = ARGTYPE_NONE;
    store_command(0, command, &arg, NULL);
    pc = -1;
}

static bool mvar_prgms_exist(directory *dir) {
    for (int i = 0; i < dir->labels_count; i++)
        if (label_has_mvar(dir->id, i))
            return true;
    return false;
}

bool mvar_prgms_exist() {
    directory *dir = cwd;
    while (true) {
        if (mvar_prgms_exist(dir))
            return true;
        if (dir == root)
            break;
        dir = dir->parent;
    }
    vartype_list *path = get_path();
    if (path == NULL)
        return false;
    for (int i = 0; i < path->size; i++) {
        vartype *v = path->array->data[i];
        if (v->type != TYPE_DIR_REF)
            continue;
        vartype_dir_ref *r = (vartype_dir_ref *) v;
        dir = get_dir(r->dir);
        if (dir != NULL && mvar_prgms_exist(dir))
            return true;
    }
    return false;
}

bool label_has_mvar(int4 dir_id, int lblindex) {
    directory *dir = dir_list[dir_id];
    if (dir->labels[lblindex].length == 0)
        return false;
    pgm_index saved_prgm = current_prgm;
    current_prgm.set(dir->id, dir->labels[lblindex].prgm);
    int4 pc = dir->labels[lblindex].pc;
    pc += get_command_length(current_prgm, pc);
    int command;
    arg_struct arg;
    get_next_command(&pc, &command, &arg, 0, NULL);
    current_prgm = saved_prgm;
    return command == CMD_MVAR;
}

int get_command_length(pgm_index idx, int4 pc) {
    prgm_struct *prgm = dir_list[idx.dir]->prgms + idx.idx;
    int4 pc2 = pc;
    int command = prgm->text[pc2++];
    int argtype = prgm->text[pc2++];
    command |= (argtype & 112) << 4;
    bool have_orig_num = command == CMD_NUMBER && (argtype & 128) != 0;
    argtype &= 15;

    if ((command == CMD_GTO || command == CMD_XEQ)
            && (argtype == ARGTYPE_NUM || argtype == ARGTYPE_STK
                                       || argtype == ARGTYPE_LCLBL)
            || command == CMD_GTOL || command == CMD_XEQL)
        pc2 += 4;
    switch (argtype) {
        case ARGTYPE_NUM:
        case ARGTYPE_NEG_NUM:
        case ARGTYPE_IND_NUM: {
            while ((prgm->text[pc2++] & 128) == 0);
            break;
        }
        case ARGTYPE_STK:
        case ARGTYPE_IND_STK:
        case ARGTYPE_LCLBL:
            pc2++;
            break;
        case ARGTYPE_STR:
        case ARGTYPE_IND_STR: {
            pc2 += prgm->text[pc2] + 1;
            break;
        }
        case ARGTYPE_DOUBLE:
            pc2 += sizeof(phloat);
            break;
        case ARGTYPE_XSTR: {
            int xl = prgm->text[pc2++];
            xl += prgm->text[pc2++] << 8;
            pc2 += xl;
            break;
        }
    }
    if (have_orig_num)
        while (prgm->text[pc2++]);
    if (command == CMD_N_PLUS_U) {
        /* We don't just blindly get the lengths of the next two commands;
         * we check what they are, because we want to be robust in case N+U is
         * *not* followed by a NUMBER and an XSTR.
         */
        arg_struct arg;
        int4 pc3 = pc2;
        pgm_index saved_prgm = current_prgm;
        current_prgm = idx;
        get_next_command(&pc3, &command, &arg, 0, NULL);
        if (command == CMD_NUMBER) {
            get_next_command(&pc3, &command, &arg, 0, NULL);
            if (command == CMD_XSTR)
                pc2 = pc3;
        }
        current_prgm = saved_prgm;
    }
    return pc2 - pc;
}

void get_next_command(int4 *pc, int *command, arg_struct *arg, int find_target, const char **num_str) {
    prgm_struct *prgm = dir_list[current_prgm.dir]->prgms + current_prgm.idx;
    int i;
    int4 target_pc;
    int4 orig_pc = *pc;

    *command = prgm->text[(*pc)++];
    arg->type = prgm->text[(*pc)++];
    *command |= (arg->type & 112) << 4;
    bool have_orig_num = *command == CMD_NUMBER && (arg->type & 128) != 0;
    arg->type &= 15;

    if (*command == CMD_N_PLUS_U) {
        /* N+U should always be followed by NUMBER and XSTR.
         * But we try to be nice about it if it isn't...
         */
        int4 pc2 = *pc;
        int command2;
        get_next_command(&pc2, &command2, arg, false, num_str);
        if (command2 != CMD_NUMBER) {
            no_unit:
            arg->type = ARGTYPE_NONE;
            return;
        }
        phloat ph = arg->val_d;
        get_next_command(&pc2, &command2, arg, false, NULL);
        if (command2 != CMD_XSTR) {
            *num_str = NULL;
            goto no_unit;
        }
        arg->val_d = ph;
        *pc = pc2;
        /* Returning the number *and* unit in one arg_struct.
         * Note that type == ARGTYPE_XSTR at this point.
         * Don't try this at home...
         */
        return;
    }

    if ((*command == CMD_GTO || *command == CMD_XEQ)
            && (arg->type == ARGTYPE_NUM
                || arg->type == ARGTYPE_LCLBL
                || arg->type == ARGTYPE_STK)
            || *command == CMD_GTOL || *command == CMD_XEQL) {
        if (find_target) {
            target_pc = 0;
            for (i = 0; i < 4; i++)
                target_pc = (target_pc << 8) | prgm->text[(*pc)++];
            if (target_pc != -1) {
                arg->target = target_pc;
                find_target = 0;
            }
        } else
            (*pc) += 4;
    } else {
        find_target = 0;
        arg->target = -1;
    }

    switch (arg->type) {
        case ARGTYPE_NUM:
        case ARGTYPE_NEG_NUM:
        case ARGTYPE_IND_NUM: {
            int4 num = 0;
            unsigned char c;
            do {
                c = prgm->text[(*pc)++];
                num = (num << 7) | (c & 127);
            } while ((c & 128) == 0);
            if (arg->type == ARGTYPE_NEG_NUM) {
                arg->type = ARGTYPE_NUM;
                num = -num;
            }
            arg->val.num = num;
            break;
        }
        case ARGTYPE_STK:
        case ARGTYPE_IND_STK:
            arg->val.stk = prgm->text[(*pc)++];
            break;
        case ARGTYPE_LCLBL:
            arg->val.lclbl = prgm->text[(*pc)++];
            break;
        case ARGTYPE_STR:
        case ARGTYPE_IND_STR: {
            arg->length = prgm->text[(*pc)++];
            for (i = 0; i < arg->length; i++)
                arg->val.text[i] = prgm->text[(*pc)++];
            break;
        }
        case ARGTYPE_DOUBLE: {
            unsigned char *b = (unsigned char *) &arg->val_d;
            for (int i = 0; i < (int) sizeof(phloat); i++)
                *b++ = prgm->text[(*pc)++];
            break;
        }
        case ARGTYPE_XSTR: {
            int xstr_len = prgm->text[(*pc)++];
            xstr_len += prgm->text[(*pc)++] << 8;
            arg->length = xstr_len;
            arg->val.xstr = (const char *) (prgm->text + *pc);
            (*pc) += xstr_len;
            break;
        }
    }

    if (*command == CMD_NUMBER) {
        if (have_orig_num) {
            char *p = (char *) &prgm->text[*pc];
            if (num_str != NULL)
                *num_str = p;
            /* Make sure the decimal stored in the program matches
             * the current setting of flag 28.
             */
            char wrong_dot = flags.f.decimal_point ? ',' : '.';
            char right_dot = flags.f.decimal_point ? '.' : ',';
            int numlen = 1;
            while (*p != 0) {
                if (*p == wrong_dot)
                    *p = right_dot;
                p++;
                numlen++;
            }
            *pc += numlen;
        } else {
            if (num_str != NULL)
                *num_str = NULL;
        }
        if (arg->type != ARGTYPE_DOUBLE) {
            /* argtype is ARGTYPE_NUM; convert to phloat */
            arg->val_d = arg->val.num;
            arg->type = ARGTYPE_DOUBLE;
        }
    }

    if (find_target) {
        if (*command == CMD_GTOL || *command == CMD_XEQL)
            target_pc = line2pc(arg->val.num);
        else
            target_pc = find_local_label(arg);
        arg->target = target_pc;
        for (i = 5; i >= 2; i--) {
            prgm->text[orig_pc + i] = target_pc;
            target_pc >>= 8;
        }
        prgm->lclbl_invalid = false;
    }
}

void rebuild_label_table() {
    /* TODO -- this is *not* efficient; inserting and deleting ENDs and
     * global LBLs should not cause every single program to get rescanned!
     * But, I don't feel like dealing with that at the moment, so just
     * this ugly brute force approach for now.
     */
    int prgm_index;
    int4 pc;
    cwd->labels_count = 0;
    for (prgm_index = 0; prgm_index < cwd->prgms_count; prgm_index++) {
        prgm_struct *prgm = cwd->prgms + prgm_index;
        pc = 0;
        while (pc < prgm->size) {
            int command = prgm->text[pc];
            int argtype = prgm->text[pc + 1];
            command |= (argtype & 112) << 4;
            argtype &= 15;

            if (command == CMD_END
                        || (command == CMD_LBL && argtype == ARGTYPE_STR)) {
                label_struct *newlabel;
                if (cwd->labels_count == cwd->labels_capacity) {
                    label_struct *newlabels;
                    int i;
                    cwd->labels_capacity += 50;
                    newlabels = (label_struct *)
                                malloc(cwd->labels_capacity * sizeof(label_struct));
                    // TODO - handle memory allocation failure
                    for (i = 0; i < cwd->labels_count; i++)
                        newlabels[i] = cwd->labels[i];
                    if (cwd->labels != NULL)
                        free(cwd->labels);
                    cwd->labels = newlabels;
                }
                newlabel = cwd->labels + cwd->labels_count++;
                if (command == CMD_END)
                    newlabel->length = 0;
                else {
                    int i;
                    newlabel->length = prgm->text[pc + 2];
                    for (i = 0; i < newlabel->length; i++)
                        newlabel->name[i] = prgm->text[pc + 3 + i];
                }
                newlabel->prgm = prgm_index;
                newlabel->pc = pc;
            }
            pgm_index idx;
            idx.set(cwd->id, prgm_index);
            pc += get_command_length(idx, pc);
        }
    }
}

static void update_label_table(pgm_index prgm, int4 pc, int inserted) {
    directory *dir = dir_list[prgm.dir];
    for (int i = 0; i < dir->labels_count; i++) {
        if (dir->labels[i].prgm > prgm.idx)
            return;
        if (dir->labels[i].prgm == prgm.idx && dir->labels[i].pc >= pc)
            dir->labels[i].pc += inserted;
    }
}

static void invalidate_lclbls(pgm_index idx, bool force) {
    prgm_struct *prgm = dir_list[idx.dir]->prgms + idx.idx;
    if (force || !prgm->lclbl_invalid) {
        int4 pc2 = 0;
        while (pc2 < prgm->size) {
            int command = prgm->text[pc2];
            int argtype = prgm->text[pc2 + 1];
            command |= (argtype & 112) << 4;
            argtype &= 15;
            if ((command == CMD_GTO || command == CMD_XEQ)
                    && (argtype == ARGTYPE_NUM || argtype == ARGTYPE_STK
                                               || argtype == ARGTYPE_LCLBL)
                    || command == CMD_GTOL || command == CMD_XEQL) {
                /* A dest_pc value of -1 signals 'unknown',
                 * -2 means 'nonexistent', and anything else is
                 * the pc where the destination label is found.
                 */
                int4 pos;
                for (pos = pc2 + 2; pos < pc2 + 6; pos++)
                    prgm->text[pos] = 255;
            }
            pc2 += get_command_length(idx, pc2);
        }
        prgm->lclbl_invalid = true;
    }
}

void count_embed_references(directory *dir, int prgm, bool up) {
    int4 pc = 0;
    int command;
    arg_struct arg;
    
    if (dir->prgms[prgm].text == NULL)
        return;

    while (true) {
        pgm_index saved_prgm = current_prgm;
        current_prgm.set(dir->id, prgm);
        get_next_command(&pc, &command, &arg, 0, NULL);
        current_prgm = saved_prgm;

        if (command == CMD_END)
            break;
        if (command == CMD_EMBED) {
            int4 id = arg.val.num;
            equation_data *eqd = eq_dir->prgms[id].eq_data;
            if (up)
                eqd->refcount++;
            else
                remove_equation_reference(id);
        }
    }
}

static void count_embed_references(directory *dir, bool up) {
    for (int i = 0; i < dir->children_count; i++)
        count_embed_references(dir->children[i].dir, up);

    for (int i = 0; i < dir->prgms_count; i++)
        count_embed_references(dir, i, up);
}

void delete_command(int4 pc) {
    directory *dir = dir_list[current_prgm.dir];
    prgm_struct *prgm = dir->prgms + current_prgm.idx;
    int command = prgm->text[pc];
    int argtype = prgm->text[pc + 1];
    int length = get_command_length(current_prgm, pc);
    int4 pos;

    command |= (argtype & 112) << 4;
    argtype &= 15;

    if (command == CMD_END) {
        int4 newsize;
        prgm_struct *nextprgm;
        if (current_prgm.idx == dir->prgms_count - 1)
            /* Don't allow deletion of last program's END. */
            return;
        nextprgm = prgm + 1;
        prgm->size -= 2;
        newsize = prgm->size + nextprgm->size;
        if (newsize > prgm->capacity) {
            int4 newcapacity = (newsize + 511) & ~511;
            unsigned char *newtext = (unsigned char *) malloc(newcapacity);
            // TODO - handle memory allocation failure
            for (pos = 0; pos < prgm->size; pos++)
                newtext[pos] = prgm->text[pos];
            free(prgm->text);
            prgm->text = newtext;
            prgm->capacity = newcapacity;
        }
        for (pos = 0; pos < nextprgm->size; pos++)
            prgm->text[prgm->size++] = nextprgm->text[pos];
        free(nextprgm->text);
        clear_all_rtns();
        for (pos = current_prgm.idx + 1; pos < dir->prgms_count - 1; pos++)
            dir->prgms[pos] = dir->prgms[pos + 1];
        dir->prgms[dir->prgms_count - 1].text = NULL;
        dir->prgms[dir->prgms_count - 1].eq_data = NULL;
        dir->prgms_count--;
        rebuild_label_table();
        invalidate_lclbls(current_prgm, true);
        draw_varmenu();
        return;
    }

    if (command == CMD_EMBED) {
        int4 pc2 = pc;
        arg_struct arg;
        get_next_command(&pc2, &command, &arg, 0, NULL);
        remove_equation_reference(arg.val.num);
    }

    for (pos = pc; pos < prgm->size - length; pos++)
        prgm->text[pos] = prgm->text[pos + length];
    prgm->size -= length;
    if (command == CMD_LBL && argtype == ARGTYPE_STR)
        rebuild_label_table();
    else
        update_label_table(current_prgm, pc, -length);
    invalidate_lclbls(current_prgm, false);
    clear_all_rtns();
    draw_varmenu();
}

int eqn_flip(int4 pc) {
    if (!current_prgm.is_editable())
        return ERR_RESTRICTED_OPERATION;
    if (current_prgm.is_locked())
        return ERR_PROGRAM_LOCKED;
    int4 pc2 = pc;
    int cmd;
    arg_struct arg;
    get_next_command(&pc2, &cmd, &arg, 0, NULL);
    if (cmd == CMD_EMBED) {
        directory *dir = dir_list[current_prgm.dir];
        prgm_struct *prgm = dir->prgms + current_prgm.idx;
        prgm->text[pc + 1] ^= 4;
        return ERR_YES;
    } else
        return ERR_NONE;
}

bool store_command(int4 pc, int command, arg_struct *arg, const char *num_str) {
    unsigned char buf[100];
    int bufptr = 0;
    int xstr_len;
    int i;
    int4 pos;
    directory *dir = dir_list[current_prgm.dir];
    prgm_struct *prgm = dir->prgms + current_prgm.idx;

    if (flags.f.prgm_mode) {
        if (!current_prgm.is_editable()) {
            display_error(ERR_RESTRICTED_OPERATION);
            return false;
        }
        if (current_prgm.is_locked()) {
            display_error(ERR_PROGRAM_LOCKED);
            return false;
        }
    }

    /* We should never be called with pc = -1, but just to be safe... */
    if (pc == -1)
        pc = 0;

    if (arg->type == ARGTYPE_NUM && arg->val.num < 0) {
        arg->type = ARGTYPE_NEG_NUM;
        arg->val.num = -arg->val.num;
    } else if (command == CMD_NUMBER) {
        /* Store the string representation of the number, unless it matches
         * the canonical representation, or unless the number is zero.
         */
        if (num_str != NULL) {
            /* If num_str contains an underscore, it's a number with a unit.
             * In that case, we store an N+U instruction first, then the number
             * but with the unit removed, and finally an XSTR with the unit.
             */
            int u = 0;
            while (num_str[u] != 0 && num_str[u] != '_')
                u++;
            if (num_str[u] == '_') {
                bool saved_norm = flags.f.normal_print;
                bool saved_trace = flags.f.trace_print;
                flags.f.normal_print = false;
                flags.f.trace_print = false;
                if (u == 0) {
                    store_command(pc, CMD_NUMBER, arg, NULL);
                } else {
                    char *n = (char *) malloc(u + 1);
                    memcpy(n, num_str, u);
                    n[u] = 0;
                    store_command(pc, CMD_NUMBER, arg, n);
                    free(n);
                }
                int4 pc2 = pc;
                arg_struct arg2;
                arg2.type = ARGTYPE_XSTR;
                arg2.length = (unsigned short) strlen(num_str + u + 1);
                arg2.val.xstr = num_str + u + 1;
                store_command_after(&pc2, CMD_XSTR, &arg2, NULL);
                /* Store N+U last, because of its wacky side effects */
                flags.f.normal_print = saved_norm;
                flags.f.trace_print = saved_trace;
                arg2.type = ARGTYPE_NONE;
                store_command(pc, CMD_N_PLUS_U, &arg2, NULL);
                return true;
            }
            if (arg->val_d == 0) {
                num_str = NULL;
            } else {
                const char *ap = phloat2program(arg->val_d);
                const char *bp = num_str;
                bool equal = true;
                while (1) {
                    char a = *ap++;
                    char b = *bp++;
                    if (a == 0) {
                        if (b != 0)
                            equal = false;
                        break;
                    } else if (b == 0) {
                        goto notequal;
                    }
                    if (a != b) {
                        if (a == 24) {
                            if (b != 'E' && b != 'e')
                                goto notequal;
                        } else if (a == '.' || a == ',') {
                            if (b != '.' && b != ',')
                                goto notequal;
                        } else {
                            notequal:
                            equal = false;
                            break;
                        }
                    }
                }
                if (equal)
                    num_str = NULL;
            }
        }
        /* arg.type is always ARGTYPE_DOUBLE for CMD_NUMBER, but for storage
         * efficiency, we handle integers specially and store them as
         * ARGTYPE_NUM or ARGTYPE_NEG_NUM instead.
         */
        int4 n = to_int4(arg->val_d);
        if (n == arg->val_d && n != (int4) 0x80000000) {
            if (n >= 0) {
                arg->val.num = n;
                arg->type = ARGTYPE_NUM;
            } else {
                arg->val.num = -n;
                arg->type = ARGTYPE_NEG_NUM;
            }
        }
    } else if (arg->type == ARGTYPE_LBLINDEX) {
        int li = arg->val.num;
        directory *d = get_dir(arg->target);
        arg->length = d->labels[li].length;
        for (i = 0; i < arg->length; i++)
            arg->val.text[i] = d->labels[li].name[i];
        arg->type = ARGTYPE_STR;
    }

    buf[bufptr++] = command & 255;
    buf[bufptr++] = arg->type | ((command & 0x700) >> 4) | (command != CMD_NUMBER || num_str == NULL ? 0 : 128);

    /* If the program is nonempty, it must already contain an END,
     * since that's the very first thing that gets stored in any new
     * program. In this case, we need to split the program.
     */
    if (command == CMD_END && prgm->size > 0) {
        prgm_struct *new_prgm;
        if (dir->prgms_count == dir->prgms_capacity) {
            prgm_struct *new_prgms;
            int4 i;
            dir->prgms_capacity += 10;
            new_prgms = (prgm_struct *)
                            malloc(dir->prgms_capacity * sizeof(prgm_struct));
            // TODO - handle memory allocation failure
            for (i = dir->prgms_capacity - 10; i < dir->prgms_capacity; i++) {
                new_prgms[i].text = NULL;
                new_prgms[i].eq_data = NULL;
            }
            int4 cp = current_prgm.idx;
            for (i = 0; i <= cp; i++)
                new_prgms[i] = dir->prgms[i];
            for (i = cp + 1; i < dir->prgms_count; i++)
                new_prgms[i + 1] = dir->prgms[i];
            free(dir->prgms);
            dir->prgms = new_prgms;
            prgm = dir->prgms + cp;
        } else {
            for (i = dir->prgms_count - 1; i > current_prgm.idx; i--)
                dir->prgms[i + 1] = dir->prgms[i];
        }
        dir->prgms_count++;
        new_prgm = prgm + 1;
        new_prgm->size = prgm->size - pc;
        new_prgm->capacity = (new_prgm->size + 511) & ~511;
        new_prgm->text = (unsigned char *) malloc(new_prgm->capacity);
        new_prgm->eq_data = NULL;
        // TODO - handle memory allocation failure
        for (i = pc; i < prgm->size; i++)
            new_prgm->text[i - pc] = prgm->text[i];
        current_prgm.set(current_prgm.dir, current_prgm.idx + 1);

        /* Truncate the previously 'current' program and append an END.
         * No need to check the size against the capacity and grow the
         * program; since it contained an END before, it still has the
         * capacity for one now;
         */
        prgm->size = pc;
        prgm->text[prgm->size++] = CMD_END;
        prgm->text[prgm->size++] = ARGTYPE_NONE;
        pgm_index before;
        before.set(current_prgm.dir, current_prgm.idx - 1);
        if (flags.f.printer_exists && (flags.f.trace_print || flags.f.normal_print))
            print_program_line(before, pc);

        rebuild_label_table();
        invalidate_lclbls(current_prgm, true);
        invalidate_lclbls(before, true);
        clear_all_rtns();
        draw_varmenu();
        return true;
    }

    if ((command == CMD_GTO || command == CMD_XEQ)
            && (arg->type == ARGTYPE_NUM || arg->type == ARGTYPE_STK
                                         || arg->type == ARGTYPE_LCLBL)
            || command == CMD_GTOL || command == CMD_XEQL)
        for (i = 0; i < 4; i++)
            buf[bufptr++] = 255;
    switch (arg->type) {
        case ARGTYPE_NUM:
        case ARGTYPE_NEG_NUM:
        case ARGTYPE_IND_NUM: {
            int4 num = arg->val.num;
            char tmpbuf[5];
            int tmplen = 0;
            while (num > 127) {
                tmpbuf[tmplen++] = num & 127;
                num >>= 7;
            }
            tmpbuf[tmplen++] = num;
            tmpbuf[0] |= 128;
            while (--tmplen >= 0)
                buf[bufptr++] = tmpbuf[tmplen];
            break;
        }
        case ARGTYPE_STK:
        case ARGTYPE_IND_STK:
            buf[bufptr++] = arg->val.stk;
            break;
        case ARGTYPE_STR:
        case ARGTYPE_IND_STR: {
            buf[bufptr++] = (unsigned char) arg->length;
            for (i = 0; i < arg->length; i++)
                buf[bufptr++] = arg->val.text[i];
            break;
        }
        case ARGTYPE_LCLBL:
            buf[bufptr++] = arg->val.lclbl;
            break;
        case ARGTYPE_DOUBLE: {
            unsigned char *b = (unsigned char *) &arg->val_d;
            for (int i = 0; i < (int) sizeof(phloat); i++)
                buf[bufptr++] = *b++;
            break;
        }
        case ARGTYPE_XSTR: {
            xstr_len = arg->length;
            if (xstr_len > 65535)
                xstr_len = 65535;
            buf[bufptr++] = xstr_len;
            buf[bufptr++] = xstr_len >> 8;
            // Not storing the text in 'buf' because it may not fit;
            // we'll handle that separately when copying the buffer
            // into the program.
            bufptr += xstr_len;
            break;
        }
    }

    if (command == CMD_NUMBER && num_str != NULL) {
        const char *p = num_str;
        char c;
        const char wrong_dot = flags.f.decimal_point ? ',' : '.';
        const char right_dot = flags.f.decimal_point ? '.' : ',';
        while ((c = *p++) != 0) {
            if (c == wrong_dot)
                c = right_dot;
            else if (c == 'E' || c == 'e')
                c = 24;
            buf[bufptr++] = c;
        }
        buf[bufptr++] = 0;
    }

    if (bufptr + prgm->size > prgm->capacity) {
        unsigned char *newtext;
        prgm->capacity += bufptr + 512;
        newtext = (unsigned char *) malloc(prgm->capacity);
        // TODO - handle memory allocation failure
        for (pos = 0; pos < pc; pos++)
            newtext[pos] = prgm->text[pos];
        for (pos = pc; pos < prgm->size; pos++)
            newtext[pos + bufptr] = prgm->text[pos];
        if (prgm->text != NULL)
            free(prgm->text);
        prgm->text = newtext;
    } else {
        for (pos = prgm->size - 1; pos >= pc; pos--)
            prgm->text[pos + bufptr] = prgm->text[pos];
    }
    if (arg->type == ARGTYPE_XSTR) {
        int instr_len = bufptr - xstr_len;
        memcpy(prgm->text + pc, buf, instr_len);
        memcpy(prgm->text + pc + instr_len, arg->val.xstr, xstr_len);
    } else {
        memcpy(prgm->text + pc, buf, bufptr);
    }
    if (command == CMD_EMBED && !loading_state)
        eq_dir->prgms[arg->val.num].eq_data->refcount++;
    prgm->size += bufptr;
    if (command != CMD_END && flags.f.printer_exists && (flags.f.trace_print || flags.f.normal_print))
        print_program_line(current_prgm, pc);

    if (dir != eq_dir) {
        /* No labels in generated code, and also, all those
         * gaps in the prgms array, where we make sure the unused
         * entries have text == NULL, but don't really care about
         * the other prgm_struct members... rebuild_label_table()
         * does not react well to those.
         */
        if (command == CMD_END ||
                (command == CMD_LBL && arg->type == ARGTYPE_STR))
            rebuild_label_table();
        else
            update_label_table(current_prgm, pc, bufptr);
    }

    if (!loading_state) {
        invalidate_lclbls(current_prgm, false);
        clear_all_rtns();
        draw_varmenu();
    }
    return true;
}

void store_command_after(int4 *pc, int command, arg_struct *arg, const char *num_str) {
    int4 oldpc = *pc;
    directory *dir = dir_list[current_prgm.dir];
    if (*pc == -1)
        *pc = 0;
    else if (!dir->prgms[current_prgm.idx].is_end(*pc))
        *pc += get_command_length(current_prgm, *pc);
    if (!store_command(*pc, command, arg, num_str))
        *pc = oldpc;
}

static bool ensure_prgm_space(int n) {
    directory *dir = dir_list[current_prgm.dir];
    prgm_struct *prgm = dir->prgms + current_prgm.idx;
    if (prgm->size + n <= prgm->capacity)
        return true;
    int4 newcapacity = prgm->size + n;
    unsigned char *newtext = (unsigned char *) realloc(prgm->text, newcapacity);
    if (newtext == NULL)
        return false;
    prgm->text = newtext;
    prgm->capacity = newcapacity;
    return true;
}

int x2line() {
    if (!current_prgm.is_editable())
        return ERR_RESTRICTED_OPERATION;
    if (current_prgm.is_locked())
        return ERR_PROGRAM_LOCKED;
    switch (stack[sp]->type) {
        case TYPE_REAL: {
            if (!ensure_prgm_space(2 + sizeof(phloat)))
                return ERR_INSUFFICIENT_MEMORY;
            vartype_real *r = (vartype_real *) stack[sp];
            arg_struct arg;
            arg.type = ARGTYPE_DOUBLE;
            arg.val_d = r->x;
            store_command_after(&pc, CMD_NUMBER, &arg, NULL);
            return ERR_NONE;
        }
        case TYPE_COMPLEX: {
            if (!ensure_prgm_space(6 + 2 * sizeof(phloat)))
                return ERR_INSUFFICIENT_MEMORY;
            vartype_complex *c = (vartype_complex *) stack[sp];
            arg_struct arg;
            arg.type = ARGTYPE_DOUBLE;
            arg.val_d = c->re;
            store_command_after(&pc, CMD_NUMBER, &arg, NULL);
            arg.type = ARGTYPE_DOUBLE;
            arg.val_d = c->im;
            store_command_after(&pc, CMD_NUMBER, &arg, NULL);
            arg.type = ARGTYPE_NONE;
            store_command_after(&pc, CMD_RCOMPLX, &arg, NULL);
            return ERR_NONE;
        }
        case TYPE_STRING: {
            vartype_string *s = (vartype_string *) stack[sp];
            int len = s->length;
            if (len > 65535)
                len = 65535;
            if (!ensure_prgm_space(4 + len))
                return ERR_INSUFFICIENT_MEMORY;
            arg_struct arg;
            arg.type = ARGTYPE_XSTR;
            arg.length = len;
            arg.val.xstr = s->txt();
            store_command_after(&pc, CMD_XSTR, &arg, NULL);
            return ERR_NONE;
        }
        case TYPE_EQUATION: {
            if (!ensure_prgm_space(7))
                return ERR_INSUFFICIENT_MEMORY;
            vartype_equation *eq = (vartype_equation *) stack[sp];
            arg_struct arg;
            arg.type = ARGTYPE_NUM;
            arg.val.num = eq->data->eqn_index;
            store_command_after(&pc, CMD_EMBED, &arg, NULL);
            return ERR_NONE;
        }
        case TYPE_UNIT: {
            vartype_unit *u = (vartype_unit *) stack[sp];
            int len = u->length;
            if (len > 65535)
                len = 65535;
            if (!ensure_prgm_space(6 + sizeof(phloat) + len))
                return ERR_INSUFFICIENT_MEMORY;
            char *ub = (char *) malloc(u->length + 2);
            if (ub == NULL)
                return ERR_INSUFFICIENT_MEMORY;
            ub[0] = '_';
            memcpy(ub + 1, u->text, u->length);
            ub[u->length + 1] = 0;
            arg_struct arg;
            arg.type = ARGTYPE_DOUBLE;
            arg.val_d = u->x;
            store_command_after(&pc, CMD_NUMBER, &arg, ub);
            free(ub);
            return ERR_NONE;
        }
        default:
            return ERR_INTERNAL_ERROR;
    }
}

int a2line(bool append) {
    if (!current_prgm.is_editable())
        return ERR_RESTRICTED_OPERATION;
    if (current_prgm.is_locked())
        return ERR_PROGRAM_LOCKED;
    if (reg_alpha_length == 0) {
        squeak();
        return ERR_NONE;
    }
    if (!ensure_prgm_space(reg_alpha_length + ((reg_alpha_length - 2) / 14 + 1) * 3))
        return ERR_INSUFFICIENT_MEMORY;
    const char *p = reg_alpha;
    int len = reg_alpha_length;
    int maxlen = 15;

    arg_struct arg;
    if (append) {
        maxlen = 14;
    } else if (p[0] == 0x7f || (p[0] & 128) != 0) {
        arg.type = ARGTYPE_NONE;
        store_command_after(&pc, CMD_CLA, &arg, NULL);
        maxlen = 14;
    }

    while (len > 0) {
        int len2 = len;
        if (len2 > maxlen)
            len2 = maxlen;
        arg.type = ARGTYPE_STR;
        if (maxlen == 15) {
            arg.length = len2;
            memcpy(arg.val.text, p, len2);
        } else {
            arg.length = len2 + 1;
            arg.val.text[0] = 127;
            memcpy(arg.val.text + 1, p, len2);
        }
        store_command_after(&pc, CMD_STRING, &arg, NULL);
        p += len2;
        len -= len2;
        maxlen = 14;
    }
    return ERR_NONE;
}

int prgm_lock(bool lock) {
    if (!flags.f.prgm_mode || current_prgm.dir != cwd->id)
        return ERR_RESTRICTED_OPERATION;
    dir_list[current_prgm.dir]->prgms[current_prgm.idx].locked = lock;
    return ERR_NONE;
}

static int pc_line_convert(int4 loc, int loc_is_pc) {
    int4 pc = 0;
    int4 line = 1;
    prgm_struct *prgm = dir_list[current_prgm.dir]->prgms + current_prgm.idx;

    while (1) {
        if (loc_is_pc) {
            if (pc >= loc)
                return line;
        } else {
            if (line >= loc)
                return pc;
        }
        if (prgm->is_end(pc))
            return loc_is_pc ? line : pc;
        pc += get_command_length(current_prgm, pc);
        line++;
    }
}

int4 pc2line(int4 pc) {
    if (pc == -1)
        return 0;
    else
        return pc_line_convert(pc, 1);
}

int4 line2pc(int4 line) {
    if (line == 0)
        return -1;
    else
        return pc_line_convert(line, 0);
}

int4 global_pc2line(pgm_index prgm, int4 pc) {
    if (prgm.idx < 0)
        return pc;
    pgm_index saved_prgm = current_prgm;
    current_prgm = prgm;
    int4 res = pc2line(pc);
    current_prgm = saved_prgm;
    return res;
}

int4 global_line2pc(pgm_index prgm, int4 line) {
    if (prgm.idx < 0)
        return line;
    pgm_index saved_prgm = current_prgm;
    current_prgm = prgm;
    int4 res = line2pc(line);
    current_prgm = saved_prgm;
    return res;
}

int4 find_local_label(const arg_struct *arg) {
    int4 orig_pc = pc;
    int4 search_pc;
    int wrapped = 0;
    directory *dir = dir_list[current_prgm.dir];
    prgm_struct *prgm = dir->prgms + current_prgm.idx;

    if (orig_pc == -1)
        orig_pc = 0;
    search_pc = orig_pc;

    while (!wrapped || search_pc < orig_pc) {
        int command, argtype;
        if (search_pc >= prgm->size - 2) {
            if (orig_pc == 0)
                break;
            search_pc = 0;
            wrapped = 1;
        }
        command = prgm->text[search_pc];
        argtype = prgm->text[search_pc + 1];
        command |= (argtype & 112) << 4;
        argtype &= 15;
        if (command == CMD_LBL && (argtype == arg->type
                                || argtype == ARGTYPE_STK)) {
            if (argtype == ARGTYPE_NUM) {
                int num = 0;
                unsigned char c;
                int pos = search_pc + 2;
                do {
                    c = prgm->text[pos++];
                    num = (num << 7) | (c & 127);
                } while ((c & 128) == 0);
                if (num == arg->val.num)
                    return search_pc;
            } else if (argtype == ARGTYPE_STK) {
                // Synthetic LBL ST T etc.
                // Allow GTO ST T and GTO 112
                char stk = prgm->text[search_pc + 2];
                if (arg->type == ARGTYPE_STK) {
                    if (stk = arg->val.stk)
                        return search_pc;
                } else if (arg->type == ARGTYPE_NUM) {
                    int num = 0;
                    switch (stk) {
                        case 'T': num = 112; break;
                        case 'Z': num = 113; break;
                        case 'Y': num = 114; break;
                        case 'X': num = 115; break;
                        case 'L': num = 116; break;
                    }
                    if (num == arg->val.num)
                        return search_pc;
                }
            } else {
                char lclbl = prgm->text[search_pc + 2];
                if (lclbl == arg->val.lclbl)
                    return search_pc;
            }
        }
        search_pc += get_command_length(current_prgm, search_pc);
    }

    return -2;
}

bool find_global_label(const arg_struct *arg, pgm_index *prgm, int4 *pc, int *idx) {
    const char *name = arg->val.text;
    int namelen = arg->length;

    /* Always start by searching the current directory and its ancestors,
     * followed by PATH. The rationale is that programs, running from a certain
     * directory, should find the same labels that a user would find, when
     * doing interactive GTO from the same directory.
     */
    directory *dir = cwd;
    do {
        for (int i = dir->labels_count - 1; i >= 0; i--) {
            if (string_equals(dir->labels[i].name, dir->labels[i].length, name, namelen)) {
                prgm->set(dir->id, dir->labels[i].prgm);
                *pc = dir->labels[i].pc;
                if (idx != NULL)
                    *idx = i;
                return true;
            }
        }
        dir = dir->parent;
    } while (dir != NULL);

    vartype_list *path = get_path();
    if (path != NULL)
        for (int i = 0; i < path->size; i++) {
            vartype *v = path->array->data[i];
            if (v->type != TYPE_DIR_REF)
                continue;
            dir = get_dir(((vartype_dir_ref *) v)->dir);
            if (dir == NULL)
                continue;
            for (int j = dir->labels_count - 1; j >= 0; j--) {
                if (string_equals(dir->labels[j].name, dir->labels[j].length, name, namelen)) {
                    prgm->set(dir->id, dir->labels[j].prgm);
                    *pc = dir->labels[j].pc;
                    if (idx != NULL)
                        *idx = j;
                    return true;
                }
            }
        }

    /* If still not found, but we're a running program, search the directory
     * containing the program. If the current stack frame belongs to an
     * equation (current_prgm.dir == eq_dir->id) or to SOLVE, INTEG, or
     * PLOT (current_prgm.idx < 0), search up the RTN stack until the
     * first stack frame belonging to regular user code.
     */
    if (program_running()) {
        if (current_prgm.idx < 0 || current_prgm.dir == eq_dir->id) {
            dir = NULL;
            for (int lvl = rtn_level - 1; lvl >= 0; lvl--) {
                rtn_stack_entry *rse = rtn_stack + lvl;
                if (!rse->is_special() && rse->dir != eq_dir->id) {
                    dir = get_dir(rse->dir);
                    if (dir == NULL)
                        // Should never happen
                        return false;
                    break;
                }
            }
            if (dir == NULL)
                return false;
        } else
            dir = get_dir(current_prgm.dir);
        while (dir != NULL) {
            for (int j = dir->labels_count - 1; j >= 0; j--) {
                if (string_equals(dir->labels[j].name, dir->labels[j].length, name, namelen)) {
                    prgm->set(dir->id, dir->labels[j].prgm);
                    *pc = dir->labels[j].pc;
                    if (idx != NULL)
                        *idx = j;
                    return true;
                }
            }
            dir = dir->parent;
        }
    }

    return false;
}

int push_rtn_addr(pgm_index prgm, int4 pc) {
    if (rtn_level == MAX_RTN_LEVEL)
        return ERR_RTN_STACK_FULL;
    if (rtn_level == rtn_stack_capacity) {
        int new_rtn_stack_capacity = rtn_stack_capacity + 16;
        rtn_stack_entry *new_rtn_stack = (rtn_stack_entry *) realloc(rtn_stack, new_rtn_stack_capacity * sizeof(rtn_stack_entry));
        if (new_rtn_stack == NULL)
            return ERR_INSUFFICIENT_MEMORY;
        rtn_stack_capacity = new_rtn_stack_capacity;
        rtn_stack = new_rtn_stack;
    }
    rtn_stack[rtn_level].dir = prgm.dir;
    rtn_stack[rtn_level].set_prgm(prgm.idx);
    rtn_stack[rtn_level].pc = pc;
    rtn_level++;
    if (prgm.idx == -2)
        rtn_solve_active = true;
    else if (prgm.idx == -3)
        rtn_integ_active = true;
    else if (prgm.idx == -5)
        rtn_plot_active = true;
    return ERR_NONE;
}

int push_indexed_matrix() {
    if (rtn_level == 0 ? rtn_level_0_has_matrix_entry : rtn_stack[rtn_level - 1].has_matrix())
        return ERR_NONE;
    vartype_list *list = (vartype_list *) new_list(4 + matedit_stack_depth);
    if (list == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    list->array->data[0] = new_string(matedit_name, matedit_length);
    list->array->data[1] = new_real(matedit_dir);
    list->array->data[2] = new_real(matedit_i);
    list->array->data[3] = new_real(matedit_is_list ? -1 : matedit_j);
    for (int i = 0; i < matedit_stack_depth; i++)
        list->array->data[4 + i] = new_real(matedit_stack[i].as_phloat());
    for (int i = 0; i < 4 + matedit_stack_depth; i++)
        if (list->array->data[i] == NULL) {
            free_vartype((vartype *) list);
            return ERR_INSUFFICIENT_MEMORY;
        }
    store_private_var("MAT", 3, (vartype *) list);
    if (rtn_level == 0)
        rtn_level_0_has_matrix_entry = true;
    else
        rtn_stack[rtn_level - 1].set_has_matrix(true);
    matedit_mode = 0;
    free(matedit_stack);
    matedit_stack = NULL;
    matedit_stack_depth = 0;
    return ERR_NONE;
}

void maybe_pop_indexed_matrix(const char *name, int len) {
    if (rtn_level == 0 ? !rtn_level_0_has_matrix_entry : !rtn_stack[rtn_level - 1].has_matrix())
        return;
    if (!string_equals(matedit_name, matedit_length, name, len))
        return;
    vartype_list *list = (vartype_list *) recall_and_purge_private_var("MAT", 3);
    if (list == NULL)
        return;
    // Ignoring older MAT lists. Those have only 3 elements, and do
    // not include the directory id, and we can't reliably reconstruct
    // what the indexed variable's directory would have been.
    if (list->size >= 4) {
        int newdepth = list->size - 4;
        matedit_stack_entry *newstack = newdepth == 0 ? NULL : (matedit_stack_entry *) malloc(newdepth * sizeof(matedit_stack_entry));
        // TODO Handle memory allocation failure
        vartype_string *s = (vartype_string *) list->array->data[0];
        string_copy(matedit_name, &matedit_length, s->txt(), s->length);
        matedit_dir = to_int4(((vartype_real *) list->array->data[1])->x);
        matedit_i = to_int4(((vartype_real *) list->array->data[2])->x);
        matedit_j = to_int4(((vartype_real *) list->array->data[3])->x);
        matedit_is_list = matedit_j == -1;
        if (matedit_is_list)
            matedit_j = 0;
        matedit_stack_depth = newdepth;
        free(matedit_stack);
        matedit_stack = newstack;
        for (int i = 0; i < newdepth; i++)
            matedit_stack[i].set(((vartype_real *) list->array->data[i + 4])->x);
        matedit_mode = 1;
    } else {
        free(matedit_stack);
        matedit_stack = NULL;
        matedit_stack_depth = 0;
        matedit_mode = 0;
    }
    free_vartype((vartype *) list);
    if (rtn_level == 0)
        rtn_level_0_has_matrix_entry = false;
    else
        rtn_stack[rtn_level - 1].set_has_matrix(false);
}

/* Layout of STK list used to save state for FUNC and LNSTK/L4STK:
 * 0: Mode: [0-4][0-4] for FUNC, or -1 for stand-alone LNSTK/L4STK;
 * 1: State: "ABCDE<Text>" where A=Caller Big Stack, B=LNSTK/L4STK after FUNC,
 *    C=CSLD, D=F25, E=ERRNO, <Text>=ERRMSG
 *    For stand-alone LNSTK/L4STK, only A is present.
 * 2: Saved stack. For stand-alone LNSTK/L4STK that didn't have to
 *    change the actual stack mode, this is empty; for stand-alone
 *    LNSTK, this is also empty; for all other cases, this is the entire stack.
 * 3: Saved LASTX (FUNC only)
 */

int push_func_state(int n) {
    if (!program_running())
        return ERR_RESTRICTED_OPERATION;
    int inputs = n / 10;
    if (sp + 1 < inputs)
        return ERR_TOO_FEW_ARGUMENTS;

    vartype *stk = recall_private_var("STK", 3);
    if (stk != NULL)
        return ERR_INVALID_CONTEXT;
    if (!ensure_var_space(1))
        return ERR_INSUFFICIENT_MEMORY;

    /* Create the STK list */
    stk = new_list(4);
    if (stk == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    vartype_list *slist = (vartype_list *) stk;
    slist->array->data[0] = new_real(n);
    slist->array->data[1] = new_string(NULL, lasterr == -1 ? 5 + lasterr_length : 5);
    int i;
    for (i = 0; i < 2; i++)
        if (slist->array->data[i] == NULL) {
            nomem1:
            free_vartype(stk);
            return ERR_INSUFFICIENT_MEMORY;
        }

    /* Create the new stack
     * Note that we're creating a list here, while the actual stack is just
     * a plain array of (vartype *). We'll swap the array in this list with
     * the one for the RPN stack once all allocations have been done, and then
     * use this list to store the old stack in the STK list.
     */
    int newdepth = flags.f.big_stack ? inputs : 4;
    // Allocating size 4 because the stack must always have capacity >= 4
    vartype_list *tlist = (vartype_list *) new_list(4);
    tlist->size = newdepth;
    if (tlist == NULL)
        goto nomem1;
    for (i = 0; i < newdepth; i++) {
        tlist->array->data[newdepth - 1 - i] = i < inputs ? dup_vartype(stack[sp - i]) : new_real(0);
        if (tlist->array->data[newdepth - 1 - i] == NULL) {
            nomem2:
            free_vartype((vartype *) tlist);
            goto nomem1;
        }
    }

    vartype *newlastx = new_real(0);
    if (newlastx == NULL)
        goto nomem2;

    /* OK, we have everything we need. Now move it all into place... */
    vartype_string *s = (vartype_string *) slist->array->data[1];
    s->txt()[0] = flags.f.big_stack ? '1' : '0';
    s->txt()[1] = '0';
    s->txt()[2] = sp != -1 && is_csld() ? '1' : '0';
    s->txt()[3] = flags.f.error_ignore ? '1' : '0';
    s->txt()[4] = (char) lasterr;
    if (lasterr == -1)
        memcpy(s->txt() + 5, lasterr_text, lasterr_length);
    vartype **tmpstk = tlist->array->data;
    int4 tmpdepth = tlist->size;
    tlist->array->data = stack;
    tlist->size = sp + 1;
    stack = tmpstk;
    stack_capacity = 4;
    sp = tmpdepth - 1;
    slist->array->data[2] = (vartype *) tlist;
    slist->array->data[3] = lastx;
    lastx = newlastx;

    store_private_var("STK", 3, stk);
    flags.f.error_ignore = 0;
    lasterr = ERR_NONE;

    if (rtn_level == 0)
        rtn_level_0_has_func_state = true;
    else
        rtn_stack[rtn_level - 1].set_has_func(true);
    return ERR_NONE;
}

int push_stack_state(bool big) {
    vartype *stk = recall_private_var("STK", 3);
    if (stk != NULL) {
        /* LNSTK/L4STK after FUNC */
        vartype_list *slist = (vartype_list *) stk;
        vartype_string *s = (vartype_string *) slist->array->data[1];
        if (s->length == 1 || s->txt()[1] != '0')
            /* LNSTK/L4STK after LNSTK/L4STK: not allowed */
            return ERR_INVALID_CONTEXT;
        if ((bool) flags.f.big_stack == big) {
            /* Nothing to do */
        } else if (big) {
            /* Assuming we're being called right after FUNC, so
             * the stack contains the parameters, padded to depth 4
             * with zeros. We just remove the padding now.
             */
            vartype_real *mode = (vartype_real *) slist->array->data[0];
            int inputs = to_int(mode->x) / 10;
            int excess = 4 - inputs;
            if (excess > 0) {
                for (int i = 0; i < excess; i++)
                    free_vartype(stack[i]);
                memmove(stack, stack + excess, inputs * sizeof(vartype *));
                sp = inputs - 1;
            }
            flags.f.big_stack = true;
        } else {
            /* Just a plain switch to 4STK using truncation to 4 levels.
             * FUNC has already taken care of saving the entire stack,
             * so there's no need to do that here.
             */
            int err = docmd_4stk(NULL);
            if (err != ERR_NONE)
                return err;
        }
        s->txt()[1] = '1';
        return ERR_NONE;
    } else {
        /* Stand-alone LNSTK/L4STK */

        bool save_stk = flags.f.big_stack && !big;

        /* Create the STK list */
        stk = new_list(3);
        if (stk == NULL)
            return ERR_INSUFFICIENT_MEMORY;
        vartype_list *slist = (vartype_list *) stk;
        slist->array->data[0] = new_real(-1);
        slist->array->data[1] = new_string(flags.f.big_stack ? "1" : "0", 1);
        slist->array->data[2] = NULL;
        int i;
        for (i = 0; i < 2; i++)
            if (slist->array->data[i] == NULL) {
                nomem:
                free_vartype(stk);
                return ERR_INSUFFICIENT_MEMORY;
            }

        /* When switching from NSTK to 4STK, save the stack */
        if (save_stk) {
            vartype_list *tlist = (vartype_list *) new_list(4);
            if (tlist == NULL)
                goto nomem;
            for (i = 0; i < 4; i++) {
                tlist->array->data[3 - i] = i <= sp ? dup_vartype(stack[sp - i]) : new_real(0);
                if (tlist->array->data[3 - i] == NULL) {
                    free_vartype((vartype *) tlist);
                    goto nomem;
                }
            }
            vartype **tmpstk = tlist->array->data;
            int4 tmpdepth = tlist->size;
            tlist->array->data = stack;
            tlist->size = sp + 1;
            stack = tmpstk;
            stack_capacity = tmpdepth;
            sp = tmpdepth - 1;
            slist->array->data[2] = (vartype *) tlist;
        }

        store_private_var("STK", 3, stk);
        flags.f.big_stack = big;

        if (rtn_level == 0)
            rtn_level_0_has_func_state = true;
        else
            rtn_stack[rtn_level - 1].set_has_func(true);
        return ERR_NONE;
    }
}

/* FUNC and L4STK save the stack in a list, but this has one undesirable
 * side effect: if the state is saved and restored, the array inside the
 * list will be created without any spare room. This is bad when restoring
 * the stack, because there is the assumption that the stack array will
 * always have capacity >= 4. This function makes sure the array has that
 * capacity.
 */
static bool ensure_list_capacity_4(vartype_list *list) {
    int4 size = list->size;
    if (size < 4) {
        vartype **newdata = (vartype **) realloc(list->array->data, 4 * sizeof(vartype *));
        if (newdata == NULL)
            return false;
        list->array->data = newdata;
    }
    return true;
}

int pop_func_state(bool error) {
    if (rtn_level == 0) {
        if (!rtn_level_0_has_func_state)
            return ERR_NONE;
    } else {
        if (!rtn_stack[rtn_level - 1].has_func())
            return ERR_NONE;
    }

    vartype_list *stk = (vartype_list *) recall_private_var("STK", 3);
    if (stk == NULL)
        // Older FD/ST-based FUNC logic, pre-47. Too difficult to deal with, so
        // we punt. Note that this can only happen if a user upgrades from <47
        // to >=47 while execution is stopped with old FUNC data on the stack.
        // That seems like a reasonable scenario to ignore.
        return ERR_INVALID_DATA;

    vartype **stk_data = stk->array->data;
    int n = to_int(((vartype_real *) stk_data[0])->x);
    vartype_string *state = (vartype_string *) stk_data[1];
    bool big = state->txt()[0] == '1';

    int err = ERR_NONE;
    if (false) {
        error:
        free_vartype(lastx);
        lastx = stk_data[3];
        stk_data[3] = NULL;
        goto done;
    }

    if (n == -1) {
        // Stand-alone LNSTK/L4STK
        if (big && !flags.f.big_stack && stk_data[2] != NULL) {
            // Extend the stack back to its original size, restoring its
            // original contents, but only those above level 4.
            vartype_list *tlist = (vartype_list *) stk_data[2];
            if (!ensure_list_capacity_4(tlist))
                return ERR_INSUFFICIENT_MEMORY;
            while (tlist->size < 4)
                tlist->array->data[tlist->size++] = NULL;
            for (int i = 0; i < 4; i++) {
                free_vartype(tlist->array->data[tlist->size - 1 - i]);
                tlist->array->data[tlist->size - 1 - i] = stack[sp - i];
                stack[sp - i] = NULL;
            }
            vartype **tmpstk = stack;
            int tmpsize = sp + 1;
            stack = tlist->array->data;
            stack_capacity = tlist->size;
            sp = stack_capacity - 1;
            tlist->array->data = tmpstk;
            tlist->size = tmpsize;
        } else if (!big && flags.f.big_stack) {
            if (sp < 3) {
                int extra = 3 - sp;
                vartype *zeros[4];
                bool nomem = false;
                for (int i = 0; i < extra; i++) {
                    zeros[i] = new_real(0);
                    if (zeros[i] == NULL)
                        nomem = true;
                }
                if (nomem || !ensure_stack_capacity(extra)) {
                    for (int i = 0; i < extra; i++)
                        free_vartype(zeros[i]);
                    big = true; // because the switch back to 4STK has failed
                    err = ERR_INSUFFICIENT_MEMORY;
                } else {
                    memmove(stack + extra, stack, (sp + 1) * sizeof(vartype *));
                    for (int i = 0; i < extra; i++)
                        stack[i] = zeros[i];
                    sp = 3;
                }
            } else if (sp > 3) {
                int excess = sp - 3;
                for (int i = 0; i < excess; i++)
                    free_vartype(stack[i]);
                memmove(stack, stack + excess, 4 * sizeof(vartype *));
                sp = 3;
            }
        }
    } else {
        // FUNC, with or without LNSTK/L4STK
        vartype_list *tlist = (vartype_list *) stk_data[2];
        if (!ensure_list_capacity_4(tlist)) {
            err = ERR_INSUFFICIENT_MEMORY;
            goto error;
        }

        vartype **tmpstk = stack;
        int tmpsize = sp + 1;
        stack = tlist->array->data;
        stack_capacity = tlist->size;
        sp = stack_capacity - 1;
        if (stack_capacity < 4)
            stack_capacity = 4;
        tlist->array->data = tmpstk;
        tlist->size = tmpsize;

        if (error)
            goto error;

        int inputs = n / 10;
        int outputs = n % 10;
        if (tmpsize < outputs) {
            // One could make the case that this should be an error.
            // I chose to be lenient and pad the result set with zeros instead.
            int deficit = outputs - tmpsize;
            memmove(tmpstk + deficit, tmpstk, tmpsize * sizeof(vartype *));
            bool nomem = false;
            for (int i = 0; i < deficit; i++) {
                vartype *zero = new_real(0);
                tmpstk[i] = zero;
                if (zero == NULL)
                    nomem = true;
            }
            tmpsize += deficit;
            tlist->size = tmpsize;
            if (nomem) {
                err = ERR_INSUFFICIENT_MEMORY;
                goto error;
            }
        }

        bool do_lastx = inputs > 0;
        if (n == 1 && state->txt()[2] == '1')
            // RCL-like function with stack lift disabled
            inputs = 1;
        int growth = outputs - inputs;
        if (big) {
            flags.f.big_stack = true;
            if (!ensure_stack_capacity(growth)) {
                err = ERR_INSUFFICIENT_MEMORY;
                goto error;
            }
            free_vartype(lastx);
            if (do_lastx) {
                lastx = stack[sp];
                stack[sp] = NULL;
            } else {
                lastx = stk_data[3];
                stk_data[3] = NULL;
            }
            for (int i = 0; i < inputs; i++) {
                free_vartype(stack[sp - i]);
                stack[sp - i] = NULL;
            }
            sp -= inputs;
            sp += outputs;
            for (int i = 0; i < outputs; i++) {
                stack[sp - i] = tmpstk[tmpsize - i - 1];
                tmpstk[tmpsize - i - 1] = NULL;
            }
        } else {
            vartype *tdups[4];
            int n_tdups = -growth;
            for (int i = 0; i < n_tdups; i++) {
                tdups[i] = dup_vartype(stack[0]);
                if (tdups[i] == NULL) {
                    for (int j = 0; j < i; j++)
                        free_vartype(tdups[j]);
                    err = ERR_INSUFFICIENT_MEMORY;
                    goto error;
                }
            }
            free_vartype(lastx);
            if (do_lastx) {
                lastx = stack[sp];
                stack[sp] = NULL;
            } else {
                lastx = stk_data[3];
                stk_data[3] = NULL;
            }
            for (int i = 0; i < inputs; i++) {
                free_vartype(stack[sp - i]);
                stack[sp - i] = NULL;
            }
            if (growth > 0) {
                for (int i = 0; i < growth; i++)
                    free_vartype(stack[i]);
                memmove(stack, stack + growth, (4 - outputs) * sizeof(vartype *));
            } else if (growth < 0) {
                int shrinkage = -growth;
                memmove(stack + shrinkage, stack, (4 - inputs) * sizeof(vartype *));
                for (int i = 0; i < shrinkage; i++)
                    stack[i] = tdups[i];
            }
            for (int i = 0; i < outputs; i++) {
                stack[sp - i] = tmpstk[tmpsize - i - 1];
                tmpstk[tmpsize - i - 1] = NULL;
            }
        }

        flags.f.error_ignore = state->txt()[3] == '1';
        lasterr = (signed char) state->txt()[4];
        if (lasterr == -1) {
            lasterr_length = state->length - 5;
            memcpy(lasterr_text, state->txt() + 5, lasterr_length);
        }
    }

    done:
    if (rtn_level == 0)
        rtn_level_0_has_func_state = false;
    else
        rtn_stack[rtn_level - 1].set_has_func(false);

    flags.f.big_stack = big;
    print_trace();
    return err;
}

int get_saved_stack_level(int level, vartype **res) {
    vartype_list *stk = (vartype_list *) recall_private_var("STK", 3, true);
    if (stk == NULL || ((vartype_real *) stk->array->data[0])->x == -1)
        return ERR_INVALID_CONTEXT;
    vartype *v;
    if (level == 0)
        v = stk->array->data[3];
    else {
        vartype_list *sstack = (vartype_list *) stk->array->data[2];
        if (level > sstack->size)
            return ERR_STACK_DEPTH_ERROR;
        v = sstack->array->data[sstack->size - level];
    }
    v = dup_vartype(v);
    if (v == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    *res = v;
    return ERR_NONE;
}

void step_out() {
    if (rtn_level > 0)
        rtn_stop_level = rtn_level - 1;
}

void step_over() {
    if (rtn_level >= 0)
        rtn_stop_level = rtn_level;
}

void return_here_after_last_rtn() {
    if (current_prgm.dir != eq_dir->id && current_prgm.idx >= 0) {
        rtn_after_last_rtn_dir = current_prgm.dir;
        rtn_after_last_rtn_prgm = current_prgm.idx;
        rtn_after_last_rtn_pc = pc;
    } else {
        rtn_after_last_rtn_dir = -1;
        rtn_after_last_rtn_prgm = -1;
        rtn_after_last_rtn_pc = -1;
    }
}

void equation_deleted(int eqn_index) {
    if (current_prgm.dir == eq_dir->id && current_prgm.idx == eqn_index) {
        current_prgm.set(cwd->id, cwd->prgms_count - 1);
        pc = cwd->prgms[cwd->prgms_count - 1].size - 2;
    }
    if (rtn_after_last_rtn_dir == eq_dir->id && rtn_after_last_rtn_prgm == eqn_index) {
        rtn_after_last_rtn_dir = cwd->id;
        rtn_after_last_rtn_prgm = cwd->prgms_count - 1;
        rtn_after_last_rtn_pc = cwd->prgms[cwd->prgms_count - 1].size - 2;
    }
    math_equation_deleted(eqn_index);
}

bool unwind_after_eqn_error() {
    int4 saved_dir = rtn_after_last_rtn_dir;
    int4 saved_prgm = rtn_after_last_rtn_prgm;
    int4 saved_pc = rtn_after_last_rtn_pc;
    while (true) {
        int err = pop_func_state(true);
        if (err != ERR_NONE) {
            // Fall back on just dropping stack contents
            clear_all_rtns();
            goto done;
        }
        pgm_index prgm;
        int4 dummy1;
        bool dummy2;
        pop_rtn_addr(&prgm, &dummy1, &dummy2);
        if (prgm.idx == -1)
            break;
        if (prgm.idx == -4)
            return true;
    }
    if (mode_plainmenu == MENU_PROGRAMMABLE)
        set_menu(MENULEVEL_PLAIN, MENU_NONE);
    if (varmenu_role == 3)
        varmenu_role = 0;
    done:
    if (saved_prgm != -1) {
        current_prgm.set(saved_dir, saved_prgm);
        pc = saved_pc;
    }
    return false;
}

bool should_i_stop_at_this_level() {
    bool stop = rtn_stop_level >= rtn_level;
    if (stop)
        rtn_stop_level = -1;
    return stop;
}

static void remove_locals() {
    if (matedit_mode == 3 && matedit_dir <= 0 && -matedit_dir >= rtn_level)
        leave_matrix_editor();
    int old_count = local_vars_count;
    for (int i = local_vars_count - 1; i >= 0; i--) {
        if (local_vars[i].level < rtn_level)
            break;
        free_vartype(local_vars[i].value);
        local_vars_count--;
    }
    if (local_vars_count != old_count)
        update_catalog();
}

int rtn(int err) {
    // NOTE: 'err' should be one of ERR_NONE, ERR_YES, or ERR_NO.
    // For any actual *error*, i.e. anything that should actually
    // stop program execution, use rtn_with_error() instead.
    pgm_index newprgm;
    int4 newpc;
    bool stop;
    pop_rtn_addr(&newprgm, &newpc, &stop);
    if (newprgm.idx < 0) {
        switch (newprgm.idx) {
            case -1:
                if (pc >= dir_list[current_prgm.dir]->prgms[current_prgm.idx].size)
                    /* It's an END; go to line 0 */
                    pc = -1;
                if (err != ERR_NONE)
                    display_error(err);
                return ERR_STOP;
            case -2: return return_to_solve(false, stop);
            case -3: return return_to_integ(stop);
            case -4: return return_to_eqn_edit(ERR_NONE);
            case -5: return return_to_plot(false, stop);
            default: return ERR_INTERNAL_ERROR;
        }
    } else {
        current_prgm = newprgm;
        pc = newpc;
        if (err == ERR_NO) {
            int command;
            arg_struct arg;
            get_next_command(&pc, &command, &arg, 0, NULL);
            if (command == CMD_END)
                pc = newpc;
        }
        return stop ? ERR_STOP : ERR_NONE;
    }
}

int rtn_with_error(int err) {
    bool stop;
    if (solve_or_plot_active() && (err == ERR_OUT_OF_RANGE
                                || err == ERR_DIVIDE_BY_0
                                || err == ERR_INVALID_DATA
                                || err == ERR_STAT_MATH_ERROR
                                || err == ERR_INVALID_FORECAST_MODEL)) {
        int which;
        stop = unwind_stack_until_solve_or_plot(&which);
        if (which == -2)
            return return_to_solve(true, stop);
        else
            return return_to_plot(true, stop);
    }
    pgm_index newprgm;
    int4 newpc;
    pop_rtn_addr(&newprgm, &newpc, &stop);
    if (newprgm.idx >= 0) {
        // Stop on the calling XEQ, not the RTNERR
        current_prgm = newprgm;
        int line = pc2line(newpc);
        set_old_pc(line2pc(line - 1));
    }
    return err;
}

bool need_fstart() {
    int level = rtn_level;
    while (true) {
        if (level == 0)
            return true;
        int4 dir = rtn_stack[level - 1].dir;
        if (dir == 1)
            return false;
        if (dir > 1)
            return true;
        // dir == 0 implies is_special() == true; ignore and try the next one
        level--;
    }
}

void pop_rtn_addr(pgm_index *prgm, int4 *pc, bool *stop) {
    if (rtn_level == 0 ? rtn_level_0_has_matrix_entry : rtn_stack[rtn_level - 1].has_matrix()) {
        vartype_list *list = (vartype_list *) recall_and_purge_private_var("MAT", 3);
        if (list != NULL) {
            // Ignoring older MAT lists. Those have only 3 elements, and do
            // not include the directory id, and we can't reliably reconstruct
            // what the indexed variable's directory would have been.
            if (list->size >= 4) {
                int newdepth = list->size - 4;
                matedit_stack_entry *newstack = newdepth == 0 ? NULL : (matedit_stack_entry *) malloc(newdepth * sizeof(matedit_stack_entry));
                // TODO: Handle memory allocation failure
                vartype_string *s = (vartype_string *) list->array->data[0];
                string_copy(matedit_name, &matedit_length, s->txt(), s->length);
                matedit_dir = to_int4(((vartype_real *) list->array->data[1])->x);
                matedit_i = to_int4(((vartype_real *) list->array->data[2])->x);
                matedit_j = to_int4(((vartype_real *) list->array->data[3])->x);
                matedit_is_list = matedit_j == -1;
                if (matedit_is_list)
                    matedit_j = 0;
                matedit_stack_depth = newdepth;
                free(matedit_stack);
                matedit_stack = newstack;
                for (int i = 0; i < matedit_stack_depth; i++)
                    matedit_stack[i].set(((vartype_real *) list->array->data[i + 4])->x);
                matedit_mode = 1;
            } else {
                free(matedit_stack);
                matedit_stack = NULL;
                matedit_stack_depth = 0;
                matedit_mode = 0;
            }
            free_vartype((vartype *) list);
        }
        if (rtn_level == 0)
            rtn_level_0_has_matrix_entry = false;
        else
            rtn_stack[rtn_level - 1].set_has_matrix(false);
    }
    remove_locals();
    if (rtn_level == 0) {
        if (rtn_after_last_rtn_prgm != -1) {
            prgm->set(rtn_after_last_rtn_dir, rtn_after_last_rtn_prgm);
            *pc = rtn_after_last_rtn_pc;
            rtn_after_last_rtn_dir = -1;
            rtn_after_last_rtn_prgm = -1;
            rtn_after_last_rtn_pc = -1;
            *stop = true;
        } else {
            prgm->set(0, -1);
            *pc = -1;
        }
        rtn_stop_level = -1;
        rtn_level_0_has_func_state = false;
    } else {
        rtn_level--;
        prgm->set(rtn_stack[rtn_level].dir, rtn_stack[rtn_level].get_prgm());
        *pc = rtn_stack[rtn_level].pc;
        if (rtn_stop_level >= rtn_level) {
            *stop = true;
            rtn_stop_level = -1;
        } else
            *stop = false;
        if (prgm->idx == -2)
            rtn_solve_active = false;
        else if (prgm->idx == -3)
            rtn_integ_active = false;
        else if (prgm->idx == -5)
            rtn_plot_active = false;
    }
}

static void get_saved_stack_mode(int *m) {
    if (rtn_level == 0) {
        if (!rtn_level_0_has_func_state)
            return;
    } else {
        if (!rtn_stack[rtn_level - 1].has_func())
            return;
    }
    vartype_list *stk = (vartype_list *) recall_private_var("STK", 3);
    if (stk == NULL)
        return;
    vartype **stk_data = stk->array->data;
    *m = ((vartype_string *) stk_data[1])->txt()[0] == '1';
}

void clear_all_rtns() {
    pgm_index prgm;
    int4 dummy1;
    bool dummy2;
    int st_mode = -1;
    while (rtn_level > 0) {
        if (stack != NULL)
            get_saved_stack_mode(&st_mode);
        pop_rtn_addr(&prgm, &dummy1, &dummy2);
    }
    if (stack != NULL)
        get_saved_stack_mode(&st_mode);
    pop_rtn_addr(&prgm, &dummy1, &dummy2);
    if (st_mode == 0) {
        arg_struct dummy_arg;
        docmd_4stk(&dummy_arg);
    } else if (st_mode == 1) {
        docmd_nstk(NULL);
    }
    if (mode_plainmenu == MENU_PROGRAMMABLE)
        set_menu(MENULEVEL_PLAIN, MENU_NONE);
    if (varmenu_role == 3)
        varmenu_role = 0;
}

int get_rtn_level() {
    return rtn_level;
}

void save_csld() {
    if (rtn_level == 0)
        mode_caller_stack_lift_disabled = flags.f.stack_lift_disable;
    else
        rtn_stack[rtn_level - 1].set_csld();
}

bool is_csld() {
    if (rtn_level == 0)
        return mode_caller_stack_lift_disabled;
    else
        return rtn_stack[rtn_level - 1].is_csld();
}

bool solve_active() {
    return rtn_solve_active;
}

bool integ_active() {
    return rtn_integ_active;
}

bool solve_or_plot_active() {
    return rtn_solve_active || rtn_plot_active;
}

bool unwind_stack_until_solve_or_plot(int *which) {
    pgm_index prgm;
    int4 pc;
    bool stop;
    int st_mode = -1;
    while (true) {
        get_saved_stack_mode(&st_mode);
        pop_rtn_addr(&prgm, &pc, &stop);
        if (prgm.idx == -2 || prgm.idx == -5) {
            *which = prgm.idx;
            break;
        }
    }
    if (st_mode == 0) {
        arg_struct dummy_arg;
        docmd_4stk(&dummy_arg);
    } else if (st_mode == 1) {
        docmd_nstk(NULL);
    }
    return stop;
}

bool dir_used(int id) {
    if (current_prgm.dir == id)
        return true;
    for (int i = 0; i < rtn_level; i++)
        if (rtn_stack[i].dir == id)
            return true;
    return false;
}

bool read_bool(bool *b) {
    return read_char((char *) b);
}

bool write_bool(bool b) {
    return fputc((char) b, gfile) != EOF;
}

bool read_char(char *c) {
    int i = fgetc(gfile);
    *c = (char) i;
    return i != EOF;
}

bool write_char(char c) {
    return fputc(c, gfile) != EOF;
}

bool read_int(int *n) {
    int4 m;
    if (!read_int4(&m))
        return false;
    *n = (int) m;
    return true;
}

bool write_int(int n) {
    return write_int4(n);
}

bool read_int2(int2 *n) {
    #ifdef F42_BIG_ENDIAN
        char buf[2];
        if (fread(buf, 1, 2, gfile) != 2)
            return false;
        char *dst = (char *) n;
        for (int i = 0; i < 2; i++)
            dst[i] = buf[1 - i];
        return true;
    #else
        return fread(n, 1, 2, gfile) == 2;
    #endif
}

bool write_int2(int2 n) {
    #ifdef F42_BIG_ENDIAN
        char buf[2];
        char *src = (char *) &n;
        for (int i = 0; i < 2; i++)
            buf[i] = src[1 - i];
        return fwrite(buf, 1, 2, gfile) == 2;
    #else
        return fwrite(&n, 1, 2, gfile) == 2;
    #endif
}

bool read_int4(int4 *n) {
    #ifdef F42_BIG_ENDIAN
        char buf[4];
        if (fread(buf, 1, 4, gfile) != 4)
            return false;
        char *dst = (char *) n;
        for (int i = 0; i < 4; i++)
            dst[i] = buf[3 - i];
        return true;
    #else
        return fread(n, 1, 4, gfile) == 4;
    #endif
}

bool write_int4(int4 n) {
    #ifdef F42_BIG_ENDIAN
        char buf[4];
        char *src = (char *) &n;
        for (int i = 0; i < 4; i++)
            buf[i] = src[3 - i];
        return fwrite(buf, 1, 4, gfile) == 4;
    #else
        return fwrite(&n, 1, 4, gfile) == 4;
    #endif
}

bool read_int8(int8 *n) {
    #ifdef F42_BIG_ENDIAN
        char buf[8];
        if (fread(buf, 1, 8, gfile) != 8)
            return false;
        char *dst = (char *) n;
        for (int i = 0; i < 8; i++)
            dst[i] = buf[7 - i];
        return true;
    #else
        return fread(n, 1, 8, gfile) == 8;
    #endif
}

bool write_int8(int8 n) {
    #ifdef F42_BIG_ENDIAN
        char buf[8];
        char *src = (char *) &n;
        for (int i = 0; i < 8; i++)
            buf[i] = src[7 - i];
        return fwrite(buf, 1, 8, gfile) == 8;
    #else
        return fwrite(&n, 1, 8, gfile) == 8;
    #endif
}

bool read_phloat(phloat *d) {
    if (bin_dec_mode_switch()) {
        #ifdef F42_BIG_ENDIAN
            #ifdef BCD_MATH
                char buf[8];
                if (fread(buf, 1, 8, gfile) != 8)
                    return false;
                double dbl;
                char *dst = (char *) &dbl;
                for (int i = 0; i < 8; i++)
                    dst[i] = buf[7 - i];
                d->assign17digits(dbl);
                return true;
            #else
                char buf[16], data[16];
                if (fread(buf, 1, 16, gfile) != 16)
                    return false;
                for (int i = 0; i < 16; i++)
                    data[i] = buf[15 - i];
                *d = decimal2double(data);
                return true;
            #endif
        #else
            #ifdef BCD_MATH
                double dbl;
                if (fread(&dbl, 1, 8, gfile) != 8)
                    return false;
                d->assign17digits(dbl);
                return true;
            #else
                char data[16];
                if (fread(data, 1, 16, gfile) != 16)
                    return false;
                *d = decimal2double(data);
                return true;
            #endif
        #endif
    } else {
        #ifdef F42_BIG_ENDIAN
            #ifdef BCD_MATH
                char buf[16];
                if (fread(buf, 1, 16, gfile) != 16)
                    return false;
                    char *dst = (char *) d;
                for (int i = 0; i < 16; i++)
                    dst[i] = buf[15 - i];
                return true;
            #else
                char buf[8];
                if (fread(buf, 1, 8, gfile) != 8)
                    return false;
                char *dst = (char *) d;
                for (int i = 0; i < 8; i++)
                    dst[i] = buf[7 - i];
                return true;
            #endif
        #else
            if (fread(d, 1, sizeof(phloat), gfile) != sizeof(phloat))
                return false;
        #endif
        return true;
    }
}

bool write_phloat(phloat d) {
    #ifdef F42_BIG_ENDIAN
        #ifdef BCD_MATH
            char buf[16];
            char *src = (char *) &d;
            for (int i = 0; i < 16; i++)
                buf[i] = src[15 - i];
            return fwrite(buf, 1, 16, gfile) == 16;
        #else
            char buf[8];
            char *src = (char *) &d;
            for (int i = 0; i < 8; i++)
                buf[i] = src[7 - i];
            return fwrite(buf, 1, 8, gfile) == 8;
        #endif
    #else
        return fwrite(&d, 1, sizeof(phloat), gfile) == sizeof(phloat);
    #endif
}

struct fake_bcd {
    char data[16];
};

struct dec_arg_struct {
    unsigned char type;
    unsigned char length;
    int4 target;
    union {
        int4 num;
        char text[15];
        char stk;
        int cmd;
        char lclbl;
    } val;
    fake_bcd val_d;
};

struct bin_arg_struct {
    unsigned char type;
    unsigned char length;
    int4 target;
    union {
        int4 num;
        char text[15];
        char stk;
        int cmd;
        char lclbl;
    } val;
    double val_d;
};

bool read_arg(arg_struct *arg, bool old) {
    if (!read_char((char *) &arg->type))
        return false;
    char c;
    switch (arg->type) {
        case ARGTYPE_NONE:
            return true;
        case ARGTYPE_NUM:
        case ARGTYPE_NEG_NUM:
        case ARGTYPE_IND_NUM:
        case ARGTYPE_LBLINDEX:
            if (!read_int4(&arg->val.num))
                return false;
            if (ver < 11) {
                arg->target = 2;
                return true;
            } else
                return read_int4(&arg->target);
        case ARGTYPE_STK:
        case ARGTYPE_IND_STK:
            return read_char(&arg->val.stk);
        case ARGTYPE_STR:
        case ARGTYPE_IND_STR:
            // Serializing 'length' as a char for backward compatibility.
            // Values > 255 only happen for XSTR, and those are never
            // serialized.
            if (!read_char(&c))
                return false;
            arg->length = c & 255;
            return fread(arg->val.text, 1, arg->length, gfile) == arg->length;
        case ARGTYPE_LCLBL:
            return read_char(&arg->val.lclbl);
        case ARGTYPE_DOUBLE:
            return read_phloat(&arg->val_d);
        default:
            // Should never happen
            return false;
    }
}

bool write_arg(const arg_struct *arg) {
    int type = arg->type;
    if (type == ARGTYPE_XSTR || type == ARGTYPE_EQN)
        // This types are always used immediately, so no need to persist them;
        // also, persisting it would be difficult, since this variant uses
        // a pointer to the actual text, which is context-dependent and
        // would be impossible to restore.
        type = ARGTYPE_NONE;

    if (!write_char(type))
        return false;
    switch (type) {
        case ARGTYPE_NONE:
            return true;
        case ARGTYPE_NUM:
        case ARGTYPE_NEG_NUM:
        case ARGTYPE_IND_NUM:
        case ARGTYPE_LBLINDEX:
            return write_int4(arg->val.num) && write_int4(arg->target);
        case ARGTYPE_STK:
        case ARGTYPE_IND_STK:
            return write_char(arg->val.stk);
        case ARGTYPE_STR:
        case ARGTYPE_IND_STR:
            return write_char((char) arg->length)
                && fwrite(arg->val.text, 1, arg->length, gfile) == arg->length;
        case ARGTYPE_LCLBL:
            return write_char(arg->val.lclbl);
        case ARGTYPE_DOUBLE:
            return write_phloat(arg->val_d);
        default:
            // Should never happen
            return false;
    }
}

static bool load_state2(bool *clear, bool *too_new) {
    int4 magic;
    int4 version;
    *clear = false;
    *too_new = false;

    /* The shell has verified the initial magic and version numbers,
     * and loaded the shell state, before we got called.
     */

    if (!read_int4(&magic))
        return false;
    if (magic != PLUS42_MAGIC)
        return false;
    if (!read_int4(&ver)) {
        // A state file containing nothing after the magic number
        // is considered empty, and results in a hard reset. This
        // is *not* an error condition; such state files are used
        // when creating a new state in the States window.
        *clear = true;
        return false;
    }

    if (ver < 7)
        return false;
    if (ver > PLUS42_VERSION) {
        *too_new = true;
        return false;
    }

    // Embedded version information. No need to read this; it's just
    // there for troubleshooting purposes. All we need to do here is
    // skip it.
    while (true) {
        char c;
        if (!read_char(&c))
            return false;
        if (c == 0)
            break;
    }

    bool state_is_decimal;
    if (!read_bool(&state_is_decimal)) return false;
    if (!state_is_decimal)
        state_file_number_format = NUMBER_FORMAT_BINARY;
    else
        state_file_number_format = NUMBER_FORMAT_BID128;

    bool bdummy;
    if (!read_bool(&bdummy)) return false;
    if (!read_bool(&bdummy)) return false;
    if (!read_bool(&bdummy)) return false;

    if (!read_bool(&mode_clall)) return false;
    if (!read_bool(&mode_command_entry)) return false;
    if (!read_char(&mode_number_entry)) return false;
    if (!read_bool(&mode_alpha_entry)) return false;
    if (!read_bool(&mode_shift)) return false;
    if (!read_int(&mode_appmenu)) return false;
    if (ver < 16)
        mode_auxmenu = MENU_NONE;
    else
        if (!read_int(&mode_auxmenu)) return false;
    if (!read_int(&mode_plainmenu)) return false;
    if (!read_bool(&mode_plainmenu_sticky)) return false;
    if (!read_int(&mode_transientmenu)) return false;
    if (!read_int(&mode_alphamenu)) return false;
    if (!read_int(&mode_commandmenu)) return false;
    if (ver < 21) {
        // inserted MENU_DISP3
        if (mode_appmenu >= 30 && mode_appmenu <= 85) mode_appmenu++;
        if (mode_auxmenu >= 30 && mode_auxmenu <= 85) mode_auxmenu++;
        if (mode_plainmenu >= 30 && mode_plainmenu <= 85) mode_plainmenu++;
        if (mode_transientmenu >= 30 && mode_transientmenu <= 85) mode_transientmenu++;
        if (mode_alphamenu >= 30 && mode_alphamenu <= 85) mode_alphamenu++;
        if (mode_commandmenu >= 30 && mode_commandmenu <= 85) mode_commandmenu++;
    }
    if (ver < 26) {
        // inserted MENU_UNIT_FCN2
        if (mode_appmenu >= 76 && mode_appmenu <= 86) mode_appmenu++;
        if (mode_auxmenu >= 76 && mode_auxmenu <= 86) mode_auxmenu++;
        if (mode_plainmenu >= 76 && mode_plainmenu <= 86) mode_plainmenu++;
        if (mode_transientmenu >= 76 && mode_transientmenu <= 86) mode_transientmenu++;
        if (mode_alphamenu >= 76 && mode_alphamenu <= 86) mode_alphamenu++;
        if (mode_commandmenu >= 76 && mode_commandmenu <= 86) mode_commandmenu++;
    }
    if (ver < 30) {
        // inserted MENU_DISP4
        if (mode_appmenu >= 31 && mode_appmenu <= 87) mode_appmenu++;
        if (mode_auxmenu >= 31 && mode_auxmenu <= 87) mode_auxmenu++;
        if (mode_plainmenu >= 31 && mode_plainmenu <= 87) mode_plainmenu++;
        if (mode_transientmenu >= 31 && mode_transientmenu <= 87) mode_transientmenu++;
        if (mode_alphamenu >= 31 && mode_alphamenu <= 87) mode_alphamenu++;
        if (mode_commandmenu >= 31 && mode_commandmenu <= 87) mode_commandmenu++;
    }
    if (ver < 45) {
        // inserted MENU_ALPHA_PUNC3
        if (mode_appmenu >= 18 && mode_appmenu <= 88) mode_appmenu++;
        if (mode_auxmenu >= 18 && mode_auxmenu <= 88) mode_auxmenu++;
        if (mode_plainmenu >= 18 && mode_plainmenu <= 88) mode_plainmenu++;
        if (mode_transientmenu >= 18 && mode_transientmenu <= 88) mode_transientmenu++;
        if (mode_alphamenu >= 18 && mode_alphamenu <= 88) mode_alphamenu++;
        if (mode_commandmenu >= 18 && mode_commandmenu <= 88) mode_commandmenu++;
    }
    if (!read_bool(&mode_running)) return false;
    if (ver < 28)
        mode_caller_stack_lift_disabled = false;
    else if (!read_bool(&mode_caller_stack_lift_disabled))
        return false;
    if (!read_bool(&mode_varmenu)) return false;
    if (ver < 19) {
        mode_varmenu_whence = CATSECT_TOP;
    } else {
        if (!read_int(&mode_varmenu_whence)) return false;
    }
    if (!read_bool(&mode_updown)) return false;

    if (!read_bool(&mode_getkey))
        return false;

    if (!read_phloat(&entered_number)) return false;
    if (!read_int(&entered_string_length)) return false;
    if (fread(entered_string, 1, 15, gfile) != 15) return false;

    if (!read_int(&pending_command)) return false;
    if (!read_arg(&pending_command_arg, false)) return false;
    if (!read_int(&xeq_invisible)) return false;

    if (!read_int(&incomplete_command)) return false;
    if (!read_bool(&incomplete_ind)) return false;
    if (!read_bool(&incomplete_alpha)) return false;
    if (!read_int(&incomplete_length)) return false;
    if (!read_int(&incomplete_maxdigits)) return false;
    if (!read_int(&incomplete_argtype)) return false;
    if (!read_int(&incomplete_num)) return false;
    int isl = ver < 23 ? 22 : incomplete_length;
    if (fread(incomplete_str, 1, isl, gfile) != isl) return false;
    if (!read_int4(&incomplete_saved_pc)) return false;
    if (!read_int4(&incomplete_saved_highlight_row)) return false;

    if (fread(cmdline, 1, 100, gfile) != 100) return false;
    if (!read_int(&cmdline_length)) return false;
    if (!read_int(&cmdline_unit)) return false;
    if (ver < 13) {
        int dummy;
        if (!read_int(&dummy)) return false;
    }

    if (!read_int(&matedit_mode)) return false;
    if (ver < 12)
        // Without a valid directory id, we can't reliably tell
        // which variable matedit would have been referring to
        matedit_mode = 0;
    else
        if (!read_int4(&matedit_dir)) return false;
    if (fread(matedit_name, 1, 7, gfile) != 7) return false;
    if (!read_int(&matedit_length)) return false;
    if (!unpersist_vartype(&matedit_x)) return false;
    if (!read_int4(&matedit_i)) return false;
    if (!read_int4(&matedit_j)) return false;
    if (!read_int(&matedit_prev_appmenu)) return false;
    if (ver < 33) {
        matedit_stack = NULL;
        matedit_stack_depth = 0;
        matedit_is_list = false;
    } else {
        if (!read_int(&matedit_stack_depth)) return false;
        if (matedit_stack_depth == 0) {
            matedit_stack = NULL;
        } else {
            matedit_stack = (matedit_stack_entry *) malloc(matedit_stack_depth * sizeof(matedit_stack_entry));
            if (matedit_stack == NULL) {
                matedit_stack_depth = 0;
                return false;
            }
            if (false) {
                nomem:
                free(matedit_stack);
                matedit_stack = NULL;
                matedit_stack_depth = 0;
                return false;
            }
            if (ver < 36)
                for (int i = 0; i < matedit_stack_depth; i++) {
                    int4 coord;
                    if (!read_int4(&coord))
                        goto nomem;
                    matedit_stack[i].set(coord, -1);
                }
            else
                for (int i = 0; i < matedit_stack_depth; i++) {
                    int8 combined;
                    if (!read_int8(&combined))
                        goto nomem;
                    matedit_stack[i].set(combined);
                }
        }
        if (!read_bool(&matedit_is_list)) return false;
        if (ver < 37) {
            matedit_view_i = -1;
            matedit_view_j = -1;
        } else {
            if (!read_int4(&matedit_view_i)) goto nomem;
            if (!read_int4(&matedit_view_j)) goto nomem;
        }
    }

    if (fread(input_name, 1, 11, gfile) != 11) return false;
    if (!read_int(&input_length)) return false;
    if (!read_arg(&input_arg, false)) return false;

    if (!read_int(&lasterr)) return false;
    if (!read_int(&lasterr_length)) return false;
    if (fread(lasterr_text, 1, 22, gfile) != 22) return false;

    if (!read_int(&baseapp)) return false;

    if (!read_int8(&random_number_low)) return false;
    if (!read_int8(&random_number_high)) return false;

    if (!read_int(&deferred_print)) return false;

    if (!read_int(&keybuf_head)) return false;
    if (!read_int(&keybuf_tail)) return false;
    for (int i = 0; i < 16; i++)
        if (!read_int(&keybuf[i]))
            return false;

    if (!unpersist_display(ver))
        return false;
    if (!unpersist_globals())
        return false;
    if (!unpersist_eqn(ver))
        return false;
    if (!unpersist_math(ver))
        return false;
    pc = line2pc(pc);
    incomplete_saved_pc = line2pc(incomplete_saved_pc);

    // It would be better to also prevent all the useless rebuild_label_table()
    // calls that have happened during state file loading until this point.
    // All that churn is harmless, but useless...
    rebuild_label_table();

    if (!read_int4(&magic)) return false;
    if (magic != PLUS42_MAGIC)
        return false;
    if (!read_int4(&version)) return false;
    if (version != ver)
        return false;

    // When parser or code generator bugs are fixed, or when the semantics of
    // generated code are changed, re-parse all equations so all equation code
    // is re-generated.
    if (ver < 41) {
        set_running(false);
        clear_all_rtns();
        pc = -1;
        reparse_all_equations();
        if (flags.f.prgm_mode && current_prgm.dir == 1)
            force_redisplay = true;
    }

    // Update equation reference counts to account for all EMBED instructions
    count_embed_references(root, true);
    for (int i = 0; i < eq_dir->prgms_count; i++) {
        /* Remove orphaned equations */
        equation_data *eqd = eq_dir->prgms[i].eq_data;
        if (eqd != NULL && eqd->refcount == 0) {
            delete eqd;
            eq_dir->prgms[i].eq_data = NULL;
            free(eq_dir->prgms[i].text);
            eq_dir->prgms[i].text = NULL;
        }
    }
    count_embed_references(eq_dir, true);

    return true;
}

bool load_state(bool *clear, bool *too_new) {
    shared_data_count = 0;
    shared_data_capacity = 0;
    shared_data = NULL;

    loading_state = true;
    bool ret = load_state2(clear, too_new);
    loading_state = false;

    free(shared_data);
    return ret;
}

static void save_state2(bool *success) {
    *success = false;
    if (!write_int4(PLUS42_MAGIC) || !write_int4(PLUS42_VERSION))
        return;

    // Write app version and platform, for troubleshooting purposes
    const char *platform = shell_platform();
    char c;
    do {
        c = *platform++;
        write_char(c);
    } while (c != 0);

    #ifdef BCD_MATH
        if (!write_bool(true)) return;
    #else
        if (!write_bool(false)) return;
    #endif
    if (!write_bool(core_settings.matrix_singularmatrix)) return;
    if (!write_bool(core_settings.matrix_outofrange)) return;
    if (!write_bool(core_settings.auto_repeat)) return;
    if (!write_bool(mode_clall)) return;
    if (!write_bool(mode_command_entry)) return;
    if (!write_char(mode_number_entry)) return;
    if (!write_bool(mode_alpha_entry)) return;
    if (!write_bool(mode_shift)) return;
    if (!write_int(mode_appmenu)) return;
    if (!write_int(mode_auxmenu)) return;
    if (!write_int(mode_plainmenu)) return;
    if (!write_bool(mode_plainmenu_sticky)) return;
    if (!write_int(mode_transientmenu)) return;
    if (!write_int(mode_alphamenu)) return;
    if (!write_int(mode_commandmenu)) return;
    if (!write_bool(mode_running)) return;
    if (!write_bool(mode_caller_stack_lift_disabled)) return;
    if (!write_bool(mode_varmenu)) return;
    if (!write_int(mode_varmenu_whence)) return;
    if (!write_bool(mode_updown)) return;
    if (!write_bool(mode_getkey)) return;

    if (!write_phloat(entered_number)) return;
    if (!write_int(entered_string_length)) return;
    if (fwrite(entered_string, 1, 15, gfile) != 15) return;

    if (!write_int(pending_command)) return;
    if (!write_arg(&pending_command_arg)) return;
    if (!write_int(xeq_invisible)) return;

    if (!write_int(incomplete_command)) return;
    if (!write_bool(incomplete_ind)) return;
    if (!write_bool(incomplete_alpha)) return;
    if (!write_int(incomplete_length)) return;
    if (!write_int(incomplete_maxdigits)) return;
    if (!write_int(incomplete_argtype)) return;
    if (!write_int(incomplete_num)) return;
    if (fwrite(incomplete_str, 1, incomplete_length, gfile) != incomplete_length) return;
    if (!write_int4(pc2line(incomplete_saved_pc))) return;
    if (!write_int4(incomplete_saved_highlight_row)) return;

    if (fwrite(cmdline, 1, 100, gfile) != 100) return;
    if (!write_int(cmdline_length)) return;
    if (!write_int(cmdline_unit)) return;

    if (!write_int(matedit_mode)) return;
    if (!write_int4(matedit_dir)) return;
    if (fwrite(matedit_name, 1, 7, gfile) != 7) return;
    if (!write_int(matedit_length)) return;
    if (!persist_vartype(matedit_x)) return;
    if (!write_int4(matedit_i)) return;
    if (!write_int4(matedit_j)) return;
    if (!write_int(matedit_prev_appmenu)) return;
    if (!write_int(matedit_stack_depth)) return;
    for (int i = 0; i < matedit_stack_depth; i++)
        if (!write_int8(matedit_stack[i].as_int8())) return;
    if (!write_bool(matedit_is_list)) return;
    if (!write_int4(matedit_view_i)) return;
    if (!write_int4(matedit_view_j)) return;

    if (fwrite(input_name, 1, 11, gfile) != 11) return;
    if (!write_int(input_length)) return;
    if (!write_arg(&input_arg)) return;

    if (!write_int(lasterr)) return;
    if (!write_int(lasterr_length)) return;
    if (fwrite(lasterr_text, 1, 22, gfile) != 22) return;

    if (!write_int(baseapp)) return;

    if (!write_int8(random_number_low)) return;
    if (!write_int8(random_number_high)) return;

    if (!write_int(deferred_print)) return;

    if (!write_int(keybuf_head)) return;
    if (!write_int(keybuf_tail)) return;
    for (int i = 0; i < 16; i++)
        if (!write_int(keybuf[i]))
            return;

    if (!persist_display())
        return;
    if (!persist_globals())
        return;
    if (!persist_eqn())
        return;
    if (!persist_math())
        return;

    if (!write_int4(PLUS42_MAGIC)) return;
    if (!write_int4(PLUS42_VERSION)) return;
    *success = true;
}

bool save_state() {
    shared_data_count = 0;
    shared_data_capacity = 0;
    shared_data = NULL;

    bool success;
    saving_state = true;
    save_state2(&success);
    saving_state = false;

    free(shared_data);
    return success;
}

// Reason:
// 0 = Memory Clear
// 1 = State File Corrupt
// 2 = State File Too New
void hard_reset(int reason) {
    vartype *regs;

    /* Clear stack */
    for (int i = 0; i <= sp; i++)
        free_vartype(stack[i]);
    free(stack);
    free_vartype(lastx);
    sp = 3;
    stack_capacity = 4;
    stack = (vartype **) malloc(stack_capacity * sizeof(vartype *));
    for (int i = 0; i <= sp; i++)
        stack[i] = new_real(0);
    lastx = new_real(0);

    /* Clear alpha */
    reg_alpha_length = 0;

    /* Clear RTN stack, variables, and programs */
    clear_rtns_vars_and_prgms();

    /* Reinitialize RTN stack */
    if (rtn_stack != NULL)
        free(rtn_stack);
    rtn_stack_capacity = 16;
    rtn_stack = (rtn_stack_entry *) malloc(rtn_stack_capacity * sizeof(rtn_stack_entry));
    rtn_level = 0;
    rtn_stop_level = -1;
    rtn_solve_active = false;
    rtn_integ_active = false;
    rtn_plot_active = false;

    /* Reinitialize programs */
    eq_dir = new directory(1);
    map_dir(1, eq_dir);
    root = new directory(2);
    map_dir(2, root);
    cwd = root;

    /* Reinitialize variables */
    regs = new_realmatrix(25, 1);
    store_var("REGS", 4, regs);

    bool prev_loading_state = loading_state;
    loading_state = true;
    goto_dot_dot(false);
    loading_state = prev_loading_state;


    pending_command = CMD_NONE;

    matedit_mode = 0;
    matedit_stack_depth = 0;
    free(matedit_stack);
    matedit_stack = NULL;
    input_length = 0;
    baseapp = 0;
    random_number_low = 0;
    random_number_high = 0;

    flags.f.f00 = flags.f.f01 = flags.f.f02 = flags.f.f03 = flags.f.f04 = 0;
    flags.f.f05 = flags.f.f06 = flags.f.f07 = flags.f.f08 = flags.f.f09 = 0;
    flags.f.f10 = 0;
    flags.f.auto_exec = 0;
    flags.f.double_wide_print = 0;
    flags.f.lowercase_print = 0;
    flags.f.f14 = 0;
    flags.f.trace_print = 0;
    flags.f.normal_print = 0;
    flags.f.f17 = flags.f.f18 = flags.f.f19 = flags.f.f20 = 0;
    flags.f.printer_enable = 0;
    flags.f.numeric_data_input = 0;
    flags.f.alpha_data_input = 0;
    flags.f.range_error_ignore = 0;
    flags.f.error_ignore = 0;
    flags.f.audio_enable = 1;
    /* flags.f.VIRTUAL_custom_menu = 0; */
    flags.f.decimal_point = number_format()[0] != ','; // HP-42S sets this to 1 on hard reset
    flags.f.thousands_separators = 1;
    flags.f.stack_lift_disable = 0;
    int df = shell_date_format();
    flags.f.dmy = df == 1;
    flags.f.direct_solver = 1;
    flags.f.f33 = 0;
    flags.f.agraph_control1 = 0;
    flags.f.agraph_control0 = 0;
    flags.f.digits_bit3 = 0;
    flags.f.digits_bit2 = 1;
    flags.f.digits_bit1 = 0;
    flags.f.digits_bit0 = 0;
    flags.f.fix_or_all = 1;
    flags.f.eng_or_all = 0;
    flags.f.grad = 0;
    flags.f.rad = 0;
    /* flags.f.VIRTUAL_continuous_on = 0; */
    /* flags.f.VIRTUAL_solving = 0; */
    /* flags.f.VIRTUAL_integrating = 0; */
    /* flags.f.VIRTUAL_variable_menu = 0; */
    /* flags.f.VIRTUAL_alpha_mode = 0; */
    /* flags.f.VIRTUAL_low_battery = 0; */
    /* flags.f.VIRTUAL_message = 1; */
    /* flags.f.VIRTUAL_two_line_message = 0; */
    flags.f.prgm_mode = 0;
    /* flags.f.VIRTUAL_input = 0; */
    flags.f.eqn_compat = 0;
    flags.f.printer_exists = 0;
    flags.f.lin_fit = 1;
    flags.f.log_fit = 0;
    flags.f.exp_fit = 0;
    flags.f.pwr_fit = 0;
    flags.f.all_sigma = 1;
    flags.f.log_fit_invalid = 0;
    flags.f.exp_fit_invalid = 0;
    flags.f.pwr_fit_invalid = 0;
    flags.f.shift_state = 0;
    /* flags.f.VIRTUAL_matrix_editor = 0; */
    flags.f.grow = 0;
    flags.f.ymd = df == 2;
    flags.f.base_bit0 = 0;
    flags.f.base_bit1 = 0;
    flags.f.base_bit2 = 0;
    flags.f.base_bit3 = 0;
    flags.f.local_label = 0;
    flags.f.polar = 0;
    flags.f.real_result_only = 0;
    /* flags.f.VIRTUAL_programmable_menu = 0; */
    flags.f.matrix_edge_wrap = 0;
    flags.f.matrix_end_wrap = 0;
    flags.f.base_signed = 1;
    flags.f.base_wrap = 0;
    flags.f.big_stack = 0;
    flags.f.f81 = flags.f.f82 = flags.f.f83 = flags.f.f84 = 0;
    flags.f.f85 = flags.f.f86 = flags.f.f87 = flags.f.f88 = flags.f.f89 = 0;
    flags.f.f90 = flags.f.f91 = flags.f.f92 = flags.f.f93 = flags.f.f94 = 0;
    flags.f.f95 = flags.f.f96 = flags.f.f97 = flags.f.f98 = flags.f.f99 = 0;

    mode_clall = false;
    mode_command_entry = false;
    mode_number_entry = false;
    mode_alpha_entry = false;
    mode_shift = false;
    mode_commandmenu = MENU_NONE;
    mode_alphamenu = MENU_NONE;
    mode_transientmenu = MENU_NONE;
    mode_plainmenu = MENU_NONE;
    mode_auxmenu = MENU_NONE;
    mode_appmenu = MENU_NONE;
    mode_running = false;
    mode_getkey = false;
    mode_pause = false;
    mode_caller_stack_lift_disabled = false;
    mode_varmenu = false;
    mode_varmenu_whence = CATSECT_TOP;
    prgm_highlight_row = 0;
    varmenu_eqn = NULL;
    varmenu_length = 0;
    mode_updown = false;
    mode_sigma_reg = 11;
    mode_goose = -1;
    mode_time_clktd = false;
    mode_time_clk24 = shell_clk24();
    mode_wsize = 36;
    mode_header = true;
    mode_amort_seq = 0;
    mode_plot_viewer = false;
    mode_plot_key = 0;
    mode_plot_sp = 0;
    mode_plot_inv = NULL;
    mode_plot_result_width = 0;
    mode_multi_line = true;
    mode_lastx_top = false;
    mode_alpha_top = false;
    mode_header_flags = false;
    mode_header_polar = false;
    mode_matedit_stk = false;

    reset_math();
    reset_eqn();

    clear_display();
    clear_custom_menu();
    clear_prgm_menu();
    switch (reason) {
        case 0:
            draw_message(0, "Memory Clear", 12, false);
            break;
        case 1:
            draw_message(0, "State File Corrupt", 18, false);
            break;
        case 2:
            draw_message(0, "State File Too New", 18, false);
            break;
    }
}

#ifdef IPHONE
bool off_enabled() {
    if (off_enable_flag)
        return true;
    if (sp == -1 || stack[sp]->type != TYPE_STRING)
        return false;
    vartype_string *str = (vartype_string *) stack[sp];
    off_enable_flag = string_equals(str->txt(), str->length, "YESOFF", 6);
    return off_enable_flag;
}
#endif
