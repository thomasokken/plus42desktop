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

#ifndef CORE_GLOBALS_H
#define CORE_GLOBALS_H 1


#include <stdio.h>

#include "free42.h"
#include "core_parser.h"
#include "core_phloat.h"
#include "core_tables.h"
#include "core_variables.h"

extern FILE *gfile;

/**********/
/* Errors */
/**********/

#define ERR_NONE                    0
#define ERR_ALPHA_DATA_IS_INVALID   1
#define ERR_OUT_OF_RANGE            2
#define ERR_DIVIDE_BY_0             3
#define ERR_INVALID_TYPE            4
#define ERR_INVALID_DATA            5
#define ERR_NONEXISTENT             6
#define ERR_DIMENSION_ERROR         7
#define ERR_TOO_FEW_ARGUMENTS       8
#define ERR_SIZE_ERROR              9
#define ERR_STACK_DEPTH_ERROR      10
#define ERR_RESTRICTED_OPERATION   11
#define ERR_YES                    12
#define ERR_NO                     13
#define ERR_STOP                   14
#define ERR_LABEL_NOT_FOUND        15
#define ERR_NO_REAL_VARIABLES      16
#define ERR_NO_COMPLEX_VARIABLES   17
#define ERR_NO_MATRIX_VARIABLES    18
#define ERR_NO_LIST_VARIABLES      19
#define ERR_NO_EQUATION_VARIABLES  20
#define ERR_NO_NAMED_EQUATIONS     21
#define ERR_NO_OTHER_VARIABLES     22
#define ERR_NO_MENU_VARIABLES      23
#define ERR_STAT_MATH_ERROR        24
#define ERR_INVALID_FORECAST_MODEL 25
#define ERR_SINGULAR_MATRIX        26
#define ERR_SOLVE_SOLVE            27
#define ERR_INTEG_INTEG            28
#define ERR_RUN                    29
#define ERR_INTERRUPTED            30
#define ERR_PRINTING_IS_DISABLED   31
#define ERR_INTERRUPTIBLE          32
#define ERR_NO_VARIABLES           33
#define ERR_INSUFFICIENT_MEMORY    34
#define ERR_NOT_YET_IMPLEMENTED    35
#define ERR_INTERNAL_ERROR         36
#define ERR_SUSPICIOUS_OFF         37
#define ERR_RTN_STACK_FULL         38
#define ERR_NUMBER_TOO_LARGE       39
#define ERR_NUMBER_TOO_SMALL       40
#define ERR_INVALID_CONTEXT        41
#define ERR_NAME_TOO_LONG          42
#define ERR_PARSE_ERROR            43
#define ERR_INVALID_EQUATION       44
#define ERR_INCONSISTENT_UNITS     45
#define ERR_INVALID_UNIT           46
#define ERR_VARIABLE_NOT_WRITABLE  47
#define ERR_DIRECTORY_EXISTS       48
#define ERR_VARIABLE_EXISTS        49
#define ERR_TOO_MANY_ARGUMENTS     50
#define ERR_NO_SOLUTION_FOUND      51
#define ERR_PROGRAM_LOCKED         52
#define ERR_NEXT_PROGRAM_LOCKED    53

#define RTNERR_MAX 8

struct error_spec {
    const char *text;
    int length;
};

extern const error_spec errors[];


/*************/
/* Key codes */
/*************/

#define KEY_SIGMA  1
#define KEY_INV    2
#define KEY_SQRT   3
#define KEY_LOG    4
#define KEY_LN     5
#define KEY_XEQ    6
#define KEY_STO    7
#define KEY_RCL    8
#define KEY_RDN    9
#define KEY_SIN   10
#define KEY_COS   11
#define KEY_TAN   12
#define KEY_ENTER 13
#define KEY_SWAP  14
#define KEY_CHS   15
#define KEY_E     16
#define KEY_BSP   17
#define KEY_UP    18
#define KEY_7     19
#define KEY_8     20
#define KEY_9     21
#define KEY_DIV   22
#define KEY_DOWN  23
#define KEY_4     24
#define KEY_5     25
#define KEY_6     26
#define KEY_MUL   27
#define KEY_SHIFT 28
#define KEY_1     29
#define KEY_2     30
#define KEY_3     31
#define KEY_SUB   32
#define KEY_EXIT  33
#define KEY_0     34
#define KEY_DOT   35
#define KEY_RUN   36
#define KEY_ADD   37


/*********/
/* Menus */
/*********/

#define MENU_NONE          -1
#define MENU_ALPHA1         0
#define MENU_ALPHA2         1
#define MENU_ALPHA_ABCDE1   2
#define MENU_ALPHA_ABCDE2   3
#define MENU_ALPHA_FGHI     4
#define MENU_ALPHA_JKLM     5
#define MENU_ALPHA_NOPQ1    6
#define MENU_ALPHA_NOPQ2    7
#define MENU_ALPHA_RSTUV1   8
#define MENU_ALPHA_RSTUV2   9
#define MENU_ALPHA_WXYZ    10
#define MENU_ALPHA_PAREN   11
#define MENU_ALPHA_ARROW   12
#define MENU_ALPHA_COMP    13
#define MENU_ALPHA_MATH1   14
#define MENU_ALPHA_MATH2   15
#define MENU_ALPHA_PUNC1   16
#define MENU_ALPHA_PUNC2   17
#define MENU_ALPHA_PUNC3   18
#define MENU_ALPHA_MISC1   19
#define MENU_ALPHA_MISC2   20
#define MENU_ST            21
#define MENU_IND_ST        22
#define MENU_IND           23
#define MENU_MODES1        24
#define MENU_MODES2        25
#define MENU_MODES3        26
#define MENU_MODES4        27
#define MENU_DISP1         28
#define MENU_DISP2         29
#define MENU_DISP3         30
#define MENU_DISP4         31
#define MENU_CLEAR1        32
#define MENU_CLEAR2        33
#define MENU_CONVERT1      34
#define MENU_CONVERT2      35
#define MENU_FLAGS         36
#define MENU_PROB          37
#define MENU_CUSTOM1       38
#define MENU_CUSTOM2       39
#define MENU_CUSTOM3       40
#define MENU_PGM_FCN1      41
#define MENU_PGM_FCN2      42
#define MENU_PGM_FCN3      43
#define MENU_PGM_FCN4      44
#define MENU_PGM_XCOMP0    45
#define MENU_PGM_XCOMPY    46
#define MENU_PRINT1        47
#define MENU_PRINT2        48
#define MENU_PRINT3        49
#define MENU_TOP_FCN       50
#define MENU_CATALOG       51
#define MENU_BLANK         52
#define MENU_PROGRAMMABLE  53
#define MENU_VARMENU       54
#define MENU_STAT1         55
#define MENU_STAT2         56
#define MENU_STAT_CFIT     57
#define MENU_STAT_MODL     58
#define MENU_STAT_SUMS1    59
#define MENU_STAT_SUMS2    60
#define MENU_STAT_SUMS3    61
#define MENU_MATRIX1       62
#define MENU_MATRIX2       63
#define MENU_MATRIX3       64
#define MENU_MATRIX_SIMQ   65
#define MENU_MATRIX_EDIT1  66
#define MENU_MATRIX_EDIT2  67
#define MENU_BASE1         68
#define MENU_BASE2         69
#define MENU_BASE3         70
#define MENU_BASE4         71
#define MENU_BASE5         72
#define MENU_BASE_A_THRU_F 73
#define MENU_BASE_LOGIC    74
#define MENU_BASE_FLOAT    75
#define MENU_BASE_MODES    76
#define MENU_BASE_DISP     77
#define MENU_SOLVE         78
#define MENU_INTEG         79
#define MENU_INTEG_PARAMS  80
#define MENU_DIR_FCN1      81
#define MENU_DIR_FCN2      82
#define MENU_UNIT_FCN1     83
#define MENU_UNIT_FCN2     84
#define MENU_TVM_APP1      85
#define MENU_TVM_APP2      86
#define MENU_TVM_AMORT     87
#define MENU_TVM_TABLE     88
#define MENU_TVM_PARAMS    89
#define MENU_TVM_PRGM1     90
#define MENU_TVM_PRGM2     91
#define MENU_EQN_FCN1      92
#define MENU_EQN_FCN2      93
#define MENU_GRAPH         94
#define MENU_GRAPH_AXES    95
#define MENU_GRAPH_VIEW    96


struct menu_item_spec {
    int2 menuid;
    unsigned char title_length;
    const char *title;
};

struct menu_spec {
    int2 parent;
    int2 next;
    int2 prev;
    menu_item_spec child[6];
};

extern const menu_spec menus[];


/******************/
/* Emulator state */
/******************/

/* Suppress menu updates while state loading is in progress */
extern bool loading_state;
extern bool saving_state;


/* Registers */
#define REG_T 0
#define REG_Z 1
#define REG_Y 2
#define REG_X 3
extern vartype **stack;
extern int sp;
extern int stack_capacity;
extern vartype *lastx;
extern int reg_alpha_length;
extern char reg_alpha[44];

/* FLAGS
 * Note: flags whose names start with VIRTUAL_ are named here for reference
 * only; they are actually handled by virtual_flag_handler(). Setting or
 * clearing them in 'flags' has no effect.
 * Flags whose names are the letter 'f' followed by two digits have no
 * specified meaning according to the HP-42S manual; they are either user flags
 * or reserved.
 */
typedef union {
    char farray[100];
    struct {
        char f00; char f01; char f02; char f03; char f04;
        char f05; char f06; char f07; char f08; char f09;
        char f10;
        char auto_exec;
        char double_wide_print;
        char lowercase_print;
        char f14;
        char trace_print; /* 'normal_print' ignored if this is set */
        char normal_print;
        char f17; char f18; char f19; char f20;
        char printer_enable;
        char numeric_data_input;
        char alpha_data_input;
        char range_error_ignore;
        char error_ignore;
        char audio_enable;
        char VIRTUAL_custom_menu;
        char decimal_point;
        char thousands_separators;
        char stack_lift_disable;
        char dmy; /* Time Module DMY mode */
        char direct_solver;
        char f33;
        char agraph_control1; /* 0 (default): dst = dst | src, 1: dst = src, */
        char agraph_control0; /* 2: dst = dst & ~src, 3: dst = dst ^ src */
        char digits_bit3;
        char digits_bit2;
        char digits_bit1;
        char digits_bit0;
        char fix_or_all;
        char eng_or_all;
        char grad;
        char rad;
        char VIRTUAL_continuous_on;
        char VIRTUAL_solving;
        char VIRTUAL_integrating;
        char VIRTUAL_variable_menu;
        char VIRTUAL_alpha_mode;
        char VIRTUAL_low_battery;
        char VIRTUAL_message;
        char VIRTUAL_two_line_message;
        char prgm_mode;
        char VIRTUAL_input;
        char eqn_compat; /* HP-17B compatibility for equations */
        char printer_exists;
        char lin_fit;
        char log_fit;
        char exp_fit;
        char pwr_fit;
        char all_sigma;
        char log_fit_invalid;
        char exp_fit_invalid;
        char pwr_fit_invalid;
        char shift_state; /* For use by MENU handlers */
        char VIRTUAL_matrix_editor;
        char grow;
        char ymd; /* Programming extension YMD mode; overrides dmy flag */
        char base_bit0; /* Note: dec=0, bin=1, oct=7, hex=15 */
        char base_bit1;
        char base_bit2;
        char base_bit3;
        char local_label;
        char polar;
        char real_result_only;
        char VIRTUAL_programmable_menu;
        char matrix_edge_wrap;
        char matrix_end_wrap;
        char base_signed; /* Programming extension */
        char base_wrap; /* Programming extension */
        char big_stack; /* Big Stack extension */
        char f81; char f82; char f83; char f84;
        char f85; char f86; char f87; char f88; char f89;
        char f90; char f91; char f92; char f93; char f94;
        char f95; char f96; char f97; char f98; char f99;
    } f;
} flags_struct;
extern flags_struct flags;
extern const char *virtual_flags;

/* For var_struct.flags */
#define VAR_HIDDEN  1
#define VAR_HIDING  2
#define VAR_PRIVATE 4

/* Variables */
struct var_struct {
    unsigned char length;
    char name[7];
    int2 level;
    int2 flags;
    vartype *value;
};

/* Programs */
struct prgm_struct {
    int4 capacity;
    int4 size;
    bool lclbl_invalid;
    bool locked;
    unsigned char *text;
    equation_data *eq_data;
    inline bool is_end(int4 pc) {
        return text[pc] == CMD_END && (text[pc + 1] & 112) == 0;
    }
};
struct label_struct {
    unsigned char length;
    char name[7];
    int prgm;
    int4 pc;
};

struct directory;

/* Subdirectories */
struct subdir_struct {
    unsigned char length;
    char name[7];
    directory *dir;
};

extern int local_vars_capacity;
extern int local_vars_count;
extern var_struct *local_vars;

/* Hierarchical storage */
struct directory {
    int id;
    int vars_capacity;
    int vars_count;
    var_struct *vars;
    int prgms_capacity;
    int prgms_count;
    prgm_struct *prgms;
    int labels_capacity;
    int labels_count;
    label_struct *labels;
    int children_capacity;
    int children_count;
    subdir_struct *children;
    directory *parent;
    directory(int id);
    ~directory();
    directory *clone();
};

extern directory *root;
extern directory *cwd;
extern directory *eq_dir;
extern directory **dir_list;
int get_dir_id();
void map_dir(int id, directory *dir);
void unmap_dir(int id);
directory *get_dir(int id);
void dir_list_clear();

extern pgm_index current_prgm;
extern int4 pc;
extern int prgm_highlight_row;

extern vartype *varmenu_eqn;
extern int varmenu_length;
extern char varmenu[7];
extern int varmenu_rows;
extern int varmenu_row;
extern int varmenu_labellength[6];
extern char varmenu_labeltext[6][7];
extern int varmenu_role;


/****************/
/* More globals */
/****************/

extern bool mode_clall;
#define ALL_LINES 9999
extern int mode_message_lines;
extern int (*mode_interruptible)(bool);
extern bool mode_stoppable;
extern bool mode_command_entry;
extern char mode_number_entry;
extern bool mode_alpha_entry;
extern bool mode_shift;
extern int mode_appmenu;
extern int mode_auxmenu;
extern int mode_plainmenu;
extern bool mode_plainmenu_sticky;
extern int mode_transientmenu;
extern int mode_alphamenu;
extern int mode_commandmenu;
extern bool mode_running;
extern bool mode_getkey;
extern bool mode_getkey1;
extern bool mode_pause;
extern bool mode_disable_stack_lift;
extern bool mode_varmenu;
extern int mode_varmenu_whence;
extern bool mode_updown;
extern int4 mode_sigma_reg;
extern int mode_goose;
extern bool mode_time_clktd;
extern bool mode_time_clk24;
extern int mode_wsize;
#if defined(ANDROID) || defined(IPHONE)
extern bool mode_popup_unknown;
#endif
extern bool mode_header;
extern int mode_amort_seq;
extern bool mode_plot_viewer;
extern int mode_plot_key;
extern int mode_plot_sp;
extern vartype *mode_plot_inv;
extern int mode_plot_result_width;
extern bool mode_multi_line;
extern bool mode_lastx_top;
extern bool mode_alpha_top;
extern bool mode_header_flags;
extern bool mode_header_polar;
extern bool mode_matedit_stk;

extern phloat entered_number;
extern int entered_string_length;
extern char entered_string[15];

extern int pending_command;
extern arg_struct pending_command_arg;
extern int xeq_invisible;

/* Multi-keystroke commands -- edit state */
/* Relevant when mode_command_entry != 0 */
extern int incomplete_command;
extern bool incomplete_ind;
extern bool incomplete_alpha;
extern int incomplete_length;
extern int incomplete_maxdigits;
extern int incomplete_argtype;
extern int incomplete_num;
extern char incomplete_str[50];
extern int4 incomplete_saved_pc;
extern int4 incomplete_saved_highlight_row;

#define CATSECT_TOP 0
#define CATSECT_FCN 1
#define CATSECT_PGM 2
#define CATSECT_REAL 3
#define CATSECT_CPX 4
#define CATSECT_MAT 5
#define CATSECT_MORE 6
#define CATSECT_LIST 7
#define CATSECT_EQN 8
#define CATSECT_EQN_NAMED 9
#define CATSECT_PGM_ONLY 10
#define CATSECT_REAL_ONLY 11
#define CATSECT_MAT_ONLY 12
#define CATSECT_EQN_ONLY 13
#define CATSECT_VARS_ONLY 14
#define CATSECT_PGM_SOLVE 15
#define CATSECT_PGM_INTEG 16
#define CATSECT_PGM_MENU 17
#define CATSECT_EXT_1 18
#define CATSECT_EXT_TIME 19
#define CATSECT_EXT_XFCN 20
#define CATSECT_EXT_BASE 21
#define CATSECT_EXT_PRGM 22
#define CATSECT_EXT_STR 23
#define CATSECT_EXT_STK 24
#define CATSECT_EXT_2 25
#define CATSECT_EXT_EQN 26
#define CATSECT_EXT_UNIT 27
#define CATSECT_EXT_STAT 28
#define CATSECT_EXT_DIR 29
#define CATSECT_EXT_DISP 30
#define CATSECT_EXT_MISC 31
#define CATSECT_EXT_0_CMP 32
#define CATSECT_EXT_X_CMP 33
#define CATSECT_UNITS_1 34
#define CATSECT_UNITS_2 35
#define CATSECT_UNITS_3 36
#define CATSECT_UNITS_LENG 37
#define CATSECT_UNITS_AREA 38
#define CATSECT_UNITS_VOL 39
#define CATSECT_UNITS_TIME 40
#define CATSECT_UNITS_SPEED 41
#define CATSECT_UNITS_MASS 42
#define CATSECT_UNITS_FORCE 43
#define CATSECT_UNITS_ENRG 44
#define CATSECT_UNITS_POWR 45
#define CATSECT_UNITS_PRESS 46
#define CATSECT_UNITS_TEMP 47
#define CATSECT_UNITS_ELEC 48
#define CATSECT_UNITS_ANGL 49
#define CATSECT_UNITS_LIGHT 50
#define CATSECT_UNITS_RAD 51
#define CATSECT_UNITS_VISC 52
#define CATSECT_DIRS 53
#define CATSECT_DIRS_ONLY 54
#define CATSECT_OTHER 55
#define CATSECT_LIST_STR_ONLY 56
#define CATSECT_MAT_LIST 57
#define CATSECT_MAT_LIST_ONLY 58
#define CATSECT_LIST_ONLY 59

/* Command line handling temporaries */
extern char cmdline[100];
extern int cmdline_length;
extern int cmdline_unit;

/* Matrix editor / matrix indexing */
extern int matedit_mode; /* 0=off, 1=index, 2=edit, 3=editn */
extern int4 matedit_dir; /* dir <= 0 is local at level -dir */
extern char matedit_name[7];
extern int matedit_length;
extern vartype *matedit_x;
extern int4 matedit_i;
extern int4 matedit_j;
extern int matedit_prev_appmenu;

struct matedit_stack_entry {
    int4 coord;
    int4 anchor;
    void set(int4 coord, int4 anchor) {
        this->coord = coord;
        this->anchor = anchor;
    }
    void set(int8 combined) {
        coord = (int4) combined;
        anchor = (int4) (combined >> 32);
    }
    int8 as_int8() {
        return coord & 0x0ffffffffLL | (((int8) anchor) << 32);
    }
    void set(phloat combined) {
        int8 c = to_int8(combined);
        coord = (int4) c;
        int2 offset = (int2) (c >> 32);
        anchor = coord + offset;
    }
    phloat as_phloat() {
        int4 offset = anchor - coord;
        int8 combined = coord & 0x0ffffffffLL | ((offset & 0x0ffffLL) << 32);
        return phloat(combined);
    }
};

extern matedit_stack_entry *matedit_stack;
extern int matedit_stack_depth;
extern bool matedit_is_list;
extern int4 matedit_view_i;
extern int4 matedit_view_j;

/* INPUT */
extern char input_name[11];
extern int input_length;
extern arg_struct input_arg;

/* ERRMSG/ERRNO */
extern int lasterr;
extern int lasterr_length;
extern char lasterr_text[22];

/* BASE application */
extern int baseapp;

/* Random number generator */
extern int8 random_number_low, random_number_high;

/* NORM & TRACE mode: number waiting to be printed */
extern int deferred_print;

/* Keystroke buffer - holds keystrokes received while
 * there is a program running.
 */
extern int keybuf_head;
extern int keybuf_tail;
extern int keybuf[16];

extern int remove_program_catalog;

#define NUMBER_FORMAT_BINARY 0
#define NUMBER_FORMAT_BID128 1
extern int state_file_number_format;

extern bool no_keystrokes_yet;


/*********************/
/* Utility functions */
/*********************/

int4 new_eqn_idx();
void clear_rtns_vars_and_prgms();
int clear_prgm(const arg_struct *arg);
int clear_prgm_by_int_index(int prgm);
void clear_prgm_lines(int4 count);
void goto_dot_dot(bool force_new);
bool mvar_prgms_exist();
bool label_has_mvar(int4 dir_id, int lblindex);
int get_command_length(pgm_index prgm, int4 pc);
void get_next_command(int4 *pc, int *command, arg_struct *arg, int find_target, const char **num_str);
void rebuild_label_table();
void count_embed_references(directory *dir, int prgm, bool up);
void delete_command(int4 pc);
int eqn_flip(int4 pc);
bool store_command(int4 pc, int command, arg_struct *arg, const char *num_str);
void store_command_after(int4 *pc, int command, arg_struct *arg, const char *num_str);
int x2line();
int a2line(bool append);
int prgm_lock(bool lock);
int4 pc2line(int4 pc);
int4 line2pc(int4 line);
int4 global_pc2line(pgm_index prgm, int4 pc);
int4 global_line2pc(pgm_index prgm, int4 line);
int4 find_local_label(const arg_struct *arg);
bool find_global_label(const arg_struct *arg, pgm_index *prgm, int4 *pc, int *idx = NULL);
int push_rtn_addr(pgm_index prgm, int4 pc);
int push_indexed_matrix();
void maybe_pop_indexed_matrix(const char *name, int len);
int push_func_state(int n);
int push_stack_state(bool big);
int pop_func_state(bool error);
int get_saved_stack_level(int level, vartype **res);
void step_out();
void step_over();
void return_here_after_last_rtn();
void equation_deleted(int eqn_index);
bool unwind_after_eqn_error();
bool should_i_stop_at_this_level();
int rtn(int err);
int rtn_with_error(int err);
bool need_fstart();
void pop_rtn_addr(pgm_index *prgm, int4 *pc, bool *stop);
void clear_all_rtns();
int get_rtn_level();
void save_csld();
bool is_csld();
bool solve_active();
bool integ_active();
bool solve_or_plot_active();
bool unwind_stack_until_solve_or_plot(int *which);

bool read_bool(bool *b);
bool write_bool(bool b);
bool read_char(char *c);
bool write_char(char c);
bool read_int(int *n);
bool write_int(int n);
bool read_int2(int2 *n);
bool write_int2(int2 n);
bool read_int4(int4 *n);
bool write_int4(int4 n);
bool read_int8(int8 *n);
bool write_int8(int8 n);
bool read_phloat(phloat *d);
bool write_phloat(phloat d);
bool read_arg(arg_struct *arg, bool old);
bool write_arg(const arg_struct *arg);

#define TRACK_VAR 0
#define TRACK_STK 1
#define TRACK_IDX 2

bool load_state(bool *clear, bool *too_new);
bool save_state();
// Reason:
// 0 = Memory Clear
// 1 = State File Corrupt
// 2 = State File Too New
void hard_reset(int reason);

bool persist_vartype(vartype *v);
bool unpersist_vartype(vartype **v);

#ifdef IPHONE
bool off_enabled();
#endif

#endif
