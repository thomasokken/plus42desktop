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

#ifndef CORE_HELPERS_H
#define CORE_HELPERS_H 1


#include <stdlib.h>

#include "free42.h"
#include "core_phloat.h"
#include "core_globals.h"


/*********************/
/* Utility functions */
/*********************/

int resolve_ind_arg(arg_struct *arg, char *buf = NULL, int *len = NULL);
int arg_to_num(arg_struct *arg, int4 *num);
int get_arg_equation(arg_struct *arg, vartype_equation **eq);
int recall_result_silently(vartype *v);
int recall_result(vartype *v);
int recall_two_results(vartype *x, vartype *y);
void unary_result(vartype *x);
int unary_two_results(vartype *x, vartype *y);
int unary_no_result();
int binary_result(vartype *x);
void binary_two_results(vartype *x, vartype *y);
int ternary_result(vartype *x);
bool ensure_stack_capacity(int n);
void shrink_stack();
phloat rad_to_angle(phloat x);
phloat rad_to_deg(phloat x);
phloat deg_to_rad(phloat x);
void append_alpha_char(char c);
void append_alpha_string(const char *buf, int buflen, int reverse);

void string_copy(char *dst, unsigned char *dstlen, const char *src, int srclen);
void string_copy(char *dst, unsigned short *dstlen, const char *src, int srclen);
void string_copy(char *dst, int *dstlen, const char *src, int srclen);
bool string_equals(const char *s1, int s1len, const char *s2, int s2len);
int string_pos(const char *ntext, int nlen, const vartype *hs, int startpos);
bool vartype_equals(const vartype *v1, const vartype *v2);
int generic_comparison(const vartype *x, const vartype *y, char which);
int anum(const char *text, int len, phloat *res);

#define undefined_char(c) ((c) >= 135 && (c) != 138)

#define FLAGOP_SF 0
#define FLAGOP_CF 1
#define FLAGOP_FS_T 2
#define FLAGOP_FC_T 3
#define FLAGOP_FSC_T 4
#define FLAGOP_FCC_T 5

int virtual_flag_handler(int flagop, int flagnum);

int get_base();
void set_base(int base, bool a_thru_f = false);
int get_base_param(const vartype *v, int8 *n);
int base_range_check(int8 *n, bool force_wrap);
int effective_wsize();
phloat base2phloat(int8 n);
bool phloat2base(phloat p, int8 *n);

void print_text(const char *text, int length, bool left_justified);
void print_lines(const char *text, int length, bool left_justified);
void print_right(const char *left, int leftlen,
                 const char *right, int rightlen);
void print_wide(const char *left, int leftlen,
                const char *right, int rightlen);
void print_command(int cmd, const arg_struct *arg);
void print_trace();
void print_stack_trace();
void print_one_var(const char *name, int length);
int alpha_print_helper(const char *text, int length);
void alpha_view_helper(const char *text, int length);

void generic_r2p(phloat re, phloat im, phloat *r, phloat *phi);
void generic_p2r(phloat r, phloat phi, phloat *re, phloat *im);

phloat sin_deg(phloat x);
phloat sin_grad(phloat x);
phloat cos_deg(phloat x);
phloat cos_grad(phloat x);

/***********************/
/* Miscellaneous stuff */
/***********************/

class freer {
    private:
    void *p;
    public:
    freer(void *p) : p(p) {}
    ~freer() { free(p); }
};

int dimension_array_ref(vartype *matrix, int4 rows, int4 columns);

phloat fix_hms(phloat x);

void char2buf(char *buf, int buflen, int *bufptr, char c);
void string2buf(char *buf, int buflen, int *bufptr, const char *s, int slen);
int uint2string(uint4 n, char *buf, int buflen);
int int2string(int4 n, char *buf, int buflen);
int ulong2string(uint8 n, char *buf, int buflen);
int vartype2string(const vartype *v, char *buf, int buflen, int max_mant_digits = 12);
const char *phloat2program(phloat d);
int easy_phloat2string(phloat d, char *buf, int buflen, int base_mode);
int real2buf(char *buf, phloat x, const char *format = NULL, bool force_decimal = true);
int ip2revstring(phloat d, char *buf, int buflen);

vartype_list *get_path();
int matedit_get(vartype **res);
void leave_matrix_editor();


#endif
