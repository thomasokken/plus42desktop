/*****************************************************************************
 * Plus42 -- an enhanced HP-42S calculator simulator
 * Copyright (C) 2004-2024  Thomas Okken
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

#include "core_equations.h"
#include "core_commands1.h"
#include "core_commands2.h"
#include "core_commands7.h"
#include "core_commands8.h"
#include "core_commandsa.h"
#include "core_display.h"
#include "core_helpers.h"
#include "core_main.h"
#include "core_parser.h"
#include "shell.h"
#include "shell_spool.h"

static bool active = false;
static int menu_whence;

static vartype_list *eqns;
static int4 num_eqns;
static int selected_row = -1; // -1: top of list; num_eqns: bottom of list
static int edit_pos; // -1: in list; >= 0: in editor
static int display_pos;
static int screen_row = 0;
static int headers = 0;

static int error_eqn_id;
static int error_eqn_pos;

#define DIALOG_NONE 0
#define DIALOG_SAVE_CONFIRM 1
#define DIALOG_DELETE_CONFIRM 2
#define DIALOG_DELETE_BOTH_CONFIRM 3
#define DIALOG_RCL 4
#define DIALOG_STO 5
#define DIALOG_STO_OVERWRITE_X 6
#define DIALOG_STO_OVERWRITE_PRGM 7
#define DIALOG_STO_OVERWRITE_ALPHA 8
#define DIALOG_MODES 9
static int dialog = DIALOG_NONE;
static int dialog_min;
static int dialog_max;
static int dialog_n;
static int dialog_pos;
static int dialog_cmd;

struct menu_location {
    int id;
    int catsect;
    int catsect_rows;
    int catalog_row;
    bool skip_top;
};

static menu_location edit;
static menu_location prev_edit;
static bool menu_sticky;
static int menu_item[6];
static bool new_eq;
static char *edit_buf = NULL;
static int4 edit_len, edit_capacity;
static bool cursor_on;
static int current_error = ERR_NONE;
static vartype *current_result = NULL;

static int timeout_action = 0;
static int timeout_edit_pos;
static int rep_key = -1;

#define EQMN_PGM_FCN1   1000
#define EQMN_PGM_FCN2   1001
#define EQMN_PGM_FCN3   1002
#define EQMN_PGM_FCN4   1003
#define EQMN_PGM_TYPES1 1004
#define EQMN_PGM_TYPES2 1005
#define EQMN_MATRIX1    1006
#define EQMN_MATRIX2    1007
#define EQMN_BASE1      1008
#define EQMN_BASE2      1009
#define EQMN_CONVERT1   1010
#define EQMN_CONVERT2   1011
#define EQMN_CONVERT3   1012
#define EQMN_CONVERT4   1013
#define EQMN_EXTRA_FCN1 1014
#define EQMN_EXTRA_FCN2 1015
#define EQMN_EXTRA_FCN3 1016
#define EQMN_EXTRA_FCN4 1017
#define EQMN_STACK      1018
#define EQMN_STAT1      1019
#define EQMN_STAT2      1020
#define EQMN_STAT3      1021
#define EQMN_STAT4      1022
#define EQMN_FIN1       1023
#define EQMN_FIN2       1024

#define EQCMD_XCOORD   1000
#define EQCMD_YCOORD   1001
#define EQCMD_RADIUS   1002
#define EQCMD_ANGLE    1003
#define EQCMD_INT      1004
#define EQCMD_FOR      1005
#define EQCMD_BREAK    1006
#define EQCMD_CONTINUE 1007
#define EQCMD_SIZES    1008
#define EQCMD_MROWS    1009
#define EQCMD_MCOLS    1010
#define EQCMD_TRN      1011
#define EQCMD_IDIV     1012
#define EQCMD_SEQ      1013
#define EQCMD_MAX      1014
#define EQCMD_MIN      1015
#define EQCMD_REGX     1016
#define EQCMD_REGY     1017
#define EQCMD_REGZ     1018
#define EQCMD_REGT     1019
#define EQCMD_STACK    1020
#define EQCMD_MEANX    1021
#define EQCMD_MEANY    1022
#define EQCMD_SDEVX    1023
#define EQCMD_SDEVY    1024
#define EQCMD_SIZEC    1025
#define EQCMD_FLOW     1026
#define EQCMD_NUM_T    1027
#define EQCMD_TAIL     1028

struct eqn_cmd_spec {
    const char *name;
    int namelen;
    bool no_args;
};

const eqn_cmd_spec eqn_cmds[] = {
    { /* XCOORD */   "XCOORD",      6, false },
    { /* YCOORD */   "YCOORD",      6, false },
    { /* RADIUS */   "RADIUS",      6, false },
    { /* ANGLE */    "ANGLE",       5, false },
    { /* INT */      "INT",         3, false },
    { /* FOR */      "FOR",         3, false },
    { /* BREAK */    "BR\305\301K", 5, true  },
    { /* CONTINUE */ "CONT\311\316\325\305", 8, true  },
    { /* SIZES */    "SIZES",       5, false },
    { /* MROWS */    "MROWS",       5, false },
    { /* MCOLS */    "MCOLS",       5, false },
    { /* TRN */      "TRN",         3, false },
    { /* IDIV */     "IDIV",        4, false },
    { /* SEQ */      "SEQ",         3, false },
    { /* MAX */      "MAX",         3, false },
    { /* MIN */      "MIN",         3, false },
    { /* REGX */     "REGX",        4, true  },
    { /* REGY */     "REGY",        4, true  },
    { /* REGZ */     "REGZ",        4, true  },
    { /* REGT */     "REGT",        4, true  },
    { /* STACK */    "ST\301\303K\333", 6, true  },
    { /* MEANX */    "M\305\301NX", 5, true  },
    { /* MEANY */    "M\305\301NY", 5, true  },
    { /* SDEVX */    "SDEVX",       5, true  },
    { /* SDEVY */    "SDEVY",       5, true  },
    { /* SIZEC */    "SIZEC",       5, false },
    { /* FLOW */     "FLOW",        4, false },
    { /* NUM_T */    "#T",          2, false },
    { /* TAIL */     "TAIL",        4, false }
};

const menu_spec eqn_menus[] = {
    { /* EQMN_PGM_FCN1 */ MENU_NONE, EQMN_PGM_FCN2, EQMN_PGM_FCN4,
                      { { 0x0000 + CMD_IF_T,       2, "IF" },
                        { 0x1000 + EQCMD_FOR,      0, ""   },
                        { 0x1000 + EQCMD_BREAK,    0, ""   },
                        { 0x1000 + EQCMD_CONTINUE, 0, ""   },
                        { 0x1000 + EQCMD_SEQ,      0, ""   },
                        { 0x1000 + CMD_XEQ,        0, ""   } } },
    { /* EQMN_PGM_FCN2 */ MENU_NONE, EQMN_PGM_FCN3, EQMN_PGM_FCN1,
                      { { 0x0000 + CMD_GSTO,    1, "L"     },
                        { 0x0000 + CMD_GRCL,    1, "G"     },
                        { 0x0000 + CMD_SVAR,    1, "S"     },
                        { 0x0000 + CMD_GETITEM, 4, "ITEM"  },
                        { 0x1000 + EQCMD_MAX,   0, ""      },
                        { 0x1000 + EQCMD_MIN,   0, ""      } } },
    { /* EQMN_PGM_FCN3 */ MENU_NONE, EQMN_PGM_FCN4, EQMN_PGM_FCN2,
                      { { 0x1000 + CMD_STOP,        0, "" },
                        { 0x1000 + CMD_VIEW,        0, "" },
                        { 0x2000 + EQMN_PGM_TYPES1, 5, "TYPES" },
                        { 0x1000 + CMD_NULL,        0, "" },
                        { 0x0000 + CMD_SIGMAADD,    1, "\5" },
                        { 0x0000 + CMD_SIGMASUB,    1, "\3" } } },
    { /* EQMN_PGM_FCN4 */ MENU_NONE, EQMN_PGM_FCN1, EQMN_PGM_FCN3,
                      { { 0x1000 + CMD_DATE,    0, ""    },
                        { 0x1000 + CMD_TIME,    0, ""    },
                        { 0x0000 + CMD_GEN_AND, 3, "AND" },
                        { 0x0000 + CMD_GEN_OR,  2, "OR"  },
                        { 0x0000 + CMD_GEN_XOR, 3, "XOR" },
                        { 0x0000 + CMD_GEN_NOT, 3, "NOT" } } },
    { /* EQMN_PGM_TYPES1 */ EQMN_PGM_FCN3, EQMN_PGM_TYPES2, EQMN_PGM_TYPES2,
                      { { 0x1000 + CMD_REAL_T,   0, "" },
                        { 0x1000 + CMD_CPX_T,    0, "" },
                        { 0x1000 + CMD_MAT_T,    0, "" },
                        { 0x1000 + CMD_CPXMAT_T, 0, "" },
                        { 0x1000 + CMD_STR_T,    0, "" },
                        { 0x1000 + CMD_LIST_T,   0, "" } } },
    { /* EQMN_PGM_TYPES2 */ EQMN_PGM_FCN3, EQMN_PGM_TYPES1, EQMN_PGM_TYPES1,
                      { { 0x1000 + CMD_EQN_T,  0, "" },
                        { 0x1000 + CMD_UNIT_T, 0, "" },
                        { 0x1000 + CMD_TYPE_T, 0, "" },
                        { 0x1000 + CMD_NULL,   0, "" },
                        { 0x1000 + CMD_NULL,   0, "" },
                        { 0x1000 + CMD_NULL,   0, "" } } },
    { /* EQMN_MATRIX1 */ MENU_NONE, EQMN_MATRIX2, EQMN_MATRIX2,
                      { { 0x1000 + CMD_NEWMAT, 0, "" },
                        { 0x1000 + CMD_INVRT,  0, "" },
                        { 0x1000 + CMD_DET,    0, "" },
                        { 0x1000 + CMD_TRANS,  0, "" },
                        { 0x1000 + CMD_FNRM,   0, "" },
                        { 0x1000 + CMD_RNRM,   0, "" } } },
    { /* EQMN_MATRIX2 */ MENU_NONE, EQMN_MATRIX1, EQMN_MATRIX1,
                      { { 0x1000 + CMD_DOT,     0, ""  },
                        { 0x1000 + CMD_CROSS,   0, ""  },
                        { 0x1000 + CMD_UVEC,    0, ""  },
                        { 0x1000 + CMD_RSUM,    0, ""  },
                        { 0x1000 + EQCMD_MROWS, 0, ""  },
                        { 0x1000 + EQCMD_MCOLS, 0, ""  } } },
    { /* EQMN_BASE1 */ MENU_NONE, EQMN_BASE2, EQMN_BASE2,
                      { { 0x1000 + CMD_BASEADD, 0, "" },
                        { 0x1000 + CMD_BASESUB, 0, "" },
                        { 0x1000 + CMD_BASEMUL, 0, "" },
                        { 0x1000 + CMD_BASEDIV, 0, "" },
                        { 0x1000 + CMD_BASECHS, 0, "" },
                        { 0x1000 + CMD_NULL,    0, "" } } },
    { /* EQMN_BASE2 */ MENU_NONE, EQMN_BASE1, EQMN_BASE1,
                      { { 0x1000 + CMD_AND,  0, "" },
                        { 0x1000 + CMD_OR,   0, "" },
                        { 0x1000 + CMD_XOR,  0, "" },
                        { 0x1000 + CMD_NOT,  0, "" },
                        { 0x1000 + CMD_NULL, 0, "" },
                        { 0x1000 + CMD_NULL, 0, "" } } },
    { /* EQMN_CONVERT1 */ MENU_NONE, EQMN_CONVERT2, EQMN_CONVERT4,
                      { { 0x1000 + EQCMD_XCOORD, 0, "" },
                        { 0x1000 + EQCMD_YCOORD, 0, "" },
                        { 0x1000 + EQCMD_RADIUS, 0, "" },
                        { 0x1000 + EQCMD_ANGLE,  0, "" },
                        { 0x1000 + CMD_RCOMPLX,  0, "" },
                        { 0x1000 + CMD_PCOMPLX,  0, "" } } },
    { /* EQMN_CONVERT2 */ MENU_NONE, EQMN_CONVERT3, EQMN_CONVERT1,
                      { { 0x1000 + CMD_TO_DEG,   0, "" },
                        { 0x1000 + CMD_TO_RAD,   0, "" },
                        { 0x1000 + CMD_TO_HR,    0, "" },
                        { 0x1000 + CMD_TO_HMS,   0, "" },
                        { 0x1000 + CMD_HMSADD,   0, "" },
                        { 0x1000 + CMD_HMSSUB,   0, "" } } },
    { /* EQMN_CONVERT3 */ MENU_NONE, EQMN_CONVERT4, EQMN_CONVERT2,
                      { { 0x1000 + CMD_IP,     0, "" },
                        { 0x1000 + CMD_FP,     0, "" },
                        { 0x1000 + EQCMD_INT,  0, "" },
                        { 0x1000 + CMD_RND,    0, "" },
                        { 0x1000 + EQCMD_TRN,  0, "" },
                        { 0x1000 + CMD_NULL,   0, "" } } },
    { /* EQMN_CONVERT4 */ MENU_NONE, EQMN_CONVERT1, EQMN_CONVERT3,
                      { { 0x1000 + CMD_ABS,    0, "" },
                        { 0x1000 + CMD_SIGN,   0, "" },
                        { 0x1000 + EQCMD_IDIV, 0, "" },
                        { 0x1000 + CMD_MOD,    0, "" },
                        { 0x1000 + CMD_TO_DEC, 0, "" },
                        { 0x1000 + CMD_TO_OCT, 0, "" } } },
    { /* EQMN_EXTRA_FCN1 */ MENU_NONE, EQMN_EXTRA_FCN2, EQMN_EXTRA_FCN4,
                      { { 0x1000 + CMD_SINH,  0, "" },
                        { 0x1000 + CMD_ASINH, 0, "" },
                        { 0x1000 + CMD_COSH,  0, "" },
                        { 0x1000 + CMD_ACOSH, 0, "" },
                        { 0x1000 + CMD_TANH,  0, "" },
                        { 0x1000 + CMD_ATANH, 0, "" } } },
    { /* EQMN_EXTRA_FCN2 */ MENU_NONE, EQMN_EXTRA_FCN3, EQMN_EXTRA_FCN1,
                      { { 0x1000 + CMD_LN_1_X,    0, "" },
                        { 0x1000 + CMD_E_POW_X_1, 0, "" },
                        { 0x1000 + CMD_DATE_PLUS, 0, "" },
                        { 0x1000 + CMD_DDAYS,     0, "" },
                        { 0x1000 + CMD_NULL,      0, "" },
                        { 0x1000 + CMD_NN_TO_S,   0, "" } } },
    { /* EQMN_EXTRA_FCN3 */ MENU_NONE, EQMN_EXTRA_FCN4, EQMN_EXTRA_FCN2,
                      { { 0x1000 + CMD_N_TO_S, 0, "" },
                        { 0x1000 + CMD_S_TO_N, 0, "" },
                        { 0x1000 + CMD_N_TO_C, 0, "" },
                        { 0x1000 + CMD_C_TO_N, 0, "" },
                        { 0x1000 + CMD_APPEND, 0, "" },
                        { 0x1000 + CMD_EXTEND, 0, "" } } },
    { /* EQMN_EXTRA_FCN4 */ MENU_NONE, EQMN_EXTRA_FCN1, EQMN_EXTRA_FCN3,
                      { { 0x1000 + CMD_HEAD,   0, "" },
                        { 0x1000 + EQCMD_TAIL, 0, "" },
                        { 0x1000 + CMD_LENGTH, 0, "" },
                        { 0x1000 + CMD_POS,    0, "" },
                        { 0x1000 + CMD_SUBSTR, 0, "" },
                        { 0x1000 + CMD_REV,    0, "" } } },
    { /* EQMN_STACK */ MENU_NONE, MENU_NONE, MENU_NONE,
                      { { 0x1000 + EQCMD_REGX,  0, "" },
                        { 0x1000 + EQCMD_REGY,  0, "" },
                        { 0x1000 + EQCMD_REGZ,  0, "" },
                        { 0x1000 + EQCMD_REGT,  0, "" },
                        { 0x1000 + CMD_LASTX,   0, "" },
                        { 0x1000 + EQCMD_STACK, 0, "" } } },
    { /* EQMN_STAT1 */ MENU_NONE, EQMN_STAT2, EQMN_STAT4,
                      { { 0x1000 + EQCMD_MEANX,   0, "" },
                        { 0x1000 + EQCMD_MEANY,   0, "" },
                        { 0x1000 + EQCMD_SDEVX,   0, "" },
                        { 0x1000 + EQCMD_SDEVY,   0, "" },
                        { 0x1000 + CMD_WMEAN,     0, "" },
                        { 0x1000 + CMD_CORR,      0, "" } } },
    { /* EQMN_STAT2 */ MENU_NONE, EQMN_STAT3, EQMN_STAT1,
                      { { 0x1000 + CMD_FCSTX,     0, "" },
                        { 0x1000 + CMD_FCSTY,     0, "" },
                        { 0x1000 + CMD_SLOPE,     0, "" },
                        { 0x1000 + CMD_YINT,      0, "" },
                        { 0x1000 + CMD_NULL,      0, "" },
                        { 0x1000 + CMD_SN,        0, "" } } },
    { /* EQMN_STAT3 */ MENU_NONE, EQMN_STAT4, EQMN_STAT2,
                      { { 0x1000 + CMD_SX,        0, "" },
                        { 0x1000 + CMD_SX2,       0, "" },
                        { 0x1000 + CMD_SY,        0, "" },
                        { 0x1000 + CMD_SY2,       0, "" },
                        { 0x1000 + CMD_SXY,       0, "" },
                        { 0x1000 + CMD_SLNX,      0, "" } } },
    { /* EQMN_STAT4 */ MENU_NONE, EQMN_STAT1, EQMN_STAT3,
                      { { 0x1000 + CMD_SLNX2,     0, "" },
                        { 0x1000 + CMD_SLNY,      0, "" },
                        { 0x1000 + CMD_SLNY2,     0, "" },
                        { 0x1000 + CMD_SLNXLNY,   0, "" },
                        { 0x1000 + CMD_SXLNY,     0, "" },
                        { 0x1000 + CMD_SYLNX,     0, "" } } },
    { /* EQMN_FIN1 */ MENU_NONE, EQMN_FIN2, EQMN_FIN2,
                      { { 0x1000 + CMD_N,         0, "" },
                        { 0x1000 + CMD_I_PCT_YR,  0, "" },
                        { 0x1000 + CMD_PV,        0, "" },
                        { 0x1000 + CMD_PMT,       0, "" },
                        { 0x1000 + CMD_FV,        0, "" },
                        { 0x1000 + EQCMD_SIZEC,   0, "" } } },
    { /* EQMN_FIN2 */ MENU_NONE, EQMN_FIN1, EQMN_FIN1,
                      { { 0x1000 + CMD_SPPV,      0, "" },
                        { 0x1000 + CMD_SPFV,      0, "" },
                        { 0x1000 + CMD_USPV,      0, "" },
                        { 0x1000 + CMD_USFV,      0, "" },
                        { 0x1000 + EQCMD_FLOW,    0, "" },
                        { 0x1000 + EQCMD_NUM_T,   0, "" } } }
};

static const menu_spec *getmenu(int id) {
    if (id >= 1000)
        return eqn_menus + id - 1000;
    else
        return menus + id;
}

static short catalog[] = {
    CMD_ABS,      CMD_ACOS,    CMD_ACOSH,     CMD_AND,      EQCMD_ANGLE,   CMD_APPEND,
    CMD_ASIN,     CMD_ASINH,   CMD_ATAN,      CMD_ATANH,    CMD_BASEADD,   CMD_BASESUB,
    CMD_BASEMUL,  CMD_BASEDIV, CMD_BASECHS,   EQCMD_BREAK,  CMD_COMB,      EQCMD_CONTINUE,
    CMD_CORR,     CMD_COS,     CMD_COSH,      CMD_CPX_T,    CMD_CPXMAT_T,  CMD_CROSS,
    CMD_C_TO_N,   CMD_DATE,    CMD_DATE_PLUS, CMD_DDAYS,    CMD_DET,       CMD_DOT,
    CMD_EQN_T,    CMD_EVALN,   CMD_EXTEND,    CMD_E_POW_X,  CMD_E_POW_X_1, CMD_FCSTX,
    CMD_FCSTY,    EQCMD_FLOW,  CMD_FNRM,      EQCMD_FOR,    CMD_FP,        CMD_FV,
    CMD_GAMMA,    CMD_HEAD,    CMD_HMSADD,    CMD_HMSSUB,   EQCMD_IDIV,    CMD_IF_T,
    EQCMD_INT,    CMD_INVRT,   CMD_IP,        CMD_I_PCT_YR, CMD_LASTX,     CMD_LENGTH,
    CMD_LN,       CMD_LN_1_X,  CMD_LOG,       CMD_LIST_T,   CMD_MAT_T,     EQCMD_MAX,
    EQCMD_MEANX,  EQCMD_MEANY, EQCMD_MIN,     CMD_MOD,      EQCMD_MCOLS,   EQCMD_MROWS,
    CMD_N,        CMD_FACT,    CMD_NEWLIST,   CMD_NEWMAT,   CMD_NOT,       CMD_N_TO_C,
    CMD_N_TO_S,   CMD_NN_TO_S, CMD_OR,        CMD_PERM,     CMD_PCOMPLX,   CMD_PMT,
    CMD_POS,      CMD_PV,      EQCMD_RADIUS,  CMD_RAN,      CMD_RCOMPLX,   CMD_REAL_T,
    EQCMD_REGX,   EQCMD_REGY,  EQCMD_REGZ,    EQCMD_REGT,   CMD_REV,       CMD_RND,
    CMD_RNRM,     CMD_RSUM,    EQCMD_SDEVX,   EQCMD_SDEVY,  CMD_SEED,      EQCMD_SEQ,
    CMD_SIGN,     CMD_SIN,     CMD_SINH,      EQCMD_SIZEC,  EQCMD_SIZES,   CMD_SLOPE,
    CMD_SPFV,     CMD_SPPV,    CMD_SQRT,      EQCMD_STACK,  CMD_STOP,      CMD_STR_T,
    CMD_SUBSTR,   CMD_S_TO_N,  CMD_TAN,       CMD_TANH,     CMD_TIME,      CMD_TRANS,
    EQCMD_TRN,    CMD_TYPE_T,  EQCMD_NUM_T,   CMD_UNIT_T,   CMD_USFV,      CMD_USPV,
    CMD_UVEC,     CMD_VIEW,    CMD_WMEAN,     EQCMD_XCOORD, CMD_XEQ,       CMD_XOR,
    CMD_SQUARE,   EQCMD_TAIL,  EQCMD_YCOORD,  CMD_YINT,     CMD_Y_POW_X,   CMD_INV,
    CMD_10_POW_X, CMD_SX,      CMD_SX2,       CMD_SY,       CMD_SY2,       CMD_SXY,
    CMD_SN,       CMD_SLNX,    CMD_SLNX2,     CMD_SLNY,     CMD_SLNY2,     CMD_SLNXLNY,
    CMD_SXLNY,    CMD_SYLNX,   CMD_TO_DEC,    CMD_TO_DEG,   CMD_TO_HMS,    CMD_TO_HR,
    CMD_TO_OCT,   CMD_TO_RAD,  CMD_NULL,      CMD_NULL,     CMD_NULL,      CMD_NULL
};

static int catalog_rows = 26;


static void restart_cursor();

bool unpersist_eqn(int4 ver) {
    start_eqn_cursor = false;
    if (!read_bool(&active)) return false;
    if (!read_int(&menu_whence)) return false;
    bool have_eqns;
    if (!read_bool(&have_eqns)) return false;
    eqns = NULL;
    if (have_eqns) {
        vartype *v = recall_var("EQNS", 4);
        if (v != NULL && v->type == TYPE_LIST)
            eqns = (vartype_list *) v;
    }
    if (eqns != NULL)
        num_eqns = eqns->size;
    else
        num_eqns = 0;
    if (!read_int(&selected_row)) return false;
    if (!read_int(&edit_pos)) return false;
    if (!read_int(&display_pos)) return false;
    if (ver >= 13) {
        if (!read_int(&screen_row)) return false;
        if (!read_int(&headers)) return false;
    } else {
        screen_row = 0;
        headers = 0;
    }

    if (!read_int(&dialog)) return false;
    if (!read_int(&dialog_min)) return false;
    if (!read_int(&dialog_max)) return false;
    if (!read_int(&dialog_n)) return false;
    if (!read_int(&dialog_pos)) return false;
    if (!read_int(&dialog_cmd)) return false;

    if (!read_int(&edit.id)) return false;
    if (!read_int(&edit.catsect)) return false;
    if (!read_int(&edit.catsect_rows)) return false;
    if (!read_int(&edit.catalog_row)) return false;
    if (ver >= 22) {
        if (!read_bool(&edit.skip_top)) return false;
    } else {
        edit.skip_top = false;
    }
    if (!read_int(&prev_edit.id)) return false;
    if (!read_int(&prev_edit.catsect)) return false;
    if (!read_int(&prev_edit.catsect_rows)) return false;
    if (!read_int(&prev_edit.catalog_row)) return false;
    if (ver >= 22) {
        if (!read_bool(&prev_edit.skip_top)) return false;
    } else {
        prev_edit.skip_top = false;
    }
    if (!read_bool(&menu_sticky)) return false;
    for (int i = 0; i < 6; i++)
        if (!read_int(menu_item + i))
            return false;
    if (!read_bool(&new_eq)) return false;
    if (!read_int4(&edit_len)) return false;
    edit_capacity = edit_len;
    if (edit_buf != NULL)
        free(edit_buf);
    edit_buf = (char *) malloc(edit_len);
    if (edit_buf == NULL && edit_len != 0) {
        edit_capacity = edit_len = 0;
        return false;
    }
    if (fread(edit_buf, 1, edit_len, gfile) != edit_len) goto fail;
    if (!read_bool(&cursor_on)) goto fail;
    if (!read_int(&current_error)) return false;
    if (ver >= 20) {
        if (!unpersist_vartype(&current_result))
            return false;
    } else
        current_result = NULL;
    if (ver >= 25) {
        if (!read_int(&error_eqn_id)) return false;
        if (!read_int(&error_eqn_pos)) return false;
    } else {
        error_eqn_id = -1;
    }

    if (active && edit_pos != -1 && dialog == DIALOG_NONE)
        start_eqn_cursor = true;
    return true;

    fail:
    free(edit_buf);
    edit_buf = NULL;
    edit_capacity = edit_len = 0;
    return false;
}

bool persist_eqn() {
    if (!write_bool(active)) return false;
    if (!write_int(menu_whence)) return false;
    if (!write_bool(eqns != NULL)) return false;
    if (!write_int(selected_row)) return false;
    if (!write_int(edit_pos)) return false;
    if (!write_int(display_pos)) return false;
    if (!write_int(screen_row)) return false;
    if (!write_int(headers)) return false;
    if (!write_int(dialog)) return false;
    if (!write_int(dialog_min)) return false;
    if (!write_int(dialog_max)) return false;
    if (!write_int(dialog_n)) return false;
    if (!write_int(dialog_pos)) return false;
    if (!write_int(dialog_cmd)) return false;
    if (!write_int(edit.id)) return false;
    if (!write_int(edit.catsect)) return false;
    if (!write_int(edit.catsect_rows)) return false;
    if (!write_int(edit.catalog_row)) return false;
    if (!write_bool(edit.skip_top)) return false;
    if (!write_int(prev_edit.id)) return false;
    if (!write_int(prev_edit.catsect)) return false;
    if (!write_int(prev_edit.catsect_rows)) return false;
    if (!write_int(prev_edit.catalog_row)) return false;
    if (!write_bool(prev_edit.skip_top)) return false;
    if (!write_bool(menu_sticky)) return false;
    for (int i = 0; i < 6; i++)
        if (!write_int(menu_item[i]))
            return false;
    if (!write_bool(new_eq)) return false;
    if (!write_int(edit_len)) return false;
    if (fwrite(edit_buf, 1, edit_len, gfile) != edit_len) return false;
    if (!write_bool(cursor_on)) return false;
    if (!write_int(current_error)) return false;
    if (!persist_vartype(current_result)) return false;
    if (!write_int(error_eqn_id)) return false;
    if (!write_int(error_eqn_pos)) return false;
    return true;
}

void reset_eqn() {
    eqn_end();

    eqns = NULL;
    selected_row = -1;
    screen_row = 0;
    headers = 0;

    error_eqn_id = -1;

    dialog = DIALOG_NONE;
    free(edit_buf);
    edit_buf = NULL;
    current_error = ERR_NONE;
    free_vartype(current_result);
    current_result = NULL;

    timeout_action = 0;
    rep_key = -1;
}

#if 0
static bool is_name_char(char c) {
    /* The non-name characters are the same as on the 17B,
     * plus not-equal, less-equal, greater-equal, and
     * square brackets.
     */
    return c != '+' && c != '-' && c != 1 /* mul */
        && c != 0 /* div */     && c != '('
        && c != ')' && c != '<' && c != '>'
        && c != '^' && c != ':' && c != '=' && c != ' '
        && c != 12 /* NE */ && c != 9 /* LE */
        && c != 11 /* GE */ && c != '['
        && c != ']';
}
#endif

static void update_skin_mode() {
    shell_set_skin_mode(edit_pos == -1 ? 0 : flags.f.decimal_point ? 1 : 2);
}

static void show_error(int err) {
    current_error = err;
    eqn_draw();
}

static void restart_cursor() {
    timeout_action = 2;
    cursor_on = true;
    shell_request_timeout3(500);
}

void eqn_restart_cursor() {
    restart_cursor();
}

static int t_rep_key;
static int t_rep_count;

static bool insert_text(const char *text, int len, bool clear_mask_bit = false) {
    if (len == 1) {
        t_rep_count++;
        if (t_rep_count == 1)
            t_rep_key = 1024 + *text;
    }

    if (edit_len + len > edit_capacity) {
        int newcap = edit_len + len + 32;
        char *newbuf = (char *) realloc(edit_buf, newcap);
        if (newbuf == NULL) {
            show_error(ERR_INSUFFICIENT_MEMORY);
            return false;
        }
        edit_buf = newbuf;
        edit_capacity = newcap;
    }
    memmove(edit_buf + edit_pos + len, edit_buf + edit_pos, edit_len - edit_pos);
    if (clear_mask_bit)
        for (int i = 0; i < len; i++)
            edit_buf[edit_pos + i] = text[i] & 0x7f;
    else
        memcpy(edit_buf + edit_pos, text, len);
    edit_len += len;
    edit_pos += len;
    if (disp_r == 2) {
        while (edit_pos - display_pos > disp_c - 1)
            display_pos++;
        if (edit_pos == disp_c - 1 && edit_pos < edit_len - 1)
            display_pos++;
    } else {
        int maxlen = (disp_r - headers - 1) * disp_c;
        while (edit_pos >= display_pos + maxlen)
            display_pos += disp_c;
    }
    restart_cursor();
    eqn_draw();
    return true;
}

struct eqn_name_entry {
    int2 cmd;
    int2 len;
    const char *name;
};

/* Most built-ins are represented in equations using the same name
 * as in the RPN environment, with an opening parenthesis tacked on.
 * These functions deviate from that pattern:
 */
static eqn_name_entry eqn_name[] = {
    { CMD_Y_POW_X,   1, "^"        },
    { CMD_ADD,       1, "+"        },
    { CMD_SUB,       1, "-"        },
    { CMD_MUL,       1, "\001"     },
    { CMD_DIV,       1, "\000"     },
    { CMD_SIGMAADD,  2, "\005("    },
    { CMD_SIGMASUB,  2, "\003("    },
    { CMD_INV,       4, "INV("     },
    { CMD_SQUARE,    3, "SQ("      },
    { CMD_E_POW_X,   4, "EXP("     },
    { CMD_10_POW_X,  5, "ALOG("    },
    { CMD_E_POW_X_1, 6, "EXPM1("   },
    { CMD_LN_1_X,    5, "LNP1("    },
    { CMD_AND,       5, "BAND("    },
    { CMD_OR,        4, "BOR("     },
    { CMD_XOR,       5, "BXOR("    },
    { CMD_NOT,       5, "BNOT("    },
    { CMD_GEN_AND,   3, "AND"      },
    { CMD_GEN_OR,    2, "OR"       },
    { CMD_GEN_XOR,   3, "XOR"      },
    { CMD_GEN_NOT,   3, "NOT"      },
    { CMD_BASEADD,   5, "BADD("    },
    { CMD_BASESUB,   5, "BSUB("    },
    { CMD_BASEMUL,   5, "BMUL("    },
    { CMD_BASEDIV,   5, "BDIV("    },
    { CMD_BASECHS,   5, "BNEG("    },
    { CMD_DATE_PLUS, 5, "DATE("    },
    { CMD_HMSADD,    7, "HMSADD("  },
    { CMD_HMSSUB,    7, "HMSSUB("  },
    { CMD_FACT,      5, "FACT("    },
    { CMD_TO_DEG,    4, "DEG("     },
    { CMD_TO_RAD,    4, "RAD("     },
    { CMD_TO_HR,     4, "HRS("     },
    { CMD_TO_HMS,    4, "HMS("     },
    { CMD_TO_DEC,    4, "DEC("     },
    { CMD_TO_OCT,    4, "OCT("     },
    { CMD_SIGN,      4, "SGN("     },
    { CMD_DATE,      5, "CDATE"    },
    { CMD_TIME,      5, "CTIME"    },
    { CMD_RAN,       4, "RAN#"     },
    { CMD_GSTO,      2, "L("       },
    { CMD_GRCL,      2, "G("       },
    { CMD_SVAR,      2, "S("       },
    { CMD_IF_T,      3, "IF("      },
    { CMD_GETITEM,   5, "ITEM("    },
    { CMD_EVALN,     6, "EVALN("   },
    { CMD_XEQ,       4, "XEQ("     },
    { CMD_STOP,      5, "STOP("    },
    { CMD_RCOMPLX,   8, "RCOMPLX(" },
    { CMD_PCOMPLX,   8, "PCOMPLX(" },
    { CMD_N,         2, "N("       },
    { CMD_I_PCT_YR,  5, "I%YR("    },
    { CMD_PV,        3, "PV("      },
    { CMD_PMT,       4, "PMT("     },
    { CMD_FV,        3, "FV("      },
    { CMD_HEAD,      5, "HEAD("    },
    { CMD_VIEW,      5, "VIEW("    },
    { CMD_NULL,      0, NULL       }
};

/* Inserts a function, given by its command id, into the equation.
 * Only functions from our restricted catalog and our list of
 * special cases (the 'catalog' and 'eqn_name' arrays, above)
 * are allowed.
 */
static bool insert_function(int cmd) {
    if (cmd == CMD_NULL) {
        squeak();
        return false;
    }
    for (int i = 0; eqn_name[i].cmd != CMD_NULL; i++) {
        if (cmd == eqn_name[i].cmd) {
            if (cmd == CMD_GEN_AND || cmd == CMD_GEN_OR || cmd == CMD_GEN_XOR || cmd == CMD_GEN_NOT) {
                if (edit_pos > 0 && edit_buf[edit_pos - 1] != ' ')
                    if (!insert_text(" ", 1))
                        return false;
                if (!insert_text(eqn_name[i].name, eqn_name[i].len))
                    return false;
                if (edit_pos == edit_len || edit_buf[edit_pos] != ' ')
                    return insert_text(" ", 1);
            } else
                return insert_text(eqn_name[i].name, eqn_name[i].len);
        }
    }
    for (int i = 0; i < catalog_rows * 6; i++) {
        if (cmd == catalog[i]) {
            if (cmd >= 1000) {
                const eqn_cmd_spec *cs = eqn_cmds + cmd - 1000;
                return insert_text(cs->name, cs->namelen, true)
                    && (cs->no_args || insert_text("(", 1));
            } else {
                const command_spec *cs = cmd_array + cmd;
                return insert_text(cs->name, cs->name_length, true)
                    && (cs->argcount == 0 || insert_text("(", 1));
            }
        }
    }
    squeak();
    return false;
}

static void deleting_row(int row) {
    if (error_eqn_id == -1)
        return;
    vartype *v = eqns->array->data[row];
    if (v->type != TYPE_EQUATION)
        return;
    vartype_equation *eq = (vartype_equation *) v;
    if (eq->data->eqn_index == error_eqn_id)
        error_eqn_id = -1;
}

static void save() {
    if (eqns != NULL) {
        if (!disentangle((vartype *) eqns)) {
            nomem:
            show_error(ERR_INSUFFICIENT_MEMORY);
            return;
        }
    }
    int errpos;
    vartype *v = new_equation(edit_buf, edit_len, flags.f.eqn_compat, &errpos);
    if (v == NULL) {
        if (errpos == -1)
            goto nomem;
        v = new_string(edit_buf, edit_len);
        if (v == NULL)
            goto nomem;
    }
    if (new_eq) {
        if (num_eqns == 0) {
            eqns = (vartype_list *) new_list(1);
            if (eqns == NULL)
                goto nomem;
            eqns->array->data[0] = v;
            int err = store_root_var("EQNS", 4, (vartype *) eqns);
            if (err != ERR_NONE) {
                free_vartype((vartype *) eqns);
                eqns = NULL;
                show_error(err);
                return;
            }
            selected_row = 0;
            num_eqns = 1;
        } else {
            vartype **new_data = (vartype **) realloc(eqns->array->data, (num_eqns + 1) * sizeof(vartype *));
            if (new_data == NULL)
                goto nomem;
            eqns->array->data = new_data;
            eqns->size++;
            num_eqns++;
            selected_row++;
            if (selected_row == num_eqns)
                selected_row--;
            int n = num_eqns - selected_row - 1;
            if (n > 0)
                memmove(eqns->array->data + selected_row + 1,
                        eqns->array->data + selected_row,
                        n * sizeof(vartype *));
            eqns->array->data[selected_row] = v;
        }
    } else {
        deleting_row(selected_row);
        free_vartype(eqns->array->data[selected_row]);
        eqns->array->data[selected_row] = v;
    }
    free(edit_buf);
    edit_buf = NULL;
    edit_capacity = edit_len = 0;
    edit_pos = -1;
    update_skin_mode();
    edit.id = MENU_NONE;
    set_annunciators(0, -1, -1, -1, -1, -1);
    eqn_draw();
}

static int print_eq_row;
static bool print_eq_do_all;

static int print_eq_worker(bool interrupted) {
    if (interrupted) {
        set_annunciators(-1, -1, 0, -1, -1, -1);
        return ERR_STOP;
    }

    if (print_eq_do_all)
        print_text(NULL, 0, true);

    if (print_eq_do_all || edit_pos == -1) {
        vartype *v = eqns->array->data[print_eq_row];
        if (v->type == TYPE_STRING) {
            vartype_string *s = (vartype_string *) v;
            print_lines(s->txt(), s->length, 1);
        } else if (v->type == TYPE_EQUATION) {
            vartype_equation *eq = (vartype_equation *) v;
            equation_data *eqd = eq->data;
            print_lines(eqd->text, eqd->length, 1);
        } else {
            print_lines("<Invalid>", 9, 1);
        }
    } else {
        print_lines(edit_buf, edit_len, 1);
    }

    if (print_eq_do_all) {
        if (print_eq_row == num_eqns - 1)
            goto done;
        print_eq_row++;
        return ERR_INTERRUPTIBLE;
    } else {
        done:
        set_annunciators(-1, -1, 0, -1, -1, -1);
        return ERR_NONE;
    }
}

static void print_eq(bool all) {
    print_eq_row = all ? 0 : selected_row;
    print_eq_do_all = all;
    mode_interruptible = print_eq_worker;
    mode_stoppable = false;
    set_annunciators(-1, -1, 1, -1, -1, -1);
}

static void update_menu(int menuid, int catsect = -1, int rows = -1, int row = -1, bool skip_top = false) {
    edit.id = menuid;
    if (menuid != MENU_CATALOG) {
        int multirow = menuid != MENU_NONE && getmenu(edit.id)->next != MENU_NONE;
        set_annunciators(multirow, -1, -1, -1, -1, -1);
    } else {
        edit.catsect = catsect;
        edit.catsect_rows = rows;
        edit.catalog_row = row;
        edit.skip_top = skip_top;
        set_annunciators(rows > 1, -1, -1, -1, -1, -1);
    }
}

static void goto_prev_menu() {
    if (!menu_sticky) {
        update_menu(prev_edit.id, prev_edit.catsect, prev_edit.catsect_rows, prev_edit.catalog_row, prev_edit.skip_top);
        prev_edit.id = MENU_NONE;
    }
}

static void set_catsect(int sect) {
    if (edit.skip_top) {
        bool going_to_top = sect == CATSECT_TOP || sect == CATSECT_MORE;
        if (edit.catsect >= CATSECT_UNITS_1 && edit.catsect <= CATSECT_UNITS_VISC) {
            if (edit.catsect >= CATSECT_UNITS_1 && edit.catsect <= CATSECT_UNITS_3 && going_to_top) {
                menu_sticky = false;
                edit.catsect = CATSECT_TOP;
                goto_prev_menu();
                return;
            } else if (sect < CATSECT_UNITS_1 || sect > CATSECT_UNITS_VISC)
                edit.skip_top = false;
        } else
            edit.skip_top = false;
    }
    edit.catsect = sect;
}

static void set_catsect_no_top(int sect) {
    edit.catsect = sect;
    edit.skip_top = true;
}

int eqn_start(int whence) {
    active = true;
    menu_whence = whence;
    set_shift(false);

    vartype *v = recall_var("EQNS", 4);
    if (v == NULL) {
        eqns = NULL;
        num_eqns = 0;
    } else if (v->type == TYPE_REALMATRIX) {
        vartype_realmatrix *rm = (vartype_realmatrix *) v;
        int4 n = rm->rows * rm->columns;
        vartype_list *list = (vartype_list *) new_list(n);
        if (list == NULL) {
            active = false;
            return ERR_INSUFFICIENT_MEMORY;
        }
        for (int4 i = 0; i < n; i++) {
            vartype *s;
            if (rm->array->is_string[i]) {
                char *text;
                int len;
                get_matrix_string(rm, i, &text, &len);
                s = new_string(text, len);
            } else {
                char buf[50];
                int len = real2buf(buf, rm->array->data[i]);
                s = new_string(buf, len);
            }
            if (s == NULL) {
                active = false;
                free_vartype((vartype *) list);
                return ERR_INSUFFICIENT_MEMORY;
            }
            list->array->data[i] = s;
        }
        store_root_var("EQNS", 4, (vartype *) list);
        goto do_list;
    } else if (v->type != TYPE_LIST) {
        active = false;
        return ERR_INVALID_TYPE;
    } else {
        do_list:
        eqns = (vartype_list *) v;
        num_eqns = eqns->size;
    }
    if (selected_row > num_eqns)
        selected_row = num_eqns;
    edit_pos = -1;
    update_skin_mode();
    edit.id = MENU_NONE;
    set_annunciators(0, -1, -1, -1, -1, -1);
    eqn_draw();
    return ERR_NONE;
}

void eqn_end() {
    active = false;
    shell_set_skin_mode(0);
}

bool eqn_active() {
    return active;
}

bool eqn_alt_keys() {
    return active && edit_pos != -1;
}

bool eqn_editing() {
    return active && edit_pos != -1 && dialog == DIALOG_NONE;
}

char *eqn_copy() {
    textbuf tb;
    tb.buf = NULL;
    tb.size = 0;
    tb.capacity = 0;
    tb.fail = false;
    char buf[50];
    if (edit_pos != -1) {
        for (int4 i = 0; i < edit_len; i += 10) {
            int4 seg_len = edit_len - i;
            if (seg_len > 10)
                seg_len = 10;
            int4 bufptr = hp2ascii(buf, edit_buf + i, seg_len);
            tb_write(&tb, buf, bufptr);
        }
    } else {
        for (int4 i = 0; i < num_eqns; i++) {
            const char *text;
            int len;
            vartype *v = eqns->array->data[i];
            if (v->type == TYPE_STRING) {
                vartype_string *s = (vartype_string *) v;
                text = s->txt();
                len = s->length;
            } else if (v->type == TYPE_EQUATION) {
                vartype_equation *eq = (vartype_equation *) v;
                equation_data *eqd = eq->data;
                text = eqd->text;
                len = eqd->length;
            } else {
                text = "<Invalid>";
                len = 9;
            }
            for (int4 j = 0; j < len; j += 10) {
                int4 seg_len = len - j;
                if (seg_len > 10)
                    seg_len = 10;
                int4 bufptr = hp2ascii(buf, text + j, seg_len);
                tb_write(&tb, buf, bufptr);
            }
            tb_write(&tb, "\r\n", 2);
        }
    }
    tb_write_null(&tb);
    if (tb.fail) {
        show_error(ERR_INSUFFICIENT_MEMORY);
        free(tb.buf);
        return NULL;
    } else
        return tb.buf;
}

void eqn_paste(const char *buf) {
    if (edit_pos == -1) {
        if (num_eqns == 0 && !ensure_var_space(1)) {
            show_error(ERR_INSUFFICIENT_MEMORY);
            return;
        }
        int4 s = 0;
        while (buf[s] != 0) {
            int4 p = s;
            while (buf[s] != 0 && buf[s] != '\r' && buf[s] != '\n')
                s++;
            if (s == p) {
                if (buf[s] == 0)
                    break;
                else {
                    s++;
                    continue;
                }
            }
            int4 t = s - p;
            char *hpbuf = (char *) malloc(t + 4);
            if (hpbuf == NULL) {
                show_error(ERR_INSUFFICIENT_MEMORY);
                return;
            }
            int len = ascii2hp(hpbuf, t, buf + p, t);
            int errpos;
            vartype *v = new_equation(hpbuf, len, flags.f.eqn_compat, &errpos);
            if (v == NULL) {
                if (errpos == -1)
                    goto nomem;
                v = new_string(hpbuf, len);
                if (v == NULL)
                    goto nomem;
            }
            if (num_eqns == 0) {
                eqns = (vartype_list *) new_list(1);
                if (eqns == NULL) {
                    nomem:
                    show_error(ERR_INSUFFICIENT_MEMORY);
                    free(v);
                    free(hpbuf);
                    return;
                }
            } else {
                vartype **new_data = (vartype **) realloc(eqns->array->data, (num_eqns + 1) * sizeof(vartype *));
                if (new_data == NULL)
                    goto nomem;
                eqns->array->data = new_data;
                eqns->size++;
            }
            int n = selected_row + 1;
            if (n > num_eqns)
                n = num_eqns;
            memmove(eqns->array->data + n + 1, eqns->array->data + n, (num_eqns - n) * sizeof(vartype *));
            eqns->array->data[n] = v;
            free(hpbuf);
            if (num_eqns == 0)
                store_root_var("EQNS", 4, (vartype *) eqns);
            selected_row = n;
            num_eqns++;
            if (buf[s] != 0)
                s++;
        }
        eqn_draw();
    } else {
        int4 p = 0;
        while (buf[p] != 0 && buf[p] != '\r' && buf[p] != '\n')
            p++;
        char *hpbuf = (char *) malloc(p + 4);
        if (hpbuf == NULL) {
            show_error(ERR_INSUFFICIENT_MEMORY);
            return;
        }
        int len = ascii2hp(hpbuf, p, buf, p);
        insert_text(hpbuf, len);
        free(hpbuf);
    }
}

static void draw_print1_menu() {
    draw_key(0, 0, 0, "EQ", 2);
    draw_key(1, 0, 0, "LISTE", 5);
    draw_key(2, 0, 0, "VARS", 4);
    draw_key(3, 0, 0, "LISTV", 5);
    draw_key(4, 0, 0, "PRST", 4);
    draw_key(5, 0, 0, "ADV", 3);
}

static void draw_print2_menu() {
    if (flags.f.printer_exists) {
        draw_key(0, 0, 0, "PON\037", 4);
        draw_key(1, 0, 0, "POFF", 4);
    } else {
        draw_key(0, 0, 0, "PON", 3);
        draw_key(1, 0, 0, "POFF\037", 5);
    }
    if (!flags.f.trace_print && !flags.f.normal_print)
        draw_key(2, 0, 0, "MAN\037", 4);
    else
        draw_key(2, 0, 0, "MAN", 3);
    if (!flags.f.trace_print && flags.f.normal_print)
        draw_key(3, 0, 0, "NOR\037", 4);
    else
        draw_key(3, 0, 0, "NORM", 4);
    if (flags.f.trace_print && !flags.f.normal_print)
        draw_key(4, 0, 0, "TRAC\037", 5);
    else
        draw_key(4, 0, 0, "TRACE", 5);
    if (flags.f.trace_print && flags.f.normal_print)
        draw_key(5, 0, 0, "STRA\037", 5);
    else
        draw_key(5, 0, 0, "STRAC", 5);
}

void eqn_set_selected_row(int row) {
    screen_row += row - selected_row;
    if (screen_row < 0)
        screen_row = 0;
    selected_row = row;
}

static void draw_menu(bool highlight) {
    const menu_item_spec *mi = getmenu(edit.id)->child;
    for (int i = 0; i < 6; i++) {
        int id = mi[i].menuid;
        if (id == MENU_NONE || (id & 0x3000) != 0x1000) {
            draw_key(i, 0, 0, mi[i].title, mi[i].title_length);
        } else {
            id &= 0x0fff;
            if (id >= 1000) {
                draw_key(i, 0, 1, eqn_cmds[id - 1000].name, eqn_cmds[id - 1000].namelen);
            } else {
                bool hi = highlight && should_highlight(id);
                draw_key(i, hi, 1, cmd_array[id].name, cmd_array[id].name_length);
            }
        }
    }
}

static void draw_cursor(bool on) {
    int lines = disp_r - headers - 1;
    int maxlen = lines * disp_c;
    int len = edit_len - display_pos;
    if (len > maxlen)
        len = maxlen;
    int nl = (len + disp_c - 1) / disp_c;
    if (nl == 0)
        nl = 1;
    if (disp_r > 2 && edit_pos > 0 && edit_pos == edit_len && edit_pos % disp_c == 0)
        nl++;
    int cpos = edit_pos - display_pos;
    int cr = cpos / disp_c + lines + headers - nl;
    int cc = cpos % disp_c;
    if (on)
        draw_block(cc, cr);
    else if (edit_pos >= edit_len)
        draw_char(cc, cr, ' ');
    else
        draw_char(cc, cr, edit_buf[edit_pos]);
}

bool eqn_draw() {
    if (!active)
        return false;
    clear_display();
    if (current_error != ERR_NONE) {
        draw_string(0, 0, errors[current_error].text, errors[current_error].length);
        if (current_error == ERR_INVALID_EQUATION) {
            draw_eqn_menu:
            draw_key(0, 0, 0, "CALC", 4);
            draw_key(1, 0, 0, "EDIT", 4);
            draw_key(2, 0, 0, "DELET", 5);
            draw_key(3, 0, 0, "NEW", 3);
            draw_key(4, 0, 0, "\036", 1, true);
            draw_key(5, 0, 0, "\016", 1, true);
        } else
            draw_key(1, 0, 0, "OK", 2);
    } else if (dialog == DIALOG_SAVE_CONFIRM) {
        draw_string(0, 0, "Save this equation?", 19);
        draw_key(0, 0, 0, "YES", 3);
        draw_key(2, 0, 0, "NO", 2);
        draw_key(4, 0, 0, "EDIT", 4);
    } else if (dialog == DIALOG_DELETE_CONFIRM) {
        draw_string(0, 0, "Delete the equation?", 20);
        draw_key(1, 0, 0, "YES", 3);
        draw_key(5, 0, 0, "NO", 2);
    } else if (dialog == DIALOG_DELETE_BOTH_CONFIRM) {
        draw_string(0, 0, "Delete eqn and vars?", 20);
        draw_key(0, 0, 0, "BOTH", 4);
        draw_key(1, 0, 0, "EQN", 3);
        draw_key(3, 0, 0, "VARS", 4);
        draw_key(5, 0, 0, "NO", 2);
    } else if (dialog == DIALOG_RCL) {
        draw_string(0, 0, "Recall equation from:", 21);
        goto sto_rcl_keys;
    } else if (dialog == DIALOG_STO) {
        draw_string(0, 0, "Store equation to:", 18);
        sto_rcl_keys:
        draw_key(0, 0, 0, "X", 1);
        draw_key(1, 0, 0, "PRGM", 4);
        draw_key(2, 0, 0, "ALPHA", 5);
        draw_key(4, 0, 0, "CNCL", 4);
    } else if (dialog == DIALOG_STO_OVERWRITE_X
            || dialog == DIALOG_STO_OVERWRITE_PRGM
            || dialog == DIALOG_STO_OVERWRITE_ALPHA) {
        draw_string(0, 0, "Insert or overwrite?", 20);
        draw_key(0, 0, 0, "INSR", 4);
        draw_key(2, 0, 0, "OVER", 4);
        draw_key(4, 0, 0, "CNCL", 4);
    } else if (dialog == DIALOG_MODES) {
        const command_spec *cs = cmd_array + dialog_cmd;
        draw_string(0, 0, cs->name, cs->name_length);
        int w = dialog_cmd == CMD_SIZE ? 4 : 2;
        bool done = w == dialog_pos;
        if (done) {
            if (dialog_n < 100)
                w = dialog_pos = 2;
            else if (dialog_n < 1000)
                w = dialog_pos = 3;
        }
        int p = cs->name_length + 1;
        int n = dialog_n;
        for (int i = dialog_pos - 1; i >= 0; i--) {
            draw_char(p + i, 0, '0' + n % 10);
            n /= 10;
        }
        for (int i = dialog_pos; i < w; i++)
            draw_char(p + i, 0, '_');
        if (!done) {
            p += w + 1;
            char buf[16];
            int curr;
            if (dialog_cmd == CMD_SIZE) {
                vartype *regs = recall_var("REGS", 4);
                if (regs == NULL) {
                    curr = 0;
                } else if (regs->type == TYPE_REALMATRIX) {
                    vartype_realmatrix *rm = (vartype_realmatrix *) regs;
                    curr = rm->rows * rm->columns;
                } else if (regs->type == TYPE_COMPLEXMATRIX) {
                    vartype_complexmatrix *cm = (vartype_complexmatrix *) regs;
                    curr = cm->rows * cm->columns;
                } else {
                    curr = 0;
                }
            } else if (dialog_cmd == CMD_WSIZE) {
                curr = mode_wsize;
            } else {
                curr = (flags.f.digits_bit3 ? 8 : 0)
                     + (flags.f.digits_bit2 ? 4 : 0)
                     + (flags.f.digits_bit1 ? 2 : 0)
                     + (flags.f.digits_bit0 ? 1 : 0);
            }
            snprintf(buf, 16, "(Curr: %02d)", curr);
            draw_string(p, 0, buf, strlen(buf));
        }
        draw_menu(true);
    } else if (edit_pos == -1) {
        headers = display_header();
        int lines = disp_r - headers - 1;
        if (lines == 1 && selected_row < 0) {
            draw_string(0, 0, "<Top of List>", 13);
        } else if (lines == 1 && selected_row >= num_eqns) {
            draw_string(0, 0, "<Bottom of List>", 16);
        } else if (num_eqns == 0) {
            draw_string(1, disp_r - 2, "<Empty List>", 12);
        } else {
            if (selected_row < 0)
                selected_row = 0;
            else if (selected_row >= num_eqns)
                selected_row = num_eqns - 1;
            if (screen_row >= lines)
                screen_row = lines - 1;
            if (screen_row + num_eqns - selected_row < lines)
                screen_row = lines - num_eqns + selected_row;
            for (int i = 0; i < lines; i++) {
                int n = i + selected_row - screen_row;
                if (n < 0)
                    continue;
                const char *text;
                int4 len;
                vartype *v = eqns->array->data[n];
                if (v->type == TYPE_STRING) {
                    vartype_string *s = (vartype_string *) v;
                    text = s->txt();
                    len = s->length;
                } else if (v->type == TYPE_EQUATION) {
                    vartype_equation *eq = (vartype_equation *) v;
                    equation_data *eqd = eq->data;
                    text = eqd->text;
                    len = eqd->length;
                } else {
                    text = "<Invalid>";
                    len = 9;
                }
                int p = lines > 1;
                int w = disp_c - p;
                int r = i + headers;
                if (p && i == screen_row)
                    draw_char(0, r, 6);
                if (len <= w) {
                    draw_string(p, r, text, len);
                } else {
                    draw_string(p, r, text, w - 1);
                    draw_char(disp_c - 1, r, 26);
                }
            }
        }
        if (edit.id == MENU_PRINT1) {
            draw_print1_menu();
        } else if (edit.id == MENU_PRINT2) {
            draw_print2_menu();
        } else if (edit.id >= MENU_MODES1 && edit.id <= MENU_MODES5
                || edit.id == MENU_DISP1 || edit.id == MENU_DISP2) {
            draw_menu(true);
        } else {
            goto draw_eqn_menu;
        }
    } else {
        headers = display_header();
        int lines = disp_r - headers - 1;
        int maxlen = lines * disp_c;
        int len = edit_len - display_pos;
        bool start_ellipsis = display_pos > 0;
        bool end_ellipsis = len > maxlen;
        if (end_ellipsis)
            len = maxlen;
        int nl = (len + disp_c - 1) / disp_c;
        if (nl == 0)
            nl = 1;
        if (disp_r > 2 && edit_pos > 0 && edit_pos == edit_len && edit_pos % disp_c == 0)
            nl++;
        int pos = 0;
        for (int n = 0; n < nl; n++) {
            int seg = len - pos;
            if (seg > disp_c)
                seg = disp_c;
            int b = n == 0 && start_ellipsis;
            int e = n == nl - 1 && end_ellipsis;
            int r = lines - nl + n + headers;
            if (b)
                draw_char(0, r, 26);
            draw_string(b, r, edit_buf + display_pos + pos + b, seg - b - e);
            if (e)
                draw_char(disp_c - 1, r, 26);
            pos += seg;
        }
        if (cursor_on)
            draw_cursor(true);
        if (edit.id == MENU_NONE) {
            draw_key(0, 0, 0, "DEL", 3);
            if (disp_r == 2) {
                draw_key(1, 0, 0, "<\020", 2);
                draw_key(2, 0, 0, "\020", 1);
                draw_key(3, 0, 0, "\017", 1);
                draw_key(4, 0, 0, "\017>", 2);
            } else {
                draw_key(1, 0, 0, "\020", 1);
                draw_key(2, 0, 0, "\036", 1);
                draw_key(3, 0, 0, "\016", 1);
                draw_key(4, 0, 0, "\017", 1);
            }
            draw_key(5, 0, 0, "ALPHA", 5);
        } else if (edit.id == MENU_PRINT1) {
            draw_print1_menu();
        } else if (edit.id == MENU_PRINT2) {
            draw_print2_menu();
        } else if (edit.id >= MENU_CUSTOM1 && edit.id <= MENU_CUSTOM3) {
            int row = edit.id - MENU_CUSTOM1;
            for (int k = 0; k < 6; k++) {
                char label[7];
                int len;
                get_custom_key(row * 6 + k + 1, label, &len);
                draw_key(k, 0, 1, label, len);
            }
        } else if (edit.id == MENU_CATALOG && edit.catsect == CATSECT_FCN) {
            for (int k = 0; k < 6; k++) {
                int cmd = catalog[edit.catalog_row * 6 + k];
                if (cmd >= 1000)
                    draw_key(k, 0, 1, eqn_cmds[cmd - 1000].name, eqn_cmds[cmd - 1000].namelen);
                else
                    draw_key(k, 0, 1, cmd_array[cmd].name, cmd_array[cmd].name_length);
            }
            edit.catsect_rows = catalog_rows;
            set_annunciators(1, -1, -1, -1, -1, -1);
        } else if (edit.id == MENU_CATALOG) {
            edit.catsect_rows = draw_eqn_catalog(edit.catsect, edit.catalog_row, menu_item);
            if (edit.catalog_row >= edit.catsect_rows)
                edit.catalog_row = edit.catsect_rows - 1;
        } else {
            draw_menu(false);
        }
    }

    if (current_result != NULL) {
        int maxlen = (disp_r - 1) * disp_c;
        char *buf = (char *) malloc(maxlen);
        if (buf == NULL) {
            clear_row(0);
            draw_string(0, 0, "RES=<Low Mem>", 13);
        } else {
            int pos = 0;
            string2buf(buf, maxlen, &pos, "RES=", 4);
            pos += vartype2string(current_result, buf + pos, maxlen - pos);
            int rows = (pos + disp_c - 1) / disp_c;
            for (int i = 0; i < rows; i++) {
                int b = i * disp_c;
                int e = b + disp_c - 1;
                if (e > pos)
                    e = pos;
                clear_row(i);
                draw_string(0, i, buf + b, e - b);
            }
            free(buf);
        }
    }

    flush_display();
    return true;
}

static int keydown_list(int key, bool shift, int *repeat);
static int keydown_edit(int key, bool shift, int *repeat);
static int keydown_print1(int key, bool shift, int *repeat);
static int keydown_print2(int key, bool shift, int *repeat);
static int keydown_modes_number(int key, int shift, int *repeat);
static int keydown_modes(int key, bool shift, int *repeat);
static int keydown_error(int key, bool shift, int *repeat);
static int keydown_save_confirmation(int key, bool shift, int *repeat);
static int keydown_delete_confirmation(int key, bool shift, int *repeat);
static int keydown_delete_both_confirmation(int key, bool shift, int *repeat);
static int keydown_rcl(int key, bool shift, int *repeat);
static int keydown_sto(int key, bool shift, int *repeat);
static int keydown_sto_overwrite(int key, bool shift, int *repeat);

/* eqn_keydown() return values:
 * 0: equation editor not active; caller should perform normal event processing
 * 1: equation editor active
 * 2: equation editor active; caller should NOT suppress key timeouts
 *    (this mode is for when EQNSLVi, EQNINTi, or PMEXEC are being performed,
 *     i.e., when the CALC menu key in the list view has been pressed)
 * 3: equation editor active but busy; request CPU
 */
int eqn_keydown(int key, int *repeat) {
    if (!active)
        return 0;

    bool shift = false;
    if (mode_interruptible == NULL) {
        if (key == 0)
            return 1;
        if (key == KEY_SHIFT) {
            set_shift(!mode_shift);
            return 1;
        }
        shift = mode_shift;
        set_shift(false);
    } else {
        // Used to make print functions EQ, LISTE, and LISTV interruptible
        if (key == KEY_SHIFT) {
            set_shift(!mode_shift);
        } else if (key != 0) {
            shift = mode_shift;
            set_shift(false);
        }
        if (key == KEY_EXIT) {
            mode_interruptible(true);
            mode_interruptible = NULL;
            return 1;
        } else {
            int err = mode_interruptible(false);
            if (err == ERR_INTERRUPTIBLE) {
                if (key != 0 && key != KEY_SHIFT)
                    squeak();
                return 3;
            }
            mode_interruptible = NULL;
            // Continue normal key event processing...
            if (key == 0 || key == KEY_SHIFT)
                return 1;
        }
    }

    if (current_result != NULL) {
        free_vartype(current_result);
        current_result = NULL;
        if (key == KEY_BSP && !shift) {
            eqn_draw();
            return 1;
        }
    }

    if (current_error != ERR_NONE)
        return keydown_error(key, shift, repeat);
    else if (dialog == DIALOG_SAVE_CONFIRM)
        return keydown_save_confirmation(key, shift, repeat);
    else if (dialog == DIALOG_DELETE_CONFIRM)
        return keydown_delete_confirmation(key, shift, repeat);
    else if (dialog == DIALOG_DELETE_BOTH_CONFIRM)
        return keydown_delete_both_confirmation(key, shift, repeat);
    else if (dialog == DIALOG_RCL)
        return keydown_rcl(key, shift, repeat);
    else if (dialog == DIALOG_STO)
        return keydown_sto(key, shift, repeat);
    else if (dialog == DIALOG_STO_OVERWRITE_X
            || dialog == DIALOG_STO_OVERWRITE_PRGM
            || dialog == DIALOG_STO_OVERWRITE_ALPHA)
        return keydown_sto_overwrite(key, shift, repeat);
    else if (dialog == DIALOG_MODES)
        return keydown_modes_number(key, shift, repeat);
    else if (edit.id == MENU_PRINT1)
        return keydown_print1(key, shift, repeat);
    else if (edit.id == MENU_PRINT2)
        return keydown_print2(key, shift, repeat);
    else if (edit.id >= MENU_MODES1 && edit.id <= MENU_MODES5
            || edit.id == MENU_DISP1 || edit.id == MENU_DISP2)
        return keydown_modes(key, shift, repeat);
    else if (edit_pos == -1)
        return keydown_list(key, shift, repeat);
    else
        return keydown_edit(key, shift, repeat);
}

static int keydown_print1(int key, bool shift, int *repeat) {
    arg_struct arg;
    switch (key) {
        case KEY_SIGMA: {
            /* EQ */
            if (flags.f.printer_exists) {
                if (selected_row == -1 || selected_row == num_eqns) {
                    squeak();
                    return 1;
                } else {
                    print_eq(false);
                    return mode_interruptible == NULL ? 1 : 3;
                }
            } else {
                show_error(ERR_PRINTING_IS_DISABLED);
                return 1;
            }
        }
        case KEY_INV: {
            /* LISTE */
            if (flags.f.printer_exists) {
                if (num_eqns == 0) {
                    squeak();
                    return 1;
                } else {
                    print_eq(true);
                    return mode_interruptible == NULL ? 1 : 3;
                }
            } else {
                show_error(ERR_PRINTING_IS_DISABLED);
                return 1;
            }
        }
        case KEY_SQRT: {
            /* VARS */
            if (flags.f.printer_exists) {
                if (selected_row == -1 || selected_row == num_eqns) {
                    squeak();
                    return 1;
                } else {
                    vartype *v = eqns->array->data[selected_row];
                    if (v->type != TYPE_EQUATION) {
                        squeak();
                        return 1;
                    }
                    vartype *saved_lastx = lastx;
                    lastx = v;
                    arg.type = ARGTYPE_STK;
                    arg.val.stk = 'L';
                    docmd_eqnvar(&arg);
                    lastx = saved_lastx;
                    return mode_interruptible == NULL ? 1 : 3;
                }
            } else {
                show_error(ERR_PRINTING_IS_DISABLED);
                return 1;
            }
        }
        case KEY_LOG: {
            /* LISTV */
            if (flags.f.printer_exists) {
                docmd_prusr(NULL);
                return mode_interruptible == NULL ? 1 : 3;
            } else {
                show_error(ERR_PRINTING_IS_DISABLED);
                return 1;
            }
        }
        case KEY_LN: {
            /* PRSTK */
            if (flags.f.printer_exists) {
                docmd_prstk(&arg);
                return mode_interruptible == NULL ? 1 : 3;
            } else {
                show_error(ERR_PRINTING_IS_DISABLED);
                return 1;
            }
        }
        case KEY_XEQ: {
            /* ADV */
            docmd_adv(NULL);
            return 1;
        }
        case KEY_UP:
        case KEY_DOWN: {
            edit.id = MENU_PRINT2;
            eqn_draw();
            return 1;
        }
        case KEY_EXIT: {
            if (shift) {
                docmd_off(NULL);
            } else {
                goto_prev_menu();
                if (edit_pos != -1)
                    restart_cursor();
                eqn_draw();
            }
            return mode_interruptible == NULL ? 1 : 3;
        }
        default: {
            squeak();
            return 1;
        }
    }
}

static int keydown_print2(int key, bool shift, int *repeat) {
    switch (key) {
        case KEY_SIGMA: {
            /* PRON */
            flags.f.printer_exists = true;
            break;
        }
        case KEY_INV: {
            /* PROFF */
            flags.f.printer_exists = false;
            break;
        }
        case KEY_SQRT: {
            /* MAN */
            flags.f.trace_print = false;
            flags.f.normal_print = false;
            break;
        }
        case KEY_LOG: {
            /* NORM */
            flags.f.trace_print = false;
            flags.f.normal_print = true;
            break;
        }
        case KEY_LN: {
            /* TRACE */
            flags.f.trace_print = true;
            flags.f.normal_print = false;
            break;
        }
        case KEY_XEQ: {
            /* STRACE */
            flags.f.trace_print = true;
            flags.f.normal_print = true;
            break;
        }
        case KEY_UP:
        case KEY_DOWN: {
            edit.id = MENU_PRINT1;
            break;
        }
        case KEY_EXIT: {
            if (shift) {
                docmd_off(NULL);
                return 1;
            } else {
                goto_prev_menu();
                if (edit_pos != -1)
                    restart_cursor();
                eqn_draw();
                return 1;
            }
        }
        default: {
            squeak();
            return 1;
        }
    }

    eqn_draw();
    return 1;
}

static int keydown_modes(int key, bool shift, int *repeat) {
    switch (key) {
        case KEY_SIGMA:
        case KEY_INV:
        case KEY_SQRT:
        case KEY_LN:
        case KEY_LOG:
        case KEY_XEQ: {
            int cmd = menus[edit.id].child[key - 1].menuid & 0x0fff;
            if (cmd == CMD_NULL) {
                squeak();
            } else if (cmd == CMD_WSIZE_T) {
                char buf[23];
                snprintf(buf, 23, "WSIZE = %02d", mode_wsize);
                clear_row(0);
                draw_string(0, 0, buf, strlen(buf));
                flush_display();
                timeout_action = 1;
                shell_request_timeout3(2000);
                return 1;
            } else if (cmd == CMD_FIX || cmd == CMD_SCI || cmd == CMD_ENG || cmd == CMD_SIZE || cmd == CMD_WSIZE) {
                if (cmd == CMD_FIX || cmd == CMD_SCI || cmd == CMD_ENG) {
                    dialog_min = 0;
                    dialog_max = 11;
                } else if (cmd == CMD_SIZE) {
                    dialog_min = 0;
                    dialog_max = 9999;
                } else if (cmd == CMD_WSIZE) {
                    dialog_min = 1;
#ifdef BCD_MATH
                    dialog_max = 64;
#else
                    dialog_max = 53;
#endif
                }
                dialog_n = 0;
                dialog_pos = 0;
                dialog_cmd = cmd;
                dialog = DIALOG_MODES;
                eqn_draw();
            } else {
                arg_struct arg;
                arg.type = ARGTYPE_NONE;
                cmd_array[cmd].handler(&arg);
                eqn_draw();
            }
            break;
        }
        case KEY_UP: {
            int m = getmenu(edit.id)->prev;
            if (m != MENU_NONE) {
                update_menu(m);
                *repeat = 1;
                eqn_draw();
            }
            break;
        }
        case KEY_DOWN: {
            int m = getmenu(edit.id)->next;
            if (m != MENU_NONE) {
                update_menu(m);
                *repeat = 1;
                eqn_draw();
            }
            break;
        }
        case KEY_CHS: {
            if (shift)
                // TODO: stickiness
                // (figure out stickiness for PRINT menu, too)
                if (edit.id == MENU_DISP1 || edit.id == MENU_DISP2) {
                    edit.id = MENU_MODES1;
                    eqn_draw();
                }
            else
                squeak();
            break;
        }
        case KEY_E: {
            if (shift)
                // TODO: stickiness
                if (edit.id >= MENU_MODES1 && edit.id <= MENU_MODES5) {
                    edit.id = MENU_DISP1;
                    eqn_draw();
                }
            else
                squeak();
            break;
        }
        case KEY_EXIT: {
            if (shift) {
                docmd_off(NULL);
            } else {
                edit.id = MENU_NONE;
                eqn_draw();
            }
            break;
        }
        default:
            squeak();
            break;
    }
    return 1;
}

static int keydown_error(int key, bool shift, int *repeat) {
    if (shift && key == KEY_EXIT) {
        docmd_off(NULL);
    } else {
        show_error(ERR_NONE);
        restart_cursor();
    }
    return 1;
}

static int keydown_save_confirmation(int key, bool shift, int *repeat) {
    switch (key) {
        case KEY_SIGMA: {
            /* YES */
            if (edit_len == 0) {
                squeak();
                break;
            }
            dialog = DIALOG_NONE;
            save();
            break;
        }
        case KEY_EXIT: {
            if (shift) {
                docmd_off(NULL);
                break;
            }
            /* Fall through */
        }
        case KEY_SQRT: {
            /* NO */
            free(edit_buf);
            edit_buf = NULL;
            edit_capacity = edit_len = 0;
            edit_pos = -1;
            update_skin_mode();
            edit.id = MENU_NONE;
            dialog = DIALOG_NONE;
            eqn_draw();
            break;
        }
        case KEY_LN: {
            /* EDIT */
            dialog = DIALOG_NONE;
            restart_cursor();
            eqn_draw();
            break;
        }
        default: {
            squeak();
            break;
        }
    }
    return 1;
}

static bool delete_eqn() {
    if (!disentangle((vartype *) eqns)) {
        show_error(ERR_INSUFFICIENT_MEMORY);
        return false;
    }
    deleting_row(selected_row);
    free_vartype(eqns->array->data[selected_row]);
    memmove(eqns->array->data + selected_row,
            eqns->array->data + selected_row + 1,
            (num_eqns - selected_row - 1) * sizeof(vartype *));
    num_eqns--;
    eqns->size--;
    if (disp_r > 2) {
        if (selected_row == num_eqns) {
            selected_row = num_eqns - 1;
            screen_row--;
        }
    }
    vartype **new_data = (vartype **) realloc(eqns->array->data, num_eqns * sizeof(vartype *));
    if (new_data != NULL || num_eqns == 0)
        eqns->array->data = new_data;
    return true;
}

static int keydown_delete_confirmation(int key, bool shift, int *repeat) {
    switch (key) {
        case KEY_INV: {
            /* YES */
            if (delete_eqn())
                goto finish;
            else
                break;
        }
        case KEY_EXIT: {
            if (shift) {
                docmd_off(NULL);
                break;
            }
            /* Fall through */
        }
        case KEY_XEQ: {
            /* NO */
            finish:
            dialog = DIALOG_NONE;
            eqn_draw();
            break;
        }
        default: {
            squeak();
            break;
        }
    }
    return 1;
}

static void delete_vars() {
    vartype *v = eqns->array->data[selected_row];
    if (v->type != TYPE_EQUATION)
        return;
    vartype_equation *eq = (vartype_equation *) v;
    equation_data *eqd = eq->data;
    std::vector<std::string> params = get_parameters(eqd);
    for (int i = 0; i < params.size(); i++) {
        std::string s = params[i];
        purge_var(s.c_str(), s.length());
    }
}

static int keydown_delete_both_confirmation(int key, bool shift, int *repeat) {
    switch (key) {
        case KEY_SIGMA: {
            /* BOTH */
            delete_vars();
            /* Fall through */
        }
        case KEY_INV: {
            /* EQN */
            if (delete_eqn())
                goto finish;
            else
                break;
        }
        case KEY_LOG: {
            /* VARS */
            delete_vars();
            goto finish;
        }
        case KEY_EXIT: {
            if (shift) {
                docmd_off(NULL);
                break;
            }
            /* Fall through */
        }
        case KEY_XEQ: {
            /* NO */
            finish:
            dialog = DIALOG_NONE;
            eqn_draw();
            break;
        }
        default: {
            squeak();
            break;
        }
    }
    return 1;
}

static int keydown_rcl(int key, bool shift, int *repeat) {
    switch (key) {
        case KEY_SIGMA: {
            /* X */
            if (sp == -1 || stack[sp]->type != TYPE_STRING
                         && stack[sp]->type != TYPE_EQUATION) {
                nope:
                squeak();
            } else {
                char *text;
                int length;
                if (stack[sp]->type == TYPE_STRING) {
                    vartype_string *s = (vartype_string *) stack[sp];
                    text = s->txt();
                    length = s->length;
                } else {
                    vartype_equation *eq = (vartype_equation *) stack[sp];
                    equation_data *eqd = eq->data;
                    text = eqd->text;
                    length = eqd->length;
                }
                if (length == 0)
                    goto nope;
                edit_buf = (char *) malloc(length);
                if (edit_buf == NULL) {
                    edit_capacity = edit_len = 0;
                    show_error(ERR_INSUFFICIENT_MEMORY);
                } else {
                    memcpy(edit_buf, text, length);
                    edit_capacity = edit_len = length;
                    goto store;
                }
            }
            break;
        }
        case KEY_INV: {
            /* PRGM */
            int4 oldpc = pc;
            int cmd;
            arg_struct arg;
            get_next_command(&pc, &cmd, &arg, 0, NULL);
            pc = oldpc;
            if (cmd != CMD_XSTR || arg.length == 0) {
                squeak();
            } else {
                edit_buf = (char *) malloc(arg.length);
                if (edit_buf == NULL) {
                    edit_capacity = edit_len = 0;
                    show_error(ERR_INSUFFICIENT_MEMORY);
                } else {
                    memcpy(edit_buf, arg.val.xstr, arg.length);
                    edit_capacity = edit_len = arg.length;
                    goto store;
                }
            }
            break;
        }
        case KEY_SQRT: {
            /* ALPHA */
            if (reg_alpha_length == 0) {
                squeak();
            } else {
                edit_buf = (char *) malloc(reg_alpha_length);
                if (edit_buf == NULL) {
                    edit_capacity = edit_len = 0;
                    show_error(ERR_INSUFFICIENT_MEMORY);
                } else {
                    memcpy(edit_buf, reg_alpha, reg_alpha_length);
                    edit_capacity = edit_len = reg_alpha_length;
                    store:
                    edit_pos = 0;
                    new_eq = true;
                    save();
                    if (edit_pos == 0) {
                        free(edit_buf);
                        edit_buf = NULL;
                        edit_capacity = edit_len = 0;
                        edit_pos = -1;
                        edit.id = MENU_NONE;
                    } else {
                        dialog = DIALOG_NONE;
                        eqn_draw();
                    }
                }
                update_skin_mode();
            }
            break;
        }
        case KEY_LN:
        case KEY_EXIT: {
            /* CNCL */
            dialog = DIALOG_NONE;
            eqn_draw();
            break;
        }
    }
    return 1;
}

static bool get_equation() {
    const char *text;
    int len;
    vartype *v = eqns->array->data[selected_row];
    if (v->type == TYPE_STRING) {
        vartype_string *s = (vartype_string *) v;
        text = s->txt();
        len = s->length;
    } else if (v->type == TYPE_EQUATION) {
        vartype_equation *eq = (vartype_equation *) v;
        equation_data *eqd = eq->data;
        text = eqd->text;
        len = eqd->length;
    } else {
        text = "<Invalid>";
        len = 9;
    }
    edit_buf = (char *) malloc(len);
    if (edit_buf == NULL && len != 0) {
        edit_capacity = edit_len = 0;
        return false;
    }
    edit_len = edit_capacity = len;
    memcpy(edit_buf, text, len);
    return true;
}

static int keydown_sto(int key, bool shift, int *repeat) {
    switch (key) {
        case KEY_SIGMA: {
            /* X */
            dialog = DIALOG_STO_OVERWRITE_X;
            if (sp != -1 && stack[sp]->type == TYPE_STRING)
                goto done;
            else
                return keydown_sto_overwrite(KEY_SIGMA, false, NULL);
        }
        case KEY_INV: {
            /* PRGM */
            dialog = DIALOG_STO_OVERWRITE_PRGM;
            int4 oldpc = pc;
            int cmd;
            arg_struct arg;
            get_next_command(&pc, &cmd, &arg, 0, NULL);
            pc = oldpc;
            if (cmd == CMD_XSTR)
                goto done;
            else
                return keydown_sto_overwrite(KEY_SIGMA, false, NULL);
        }
        case KEY_SQRT: {
            /* ALPHA */
            dialog = DIALOG_STO_OVERWRITE_ALPHA;
            if (reg_alpha_length > 0)
                goto done;
            else
                return keydown_sto_overwrite(KEY_SIGMA, false, NULL);
        }
        case KEY_LN:
        case KEY_EXIT: {
            /* CNCL */
            dialog = DIALOG_NONE;
            done:
            eqn_draw();
            break;
        }
    }
    return 1;
}

static int keydown_sto_overwrite(int key, bool shift, int *repeat) {
    switch (key) {
        case KEY_SIGMA: /* INSR */
        case KEY_SQRT: /* OVER */ {
            if (dialog != DIALOG_STO_OVERWRITE_X && !get_equation()) {
                nomem:
                show_error(ERR_INSUFFICIENT_MEMORY);
                return 1;
            }
            switch (dialog) {
                case DIALOG_STO_OVERWRITE_X: {
                    vartype *v = dup_vartype(eqns->array->data[selected_row]);
                    if (v == NULL)
                        goto nomem;
                    bool sld = flags.f.stack_lift_disable;
                    flags.f.stack_lift_disable = 0;
                    if (key == KEY_SIGMA) {
                        int err = recall_result_silently(v);
                        if (err != ERR_NONE) {
                            flags.f.stack_lift_disable = sld;
                            free_vartype(v);
                            goto nomem;
                        }
                    } else {
                        free_vartype(stack[sp]);
                        stack[sp] = v;
                    }
                    break;
                }
                case DIALOG_STO_OVERWRITE_PRGM: {
                    if (!current_prgm.is_editable()) {
                        show_error(ERR_RESTRICTED_OPERATION);
                        return 1;
                    }
                    arg_struct arg;
                    arg.type = ARGTYPE_XSTR;
                    arg.length = edit_len > 65535 ? 65535 : edit_len;
                    arg.val.xstr = edit_buf;
                    if (key == KEY_SIGMA) {
                        store_command_after(&pc, CMD_XSTR, &arg, NULL);
                    } else {
                        delete_command(pc);
                        store_command(pc, CMD_XSTR, &arg, NULL);
                    }
                    break;
                }
                case DIALOG_STO_OVERWRITE_ALPHA: {
                    char *ptr = edit_buf;
                    int len = edit_len;
                    if (len > 44) {
                        len = 44;
                        ptr += edit_len - 44;
                    }
                    if (key == KEY_SIGMA) {
                        if (reg_alpha_length + len > 44) {
                            int excess = reg_alpha_length + len - 44;
                            memmove(reg_alpha, reg_alpha + excess, reg_alpha_length - excess);
                            reg_alpha_length -= excess;
                        }
                    } else {
                        reg_alpha_length = 0;
                    }
                    memcpy(reg_alpha + reg_alpha_length, ptr, len);
                    reg_alpha_length += len;
                    break;
                }
            }
            free(edit_buf);
            edit_buf = NULL;
            edit_capacity = edit_len = 0;
            goto done;
        }
        case KEY_LN:
        case KEY_EXIT: {
            /* CNCL */
            done:
            dialog = DIALOG_NONE;
            eqn_draw();
            break;
        }
    }
    return 1;
}

static int keydown_modes_number(int key, int shift, int *repeat) {
    if (shift) {
        if (key == KEY_EXIT)
            docmd_off(NULL);
        else
            squeak();
        return 1;
    }

    int d = 0;
    switch (key) {
        case KEY_9: d++;
        case KEY_8: d++;
        case KEY_7: d++;
        case KEY_6: d++;
        case KEY_5: d++;
        case KEY_4: d++;
        case KEY_3: d++;
        case KEY_2: d++;
        case KEY_1: d++;
        case KEY_0: {
            dialog_n = dialog_n * 10 + d;
            if (dialog_n > dialog_max)
                dialog_n = dialog_max;
            dialog_pos++;
            int w = dialog_cmd == CMD_SIZE ? 4 : 2;
            if (dialog_pos == w && dialog_cmd == CMD_WSIZE && dialog_n == 0) {
                dialog_pos--;
                squeak();
                return 1;
            }
            bool done = dialog_pos == w;
            eqn_draw();
            if (done) {
                shell_request_timeout3(250);
                timeout_action = 1;
                goto finish;
            }
            return 1;
        }
        case KEY_BSP: {
            if (dialog_pos == 0) {
                dialog = DIALOG_NONE;
            } else {
                dialog_n /= 10;
                dialog_pos--;
            }
            eqn_draw();
            return 1;
        }
        case KEY_ENTER: {
            if (dialog_n < dialog_min) {
                squeak();
                return 1;
            }
            dialog_pos = dialog_cmd == CMD_SIZE ? 4 : 2;
            eqn_draw();
            finish:
            if (dialog_cmd == CMD_SIZE) {
                arg_struct arg;
                arg.type = ARGTYPE_NUM;
                arg.val.num = dialog_n;
                int err = docmd_size(&arg);
                if (err != ERR_NONE) {
                    show_error(err);
                    dialog_n = 0;
                    dialog_pos = 0;
                    return 1;
                }
            } else if (dialog_cmd == CMD_WSIZE) {
                mode_wsize = dialog_n;
            } else if (dialog_cmd == CMD_FIX) {
                flags.f.fix_or_all = 1;
                flags.f.eng_or_all = 0;
                goto set_digits;
            } else if (dialog_cmd == CMD_SCI) {
                flags.f.fix_or_all = 0;
                flags.f.eng_or_all = 0;
                goto set_digits;
            } else if (dialog_cmd == CMD_ENG) {
                flags.f.fix_or_all = 0;
                flags.f.eng_or_all = 1;
                set_digits:
                flags.f.digits_bit3 = (dialog_n & 8) != 0;
                flags.f.digits_bit2 = (dialog_n & 4) != 0;
                flags.f.digits_bit1 = (dialog_n & 2) != 0;
                flags.f.digits_bit0 = (dialog_n & 1) != 0;
            }
            dialog = DIALOG_NONE;
            shell_request_timeout3(250);
            timeout_action = 1;
            return 1;
        }
        case KEY_EXIT: {
            dialog = DIALOG_NONE;
            eqn_draw();
            return 1;
        }
        default: {
            squeak();
            return 1;
        }
    }
}

static bool is_function_menu(int menu) {
    return menu == EQMN_EXTRA_FCN1
            || menu == EQMN_EXTRA_FCN2
            || menu == EQMN_EXTRA_FCN3
            || menu == EQMN_EXTRA_FCN4
            || menu == MENU_PROB
            || menu == MENU_CUSTOM1
            || menu == MENU_CUSTOM2
            || menu == MENU_CUSTOM3
            || menu == MENU_CATALOG
            || menu == EQMN_MATRIX1
            || menu == EQMN_MATRIX2
            || menu == EQMN_BASE1
            || menu == EQMN_BASE2
            || menu == EQMN_PGM_FCN1
            || menu == EQMN_PGM_FCN2
            || menu == EQMN_CONVERT1
            || menu == EQMN_CONVERT2
            || menu == EQMN_CONVERT3
            || menu == EQMN_CONVERT4;
}

static bool sibling_menus(int menu1, int menu2) {
    if (menu1 == MENU_NONE || menu2 == MENU_NONE)
        return false;
    if (menu1 == menu2)
        return true;
    int first = menu1;
    while (true) {
        menu1 = getmenu(menu1)->next;
        if (menu1 == MENU_NONE || menu1 == first)
            return false;
        if (menu1 == menu2)
            return true;
    }
}

static void select_function_menu(int menu) {
    if (!is_function_menu(edit.id))
        prev_edit = edit;
    menu_sticky = sibling_menus(menu, edit.id);
    if (menu != MENU_CATALOG) {
        if (!menu_sticky) {
            update_menu(menu);
            eqn_draw();
        }
    } else if (edit.id != MENU_CATALOG) {
        set_catsect(CATSECT_TOP);
        int rows = draw_eqn_catalog(edit.catsect, 0, menu_item);
        update_menu(menu, edit.catsect, rows, 0, false);
    }
}

static void start_edit(int pos) {
    if (!get_equation()) {
        show_error(ERR_INSUFFICIENT_MEMORY);
    } else {
        new_eq = false;
        edit_pos = pos;
        update_skin_mode();
        display_pos = 0;
        if (disp_r == 2) {
            if (pos > 12) {
                display_pos = pos - 12;
                int slop = edit_len - display_pos - disp_c;
                if (slop < 0)
                    display_pos = edit_len >= disp_c ? edit_len - disp_c : 0;
            }
        } else {
            int lines = disp_r - headers - 1;
            int maxlen = lines * disp_c;
            while (pos - display_pos > maxlen / 2)
                display_pos += disp_c;
            while (display_pos > 0 && edit_len - display_pos < maxlen - disp_c + 1)
                display_pos -= disp_c;
        }
        update_menu(MENU_NONE);
        restart_cursor();
        eqn_draw();
    }
}

static int keydown_list(int key, bool shift, int *repeat) {
    switch (key) {
        case KEY_UP: {
            if (shift) {
                selected_row = -1;
                screen_row = 0;
                eqn_draw();
            } else if (selected_row >= 0) {
                selected_row--;
                if (screen_row > 0)
                    screen_row--;
                rep_key = key;
                *repeat = disp_r == 2 ? 3 : 1;
                eqn_draw();
            }
            return 1;
        }
        case KEY_DOWN: {
            if (shift) {
                selected_row = num_eqns;
                eqn_draw();
            } else if (selected_row < num_eqns) {
                selected_row++;
                screen_row++;
                rep_key = key;
                *repeat = disp_r == 2 ? 3 : 1;
                eqn_draw();
            }
            return 1;
        }
        case KEY_SIGMA:
        case KEY_ENTER: {
            /* CALC */
            if (shift || selected_row == -1 || selected_row == num_eqns) {
                squeak();
                return 1;
            }
            vartype *v = eqns->array->data[selected_row];
            if (v->type != TYPE_STRING && v->type != TYPE_EQUATION) {
                show_error(ERR_INVALID_TYPE);
                return 1;
            }
            char *text;
            int len;
            if (v->type == TYPE_STRING) {
                vartype_string *s = (vartype_string *) v;
                text = s->txt();
                len = s->length;
            } else {
                vartype_equation *eq = (vartype_equation *) v;
                equation_data *eqd = eq->data;
                if (eqd->compatModeEmbedded || eqd->compatMode == (bool) flags.f.eqn_compat)
                    goto no_need_to_reparse;
                text = eqd->text;
                len = eqd->length;
            }
            int errpos;
            vartype *eq;
            eq = new_equation(text, len, flags.f.eqn_compat, &errpos);
            if (eq == NULL) {
                if (errpos == -1) {
                    show_error(ERR_INSUFFICIENT_MEMORY);
                } else {
                    squeak();
                    show_error(ERR_INVALID_EQUATION);
                    current_error = ERR_NONE;
                    timeout_action = 3;
                    timeout_edit_pos = errpos;
                    shell_request_timeout3(1000);
                }
                return 1;
            }
            if (!disentangle((vartype *) eqns)) {
                free_vartype(eq);
                show_error(ERR_INSUFFICIENT_MEMORY);
                return 1;
            }
            free_vartype(eqns->array->data[selected_row]);
            eqns->array->data[selected_row] = eq;
            v = eq;
            no_need_to_reparse:

            /* Make sure all parameters exist, creating new ones
             * initialized to zero where necessary.
             */
            equation_data *eqd = ((vartype_equation *) v)->data;
            std::vector<std::string> params, locals;
            eqd->ev->collectVariables(&params, &locals);
            for (int i = 0; i < params.size(); i++) {
                std::string n = params[i];
                vartype *p = recall_var(n.c_str(), (int) n.length());
                if (p == NULL) {
                    p = new_real(0);
                    if (p != NULL)
                        store_var(n.c_str(), (int) n.length(), p);
                }
            }

            pending_command_arg.type = ARGTYPE_EQN;
            pending_command_arg.val.num = ((vartype_equation *) v)->data->eqn_index;
            if (params.size() == 0) {
                pending_command = CMD_EVALNi;
            } else if (menu_whence == CATSECT_PGM_SOLVE
                    || menu_whence == CATSECT_EQN_NAMED
                    || menu_whence == CATSECT_TOP) {
                mode_varmenu_whence = menu_whence;
                pending_command = CMD_EQNSLVi;
            } else if (menu_whence == CATSECT_PGM_INTEG) {
                pending_command = CMD_EQNINTi;
            } else {
                /* PGMMENU */
                pending_command = CMD_PMEXEC;
            }
            /* Note that we don't do active = false here, since at this point
             * it is still possible that the command will go to NULL, and in
             * that case, we should stay here. Thus, setting active = false
             * is accomplished by EVALNi, PGMSLVi, PGMINTi, and PMEXEC.
             */
            redisplay();
            return 2;

        }
        case KEY_INV: {
            /* EDIT */
            if (shift || selected_row == -1 || selected_row == num_eqns) {
                squeak();
                return 1;
            }
            start_edit(0);
            return 1;
        }
        case KEY_SQRT: {
            /* DELET */
            if (shift || selected_row == -1 || selected_row == num_eqns) {
                squeak();
            } else {
                vartype *v = eqns->array->data[selected_row];
                bool all_vars_exist = false;
                if (v->type == TYPE_EQUATION) {
                    all_vars_exist = true;
                    vartype_equation *eq = (vartype_equation *) v;
                    equation_data *eqd = eq->data;
                    std::vector<std::string> params = get_parameters(eqd);
                    if (params.size() == 0)
                        all_vars_exist = false;
                    else
                        for (int i = 0; i < params.size(); i++) {
                            std::string s = params[i];
                            if (recall_var(s.c_str(), s.length()) == NULL) {
                                all_vars_exist = false;
                                break;
                            }
                        }
                }
                dialog = all_vars_exist ? DIALOG_DELETE_BOTH_CONFIRM : DIALOG_DELETE_CONFIRM;
                eqn_draw();
            }
            return 1;
        }
        case KEY_LOG: {
            /* NEW */
            if (shift) {
                squeak();
                return 1;
            }
            edit_buf = NULL;
            edit_len = edit_capacity = 0;
            new_eq = true;
            edit_pos = 0;
            update_skin_mode();
            display_pos = 0;
            update_menu(MENU_ALPHA1);
            restart_cursor();
            eqn_draw();
            return 1;
        }
        case KEY_LN:
        case KEY_XEQ: {
            /* MOVE up, MOVE down */
            if (shift || selected_row == -1 || selected_row == num_eqns) {
                squeak();
                return 1;
            }
            if (disp_r == 2) {
                /* First, show a glimpse of the current contents of the target row,
                 * then, perform the actual swap (unless the target row is one of
                 * the end-of-list markers), and finally, schedule a redraw after
                 * 0.5 to make the screen reflect the state of affairs with the
                 * completed swap.
                 */
                int dir;
                if (key == KEY_LN) {
                    /* up */
                    dir = -1;
                    goto move;
                } else {
                    /* down */
                    dir = 1;
                    move:
                    selected_row += dir;
                    eqn_draw();
                    if (selected_row == -1 || selected_row == num_eqns) {
                        selected_row -= dir;
                    } else {
                        vartype *v = eqns->array->data[selected_row];
                        eqns->array->data[selected_row] = eqns->array->data[selected_row - dir];
                        eqns->array->data[selected_row - dir] = v;
                    }
                }
                timeout_action = 1;
                shell_request_timeout3(500);
            } else {
                int dir;
                if (key == KEY_LN) {
                    /* up */
                    dir = -1;
                    goto move2;
                } else {
                    /* down */
                    dir = 1;
                    move2:
                    selected_row += dir;
                    if (selected_row == -1 || selected_row == num_eqns) {
                        selected_row -= dir;
                        squeak();
                    } else {
                        vartype *v = eqns->array->data[selected_row];
                        eqns->array->data[selected_row] = eqns->array->data[selected_row - dir];
                        eqns->array->data[selected_row - dir] = v;
                        screen_row += dir;
                        if (screen_row < 0)
                            screen_row = 0;
                        eqn_draw();
                    }
                }
            }
            return 1;
        }
        case KEY_STO: {
            if (shift || selected_row == -1 || selected_row == num_eqns) {
                squeak();
                return 1;
            }
            dialog = DIALOG_STO;
            eqn_draw();
            return 1;
        }
        case KEY_RCL: {
            if (shift) {
                squeak();
                return 1;
            }
            dialog = DIALOG_RCL;
            eqn_draw();
            return 1;
        }
        case KEY_SUB: {
            if (shift)
                select_function_menu(MENU_PRINT1);
            else
                squeak();
            return 1;
        }
        case KEY_CHS: {
            if (shift)
                select_function_menu(MENU_MODES1);
            else
                squeak();
            return 1;
        }
        case KEY_E: {
            if (shift)
                select_function_menu(MENU_DISP1);
            else
                squeak();
            return 1;
        }
        case KEY_7:
        case KEY_8: {
            if (shift) {
                clear_row(0);
                if (key == KEY_7) {
                    draw_string(0, 0, "SOLVER Menu Selected", 20);
                    menu_whence = CATSECT_PGM_SOLVE;
                } else {
                    draw_string(0, 0, "\3f(x) Menu Selected", 19);
                    menu_whence = CATSECT_PGM_INTEG;
                }
                flush_display();
                timeout_action = 1;
                shell_request_timeout3(2000);
            } else
                squeak();
            return 1;
        }
        case KEY_RUN: {
            if (error_eqn_id == -1) {
                no_eqn:
                squeak();
                return 1;
            }
            int4 idx = -1;
            for (int4 i = 0; i < num_eqns; i++) {
                vartype *v = eqns->array->data[i];
                if (v->type != TYPE_EQUATION)
                    continue;
                vartype_equation *eq = (vartype_equation *) v;
                if (eq->data->eqn_index == error_eqn_id) {
                    idx = i;
                    break;
                }
            }
            if (idx == -1) {
                // The equation with the error is not in EQNS; add it at the end
                equation_data *eqd = eq_dir->prgms[error_eqn_id].eq_data;
                if (eqd == NULL) {
                    error_eqn_id = -1;
                    goto no_eqn;
                }
                vartype *eq = new_equation(eqd);
                if (eq == NULL) {
                    error_eqn_id = -1;
                    goto no_eqn;
                }
                vartype **new_data = (vartype **) realloc(eqns->array->data, (num_eqns + 1) * sizeof(vartype *));
                if (new_data == NULL) {
                    error_eqn_id = -1;
                    free_vartype(eq);
                    goto no_eqn;
                }
                eqns->array->data = new_data;
                eqns->size++;
                eqns->array->data[num_eqns] = eq;
                idx = num_eqns;
                num_eqns++;
            }
            eqn_set_selected_row(idx);
            start_edit(error_eqn_pos);
            return 1;
        }

        case KEY_EXIT: {
            if (shift) {
                docmd_off(NULL);
                return 1;
            }
            active = false;
            if (menu_whence == CATSECT_TOP) {
                set_menu(MENULEVEL_PLAIN, MENU_NONE);
            } else if (flags.f.prgm_mode || !mvar_prgms_exist()) {
                int menu;
                switch (menu_whence) {
                    case CATSECT_PGM_SOLVE: menu = MENU_SOLVE; break;
                    case CATSECT_PGM_INTEG: menu = MENU_INTEG; break;
                    case CATSECT_PGM_MENU: menu = MENU_NONE; break;
                    case CATSECT_EQN_NAMED: goto exit_to_cat;
                    default: menu = MENU_NONE;
                }
                set_menu(MENULEVEL_APP, menu);
            } else {
                exit_to_cat:
                set_menu(MENULEVEL_APP, MENU_CATALOG);
                set_cat_section(menu_whence);
            }
            redisplay();
            return 1;
        }
        case 2048 + CMD_PLOT_M: {
            /* GRAPH */
            if (selected_row == -1 || selected_row == num_eqns) {
                no_graph:
                squeak();
                return 1;
            }
            vartype *eq = eqns->array->data[selected_row];
            if (eq->type != TYPE_EQUATION)
                goto no_graph;
            vartype *temp_lastx = lastx;
            lastx = eq;
            arg_struct arg;
            arg.type = ARGTYPE_STK;
            arg.val.stk = 'L';
            int err = docmd_eqnplot(&arg);
            lastx = temp_lastx;
            if (err != ERR_NONE) {
                show_error(err);
                return 1;
            }
            set_menu(MENULEVEL_APP, MENU_GRAPH);
            eqn_end();
            display_plot_params(-1);
            return 1;
        }
        default: {
            squeak();
            return 1;
        }
    }
}

static int keydown_edit_2(int key, bool shift, int *repeat) {
    if (key >= 1024 && key < 2048) {
        char c = key - 1024;
        insert_text(&c, 1);
        return 1;
    }

    if (key >= 2048) {
        int cmd = key - 2048;
        switch (cmd) {
            case CMD_UNITS: {
                select_function_menu(MENU_CATALOG);
                set_catsect_no_top(CATSECT_UNITS_1);
                edit.catalog_row = 0;
                eqn_draw();
                return 1;
            }
            default:
                insert_function(cmd);
                return 1;
        }
    }

    // For units; these can come from the UNITS catalog, but
    // also from the CUSTOM menu
    std::string us;

    if (key >= KEY_SIGMA && key <= KEY_XEQ) {
        /* Menu keys */
        if (edit.id == MENU_NONE) {
            /* Navigation menu */
            if (disp_r > 2 && key >= KEY_INV && key <= KEY_LN)
                /* Cursor keys arranged differently in big screen mode */
                key ^= 1;
            switch (key) {
                case KEY_SIGMA: {
                    /* DEL */
                    if (edit_len > 0 && edit_pos < edit_len) {
                        memmove(edit_buf + edit_pos, edit_buf + edit_pos + 1, edit_len - edit_pos - 1);
                        edit_len--;
                        if (edit_pos < edit_len && edit_len % disp_c == 0 && display_pos >= disp_c && display_pos + (disp_r - headers - 1) * disp_c > edit_len)
                            display_pos -= disp_c;
                        rep_key = KEY_SIGMA;
                        *repeat = 2;
                        restart_cursor();
                        eqn_draw();
                    } else
                        squeak();
                    return 1;
                }
                case KEY_INV: {
                    /* <<- */
                    if (disp_r == 2) {
                        if (shift)
                            goto left;
                        int dpos = edit_pos - display_pos;
                        int off = display_pos > 0 ? 1 : 0;
                        if (dpos > off) {
                            edit_pos = display_pos + off;
                        } else {
                            edit_pos -= disp_c - 2;
                            if (edit_pos < 0)
                                edit_pos = 0;
                            display_pos = edit_pos - 1;
                            if (display_pos < 0)
                                display_pos = 0;
                        }
                    } else {
                        if (shift) {
                            edit_pos %= disp_c;
                            display_pos = 0;
                        } else {
                            if (edit_pos - disp_c < 0)
                                return 1;
                            rep_key = KEY_INV;
                            *repeat = 2;
                            if (edit_pos > 0 && edit_pos == edit_len && edit_pos % disp_c == 0) {
                                edit_pos -= disp_c;
                                display_pos -= disp_c;
                            } else {
                                edit_pos -= disp_c;
                                if (edit_pos <= display_pos) {
                                    display_pos = (edit_pos / disp_c) * disp_c;
                                    if (edit_pos == display_pos && display_pos > 0)
                                        display_pos -= disp_c;
                                }
                            }
                        }
                    }
                    restart_cursor();
                    eqn_draw();
                    return 1;
                }
                case KEY_SQRT: {
                    /* <- */
                    left:
                    if (edit_pos > 0) {
                        if (shift) {
                            if (disp_r == 2)
                                edit_pos = 0;
                            else
                                edit_pos = (edit_pos / disp_c) * disp_c;
                        } else {
                            edit_pos--;
                            rep_key = KEY_SQRT;
                            *repeat = 2;
                        }
                        if (edit_pos + 1 == edit_len && edit_len % disp_c == 0 && display_pos >= disp_c)
                            display_pos -= disp_c;
                        while (true) {
                            int dpos = edit_pos - display_pos;
                            if (dpos > 0 || display_pos == 0 && dpos == 0)
                                break;
                            if (disp_r == 2) {
                                display_pos--;
                            } else {
                                int d = display_pos % disp_c;
                                display_pos -= d == 0 ? disp_c : d;
                            }
                        }
                        restart_cursor();
                        eqn_draw();
                    }
                    return 1;
                }
                case KEY_LOG: {
                    /* -> */
                    right:
                    if (edit_pos < edit_len) {
                        if (shift) {
                            if (disp_r == 2)
                                edit_pos = edit_len;
                            else {
                                edit_pos = (edit_pos / disp_c) * disp_c + disp_c - 1;
                                if (edit_pos > edit_len)
                                    edit_pos = edit_len;
                            }
                        } else {
                            edit_pos++;
                            rep_key = KEY_LOG;
                            *repeat = 2;
                        }
                        while (true) {
                            int dpos = edit_pos - display_pos;
                            int maxlen = (disp_r - headers - 1) * disp_c;
                            if (dpos < maxlen - 1 || display_pos + maxlen >= edit_len && dpos == maxlen - 1)
                                break;
                            if (disp_r == 2)
                                display_pos++;
                            else
                                display_pos = ((display_pos / disp_c) + 1) * disp_c;
                        }
                        restart_cursor();
                        eqn_draw();
                    }
                    return 1;
                }
                case KEY_LN: {
                    /* ->> */
                    if (disp_r == 2) {
                        if (shift)
                            goto right;
                        int dpos = edit_pos - display_pos;
                        if (edit_len - display_pos > disp_c) {
                            /* There's an ellipsis in the right margin */
                            if (dpos < disp_c - 2) {
                                edit_pos = display_pos + disp_c - 2;
                            } else {
                                edit_pos += disp_c - 2;
                                display_pos += disp_c - 2;
                                if (edit_pos > edit_len) {
                                    edit_pos = edit_len;
                                    display_pos = edit_pos - disp_c + 2;
                                }
                            }
                        } else {
                            edit_pos = edit_len;
                            display_pos = edit_pos - disp_c + 1;
                            if (display_pos < 0)
                                display_pos = 0;
                        }
                    } else {
                        if (shift) {
                            while (edit_pos + disp_c <= edit_len)
                                edit_pos += disp_c;
                        } else {
                            edit_pos += disp_c;
                            if (edit_pos > edit_len)
                                edit_pos = edit_len;
                            else {
                                rep_key = KEY_LN;
                                *repeat = 2;
                            }
                        }
                        int maxlen = (disp_r - headers - 1) * disp_c;
                        while (edit_pos - display_pos >= maxlen)
                            display_pos = ((display_pos / disp_c) + 1 ) * disp_c;
                    }
                    restart_cursor();
                    eqn_draw();
                    return 1;
                }
                case KEY_XEQ: {
                    /* ALPHA */
                    update_menu(MENU_ALPHA1);
                    prev_edit.id = MENU_NONE;
                    eqn_draw();
                    return 1;
                }
            }
        } else if (edit.id == MENU_ALPHA1 || edit.id == MENU_ALPHA2) {
            /* ALPHA menu */
            update_menu(getmenu(edit.id)->child[key - 1].menuid);
            eqn_draw();
            return 1;
        } else if (edit.id >= MENU_ALPHA_ABCDE1 && edit.id <= MENU_ALPHA_MISC2) {
            /* ALPHA sub-menus */
            char c = getmenu(edit.id)->child[key - 1].title[0];
            if (shift && c >= 'A' && c <= 'Z')
                c += 32;
            update_menu(getmenu(edit.id)->parent);
            insert_text(&c, 1);
            return 1;
        } else if (edit.id >= MENU_CUSTOM1 && edit.id <= MENU_CUSTOM3) {
            int row = edit.id - MENU_CUSTOM1;
            char label[7];
            int len;
            get_custom_key(row * 6 + key, label, &len);
            if (len == 0) {
                squeak();
                return 1;
            }
            /* Builtins go through the usual mapping; everything else
             * is inserted literally.
             */
            int cmd = find_builtin(label, len);
            if (cmd != CMD_NONE) {
                if (insert_function(cmd)) {
                    goto_prev_menu();
                    eqn_draw();
                } else
                    squeak();
                return 1;
            }
            arg_struct arg;
            arg.type = ARGTYPE_STR;
            string_copy(arg.val.text, &arg.length, label, len);
            pgm_index dummy1;
            int4 dummy2;
            if (find_global_label(&arg, &dummy1, &dummy2)) {
                std::string s("XEQ(");
                s += std::string(label, len);
                s += ":";
                if (insert_text(s.c_str(), s.length())) {
                    goto_prev_menu();
                    eqn_draw();
                } else
                    squeak();
                return 1;
            }
            vartype *v = recall_var(label, len);
            if (v == NULL) {
                squeak();
                return 1;
            }
            if (v->type == TYPE_EQUATION) {
                std::string s("EVALN(");
                s += std::string(label, len);
                s += ":";
                if (insert_text(s.c_str(), s.length())) {
                    goto_prev_menu();
                    eqn_draw();
                } else
                    squeak();
                return 1;
            }
            if (v->type == TYPE_UNIT) {
                us = std::string(label, len);
                goto do_unit;
            }
            if (insert_text(label, len)) {
                goto_prev_menu();
                eqn_draw();
            } else
                squeak();
            return 1;
        } else if (edit.id == MENU_CATALOG && edit.catsect == CATSECT_TOP) {
            switch (key) {
                case KEY_SIGMA:
                    if ((skin_flags & 1) != 0) {
                        set_catsect(CATSECT_FCN);
                        break;
                    } else {
                        /* Allowing directory navigation doesn't seem to
                         * make sense, or at least I don't see how to make
                         * that work in a useful fashion, given that everything
                         * we do here depends on EQNS, and changing directories
                         * could mean EQNS being replaced by a different one...
                         */
                        squeak();
                        return 1;
                    }
                case KEY_INV: set_catsect(CATSECT_PGM); break;
                case KEY_SQRT:
                    if (!vars_exist(CATSECT_REAL)) {
                        squeak();
                        return 1;
                    } else {
                        set_catsect(CATSECT_REAL);
                        break;
                    }
                case KEY_LOG:
                    if (!vars_exist(CATSECT_CPX)) {
                        squeak();
                        return 1;
                    } else {
                        set_catsect(CATSECT_CPX);
                        break;
                    }
                case KEY_LN:
                    if (!vars_exist(CATSECT_MAT)) {
                        squeak();
                        return 1;
                    } else {
                        set_catsect(CATSECT_MAT);
                        break;
                    }
                case KEY_XEQ:
                    if ((skin_flags & 2) != 0) {
                        display_mem();
                        timeout_action = 1;
                        shell_request_timeout3(2000);
                        return 1;
                    } else {
                        set_catsect(CATSECT_UNITS_1);
                        break;
                    }
            }
            edit.catalog_row = 0;
            eqn_draw();
        } else if (edit.id == MENU_CATALOG && edit.catsect == CATSECT_MORE) {
            switch (key) {
                case KEY_SIGMA:
                    if (!vars_exist(CATSECT_LIST)) {
                        squeak();
                        return 1;
                    } else {
                        set_catsect(CATSECT_LIST);
                        break;
                    }
                case KEY_INV:
                    if (!vars_exist(CATSECT_EQN)) {
                        squeak();
                        return 1;
                    } else {
                        set_catsect(CATSECT_EQN);
                        break;
                    }
                case KEY_SQRT:
                    if (!named_eqns_exist()) {
                        squeak();
                        return 1;
                    } else {
                        set_catsect(CATSECT_EQN_NAMED);
                        break;
                    }
                case KEY_LOG:
                    if (!vars_exist(CATSECT_OTHER)) {
                        squeak();
                        return 1;
                    } else {
                        set_catsect(CATSECT_OTHER);
                        break;
                    }
                case KEY_LN:
                    if ((skin_flags & 1) == 0) {
                        set_catsect(CATSECT_FCN);
                        break;
                    } else {
                        /* Allowing directory navigation doesn't seem to
                         * make sense, or at least I don't see how to make
                         * that work in a useful fashion, given that everything
                         * we do here depends on EQNS, and changing directories
                         * could mean EQNS being replaced by a different one...
                         */
                        squeak();
                        return 1;
                    }
                case KEY_XEQ:
                    if ((skin_flags & 2) == 0) {
                        display_mem();
                        timeout_action = 1;
                        shell_request_timeout3(2000);
                        return 1;
                    } else {
                        set_catsect(CATSECT_UNITS_1);
                        break;
                    }
            }
            edit.catalog_row = 0;
            eqn_draw();
        } else if (edit.id == MENU_CATALOG && edit.catsect == CATSECT_FCN) {
            /* Subset of the regular FCN catalog plus Free42 extensions */
            int cmd = catalog[edit.catalog_row * 6 + key - 1];
            if (cmd == CMD_NULL) {
                squeak();
            } else {
                if (insert_function(cmd)) {
                    goto_prev_menu();
                    eqn_draw();
                }
            }
            return 1;
        } else if (edit.id == MENU_CATALOG && edit.catsect >= CATSECT_UNITS_1
                   && edit.catsect <= CATSECT_UNITS_3) {
            switch (edit.catsect) {
                case CATSECT_UNITS_1:
                    switch (key) {
                        case 1: set_catsect(CATSECT_UNITS_LENG); break;
                        case 2: set_catsect(CATSECT_UNITS_AREA); break;
                        case 3: set_catsect(CATSECT_UNITS_VOL); break;
                        case 4: set_catsect(CATSECT_UNITS_TIME); break;
                        case 5: set_catsect(CATSECT_UNITS_SPEED); break;
                        case 6: set_catsect(CATSECT_UNITS_MASS); break;
                    }
                    break;
                case CATSECT_UNITS_2:
                    switch (key) {
                        case 1: set_catsect(CATSECT_UNITS_FORCE); break;
                        case 2: set_catsect(CATSECT_UNITS_ENRG); break;
                        case 3: set_catsect(CATSECT_UNITS_POWR); break;
                        case 4: set_catsect(CATSECT_UNITS_PRESS); break;
                        case 5: set_catsect(CATSECT_UNITS_TEMP); break;
                        case 6: set_catsect(CATSECT_UNITS_ELEC); break;
                    }
                    break;
                case CATSECT_UNITS_3:
                    switch (key) {
                        case 1: set_catsect(CATSECT_UNITS_ANGL); break;
                        case 2: set_catsect(CATSECT_UNITS_LIGHT); break;
                        case 3: set_catsect(CATSECT_UNITS_RAD); break;
                        case 4: set_catsect(CATSECT_UNITS_VISC); break;
                        default: squeak(); return 1;
                    }
                    break;
            }
            edit.catalog_row = 0;
            eqn_draw();
        } else if (edit.id == MENU_CATALOG) {
            if (edit.catsect == CATSECT_EQN_NAMED) {
                int index = edit.catalog_row * 6 + key - 1;
                std::vector<std::string> names;
                try {
                    names = get_equation_names();
                } catch (std::bad_alloc &) {
                    squeak();
                    return 1;
                }
                if (index >= names.size()) {
                    squeak();
                    return 1;
                }
                std::string s = names[index] + "(";
                insert_text(s.c_str(), s.length());
            } else if (edit.catsect == CATSECT_PGM) {
                int index = menu_item[key - 1];
                if (index == -1 || cwd->labels[index].length == 0) {
                    squeak();
                    return 1;
                }
                std::string s(cwd->labels[index].name, cwd->labels[index].length);
                s = "XEQ(" + s + ":";
                insert_text(s.c_str(), s.length());
            } else if (edit.catsect >= CATSECT_UNITS_LENG
                       && edit.catsect <= CATSECT_UNITS_VISC) {
                if (true) {
                    const char *text[6];
                    int length[6];
                    int row = edit.catalog_row, rows;
                    get_units_cat_row(edit.catsect, text, length, &row, &rows);
                    if (length[key - 1] == 0) {
                        squeak();
                        return 1;
                    }
                    us = std::string(text[key - 1], length[key - 1]);
                }
                do_unit:
                int upos = -1;
                int lastq = -1;
                int e = edit_pos;
                if (e > edit_len - 2)
                    e = edit_len - 2;
                for (int epos = e; epos >= 0; epos--) {
                    if (edit_buf[epos] == '_' && edit_buf[epos + 1] == '"') {
                        upos = epos;
                        break;
                    }
                    if (edit_buf[epos + 1] == '"')
                        lastq = epos + 1;
                }
                if (upos >= 0) {
                    if (lastq == edit_pos - 1) {
                        // We're right behind it; no further action needed
                    } else if (lastq != -1) {
                        // We're loose from this element; ignore it
                        upos = -1;
                    } else {
                        // Looks like we're in the middle of it; let's
                        // look for the closing quote to our right
                        for (int epos = upos + 2; epos < edit_len; epos++) {
                            if (edit_buf[epos] == '"') {
                                if (edit_buf[epos - 1] != '_')
                                    lastq = epos;
                                break;
                            }
                        }
                        if (lastq == -1)
                            upos = -1;
                    }
                }
                std::string es;
                if (upos >= 0) {
                    es = std::string(edit_buf + upos + 2, lastq - upos - 2);
                    lastq++;
                } else {
                    upos = lastq = edit_pos;
                }
                if (es == "") {
                    if (shift)
                        us = std::string("1/(", 3) + us + ")";
                } else {
                    if (shift)
                        us = es + "/(" + us + ")";
                    else
                        us = es + "*" + us;
                }
                normalize_unit(us, &us);
                memmove(edit_buf + upos, edit_buf + lastq, edit_len - lastq);
                edit_len -= lastq - upos;
                edit_pos = upos;
                if (us != "") {
                    us = std::string("_\"", 2) + us + "\"";
                    insert_text(us.c_str(), us.length());
                }
            } else {
                int index = menu_item[key - 1];
                if (index == -1) {
                    squeak();
                    return 1;
                }
                std::string s(cwd->vars[index].name, cwd->vars[index].length);
                if (edit.catsect == CATSECT_EQN)
                    s = "EVALN(" + s + ":";
                insert_text(s.c_str(), s.length());
            }
            goto_prev_menu();
            eqn_draw();
            return 1;
        } else {
            /* Various function menus */
            int cmd;
            if (shift && edit.id == MENU_TOP_FCN) {
                switch (key) {
                    case KEY_SIGMA: cmd = CMD_SIGMASUB; break;
                    case KEY_INV: cmd = CMD_Y_POW_X; break;
                    case KEY_SQRT: cmd = CMD_SQUARE; break;
                    case KEY_LOG: cmd = CMD_10_POW_X; break;
                    case KEY_LN: cmd = CMD_E_POW_X; break;
                    case KEY_XEQ: cmd = EQCMD_SEQ; break;
                }
            } else {
                cmd = getmenu(edit.id)->child[key - 1].menuid;
                if (cmd == MENU_NONE /*|| (cmd & 0xf000) == 0*/)
                    cmd = CMD_NULL;
                else if ((cmd & 0x3000) == 0x2000) {
                    update_menu(cmd & 0x0fff);
                    eqn_draw();
                    return 1;
                } else
                    cmd = cmd & 0x0fff;
            }
            if (insert_function(cmd)) {
                goto_prev_menu();
                eqn_draw();
            }
            return 1;
        }
    } else {
        /* Rest of keyboard */
        switch (key) {
            case KEY_STO: {
                if (shift)
                    insert_function(flags.f.polar ? CMD_PCOMPLX : CMD_RCOMPLX);
                else
                    insert_function(CMD_GSTO);
                break;
            }
            case KEY_RCL: {
                if (shift)
                    insert_text("%", 1);
                else
                    insert_function(CMD_GRCL);
                break;
            }
            case KEY_RDN: {
                if (shift)
                    if (flags.f.eqn_compat)
                        insert_text("PI", 2);
                    else
                        insert_text("\7", 1);
                else
                    select_function_menu(EQMN_STACK);
                break;
            }
            case KEY_SIN: {
                if (shift)
                    insert_function(CMD_ASIN);
                else
                    insert_function(CMD_SIN);
                break;
            }
            case KEY_COS: {
                if (shift)
                    insert_function(CMD_ACOS);
                else
                    insert_function(CMD_COS);
                break;
            }
            case KEY_TAN: {
                if (shift)
                    insert_function(CMD_ATAN);
                else
                    insert_function(CMD_TAN);
                break;
            }
            case KEY_ENTER: {
                if (shift) {
                    update_menu(MENU_ALPHA1);
                    eqn_draw();
                } else if (edit_len == 0) {
                    squeak();
                } else {
                    save();
                }
                break;
            }
            case KEY_SWAP: {
                if (shift)
                    insert_text("[", 1);
                else
                    insert_text("(", 1);
                break;
            }
            case KEY_CHS: {
                if (shift)
                    insert_text("]", 1);
                else
                    insert_text(")", 1);
                break;
            }
            case KEY_E: {
                if (shift)
                    squeak();
                else
                    insert_text("\030", 1);
                break;
            }
            case KEY_BSP: {
                if (shift) {
                    edit_len = 0;
                    edit_pos = 0;
                    display_pos = 0;
                    eqn_draw();
                } else if (edit_len > 0 && edit_pos > 0) {
                    edit_pos--;
                    memmove(edit_buf + edit_pos, edit_buf + edit_pos + 1, edit_len - edit_pos - 1);
                    edit_len--;
                    if (display_pos > 0) {
                        if (disp_r == 2) {
                            display_pos--;
                        } else {
                            int maxlen = (disp_r - headers - 1) * disp_c;
                            if (display_pos + maxlen - edit_len >= disp_c)
                                display_pos = ((display_pos - 1) / disp_c) * disp_c;
                            if (edit_pos == edit_len && edit_pos - display_pos == maxlen)
                                display_pos += disp_c;
                        }
                    }
                    rep_key = KEY_BSP;
                    *repeat = 2;
                    restart_cursor();
                    eqn_draw();
                } else
                    squeak();
                return 1;
            }
            case KEY_0: {
                if (shift)
                    if ((skin_flags & 4) == 0)
                        select_function_menu(MENU_TOP_FCN);
                    else
                        select_function_menu(EQMN_FIN1);
                else
                    insert_text("0", 1);
                break;
            }
            case KEY_1: {
                if (shift)
                    squeak();
                else
                    insert_text("1", 1);
                break;
            }
            case KEY_2: {
                if (shift)
                    select_function_menu(MENU_CUSTOM1);
                else
                    insert_text("2", 1);
                break;
            }
            case KEY_3: {
                if (shift)
                    select_function_menu(EQMN_PGM_FCN1);
                else
                    insert_text("3", 1);
                break;
            }
            case KEY_4: {
                if (shift)
                    select_function_menu(EQMN_BASE1);
                else
                    insert_text("4", 1);
                break;
            }
            case KEY_5: {
                if (shift)
                    select_function_menu(EQMN_CONVERT1);
                else
                    insert_text("5", 1);
                break;
            }
            case KEY_6: {
                if (shift)
                    select_function_menu(EQMN_EXTRA_FCN1);
                else
                    insert_text("6", 1);
                break;
            }
            case KEY_7: {
                if (shift)
                    squeak();
                else
                    insert_text("7", 1);
                break;
            }
            case KEY_8: {
                if (shift)
                    insert_function(CMD_SIGMASUB);
                else
                    insert_text("8", 1);
                break;
            }
            case KEY_9: {
                if (shift)
                    select_function_menu(EQMN_MATRIX1);
                else
                    insert_text("9", 1);
                break;
            }
            case KEY_DOT: {
                if (shift)
                    insert_text(flags.f.decimal_point ? "," : ".", 1);
                else
                    insert_text(flags.f.decimal_point ? "." : ",", 1);
                break;
            }
            case KEY_RUN: {
                if (shift)
                    insert_text(":", 1);
                else
                    insert_text("=", 1);
                break;
            }
            case KEY_DIV: {
                if (shift)
                    select_function_menu(EQMN_STAT1);
                else
                    insert_function(CMD_DIV);
                break;
            }
            case KEY_MUL: {
                if (shift)
                    select_function_menu(MENU_PROB);
                else
                    insert_function(CMD_MUL);
                break;
            }
            case KEY_SUB: {
                if (shift) {
                    select_function_menu(MENU_PRINT1);
                    cursor_on = false;
                    eqn_draw();
                } else {
                    insert_function(CMD_SUB);
                }
                break;
            }
            case KEY_ADD: {
                if (shift) {
                    select_function_menu(MENU_CATALOG);
                    eqn_draw();
                } else
                    insert_function(CMD_ADD);
                break;
            }
            case KEY_UP:
            case KEY_DOWN: {
                if (edit.id == MENU_CATALOG) {
                    if (edit.catsect == CATSECT_TOP) {
                        set_catsect(CATSECT_MORE);
                    } else if (edit.catsect == CATSECT_MORE) {
                        set_catsect(CATSECT_TOP);
                    } else if (edit.catsect >= CATSECT_UNITS_1 && edit.catsect <= CATSECT_UNITS_3) {
                        if (key == KEY_UP) {
                            edit.catsect--;
                            if (edit.catsect < CATSECT_UNITS_1)
                                set_catsect(CATSECT_UNITS_3);
                        } else {
                            edit.catsect++;
                            if (edit.catsect > CATSECT_UNITS_3)
                                set_catsect(CATSECT_UNITS_1);
                        }
                    } else if (key == KEY_UP) {
                        edit.catalog_row--;
                        if (edit.catalog_row == -1)
                            edit.catalog_row = edit.catsect_rows - 1;
                    } else {
                        edit.catalog_row++;
                        if (edit.catalog_row == edit.catsect_rows)
                            edit.catalog_row = 0;
                    }
                    *repeat = 1;
                    eqn_draw();
                } else if (edit.id != MENU_NONE && getmenu(edit.id)->next != MENU_NONE) {
                    if (key == KEY_DOWN)
                        update_menu(getmenu(edit.id)->next);
                    else
                        update_menu(getmenu(edit.id)->prev);
                    *repeat = 1;
                    eqn_draw();
                } else
                    squeak();
                break;
            }
            case KEY_EXIT: {
                if (shift) {
                    docmd_off(NULL);
                    break;
                }
                if (edit.id == MENU_NONE) {
                    if (!new_eq) {
                        vartype *v = eqns->array->data[selected_row];
                        const char *orig_text;
                        int orig_len;
                        if (v->type == TYPE_STRING) {
                            vartype_string *s = (vartype_string *) v;
                            orig_text = s->txt();
                            orig_len = s->length;
                        } else if (v->type == TYPE_EQUATION) {
                            vartype_equation *eq = (vartype_equation *) v;
                            equation_data *eqd = eq->data;
                            orig_text = eqd->text;
                            orig_len = eqd->length;
                        } else {
                            orig_text = "<Invalid>";
                            orig_len = 9;
                        }
                        if (string_equals(edit_buf, edit_len, orig_text, orig_len)) {
                            edit_pos = -1;
                            update_skin_mode();
                            edit.id = MENU_NONE;
                            free(edit_buf);
                            edit_buf = NULL;
                            edit_capacity = edit_len = 0;
                            eqn_draw();
                            break;
                        }
                    }
                    dialog = DIALOG_SAVE_CONFIRM;
                } else if (edit.id == MENU_CATALOG) {
                    if (edit.catsect == CATSECT_LIST
                            || edit.catsect == CATSECT_EQN
                            || edit.catsect == CATSECT_EQN_NAMED
                            || edit.catsect == CATSECT_OTHER) {
                        set_catsect(CATSECT_MORE);
                    } else if (edit.catsect == CATSECT_FCN) {
                        if ((skin_flags & 1) == 0)
                            set_catsect(CATSECT_MORE);
                        else
                            set_catsect(CATSECT_TOP);
                    } else if (edit.catsect == CATSECT_TOP
                            || edit.catsect == CATSECT_MORE) {
                        menu_sticky = false;
                        goto_prev_menu();
                    } else if (edit.catsect >= CATSECT_UNITS_LENG
                               && edit.catsect <= CATSECT_UNITS_MASS) {
                        set_catsect(CATSECT_UNITS_1);
                    } else if (edit.catsect >= CATSECT_UNITS_FORCE
                               && edit.catsect <= CATSECT_UNITS_ELEC) {
                        set_catsect(CATSECT_UNITS_2);
                    } else if (edit.catsect >= CATSECT_UNITS_ANGL
                               && edit.catsect <= CATSECT_UNITS_VISC) {
                        set_catsect(CATSECT_UNITS_3);
                    } else if (edit.catsect >= CATSECT_UNITS_1
                               && edit.catsect <= CATSECT_UNITS_3) {
                        if ((skin_flags & 2) == 0)
                            set_catsect(CATSECT_TOP);
                        else
                            set_catsect(CATSECT_MORE);
                    } else {
                        set_catsect(CATSECT_TOP);
                    }

                    eqn_draw();
                } else if (is_function_menu(edit.id)) {
                    menu_sticky = false;
                    goto_prev_menu();
                } else {
                    update_menu(getmenu(edit.id)->parent);
                }
                eqn_draw();
                break;
            }
            default: {
                squeak();
                break;
            }
        }
    }
    return 1;
}

static int keydown_edit(int key, bool shift, int *repeat) {
    t_rep_count = 0;
    int ret = keydown_edit_2(key, shift, repeat);
    if (core_settings.auto_repeat && t_rep_count == 1) {
        *repeat = 2;
        rep_key = t_rep_key;
    } else if (*repeat != 0) {
        rep_key = key;
    }
    return ret;
}

int eqn_repeat() {
    if (!active)
        return -1;
    // Like core_repeat(): 0 means stop repeating; 1 means slow repeat,
    // for Up/Down; 2 means fast repeat, for text entry; 3 means extra
    // slow repeat, for the equation editor's list view.
    if (rep_key == -1)
        return 0;
    if (edit_pos == -1) {
        if (rep_key == KEY_UP) {
            if (selected_row >= 0) {
                selected_row--;
                if (screen_row > 0)
                    screen_row--;
                eqn_draw();
                return disp_r == 2 ? 3 : 1;
            } else {
                rep_key = -1;
            }
        } else if (rep_key == KEY_DOWN) {
            if (selected_row < num_eqns) {
                selected_row++;
                screen_row++;
                eqn_draw();
                return disp_r == 2 ? 3 : 1;
            } else {
                rep_key = -1;
            }
        }
    } else {
        int repeat = 0;
        keydown_edit(rep_key, false, &repeat);
        if (repeat == 0)
            rep_key = -1;
        else
            return rep_key == KEY_UP || rep_key == KEY_DOWN ? 1 : 2;
    }
    return 0;
}

bool eqn_timeout() {
    if (!active)
        return false;

    int action = timeout_action;
    timeout_action = 0;

    if (action == 1) {
        /* Finish delayed Move Up/Down operation */
        eqn_draw();
    } else if (action == 2) {
        /* Cursor blinking */
        if (edit_pos == -1 || current_error != ERR_NONE || dialog != DIALOG_NONE
                || edit.id == MENU_PRINT1 || edit.id == MENU_PRINT2)
            return true;
        cursor_on = !cursor_on;
        draw_cursor(cursor_on);
        flush_display();
        timeout_action = 2;
        shell_request_timeout3(500);
    } else if (action == 3) {
        /* Start editing after parse error message has timed out */
        start_edit(timeout_edit_pos);
    }
    return true;
}

int return_to_eqn_edit() {
    docmd_rtn(NULL);
    set_running(false);
    if (current_result != NULL)
        free_vartype(current_result);
    if (sp == -1)
        current_result = new_string("<Stack Empty>", 13);
    else
        current_result = dup_vartype(stack[sp]);
    return eqn_start(menu_whence);
}

void eqn_save_error_pos(int eqn_id, int pos) {
    error_eqn_id = eqn_id;
    error_eqn_pos = pos;
}
