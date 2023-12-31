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

#ifndef CORE_COMMANDS8_H
#define CORE_COMMANDS8_H 1

#include <string>

#include "free42.h"
#include "core_globals.h"

int docmd_parse(arg_struct *arg);
int docmd_unparse(arg_struct *arg);
int docmd_eval(arg_struct *arg);
int docmd_evaln(arg_struct *arg);
int docmd_evalni(arg_struct *arg);
int docmd_eqn_t(arg_struct *arg);
int docmd_std(arg_struct *arg);
int docmd_comp(arg_struct *arg);
int docmd_direct(arg_struct *arg);
int docmd_numeric(arg_struct *arg);
int docmd_gtol(arg_struct *arg);
int docmd_xeql(arg_struct *arg);
int docmd_gsto(arg_struct *arg);
int docmd_grcl(arg_struct *arg);
int docmd_svar(arg_struct *arg);
int docmd_getitem(arg_struct *arg);
int docmd_putitem(arg_struct *arg);
int docmd_gen_eq(arg_struct *arg);
int docmd_gen_ne(arg_struct *arg);
int docmd_gen_lt(arg_struct *arg);
int docmd_gen_gt(arg_struct *arg);
int docmd_gen_le(arg_struct *arg);
int docmd_gen_ge(arg_struct *arg);
int docmd_gen_and(arg_struct *arg);
int docmd_gen_or(arg_struct *arg);
int docmd_gen_xor(arg_struct *arg);
int docmd_gen_not(arg_struct *arg);
int docmd_if_t(arg_struct *arg);
int docmd_geteqn(arg_struct *arg);
int docmd_to_par(arg_struct *arg);
int docmd_fstart(arg_struct *arg);
int docmd_fstack(arg_struct *arg);

int docmd_sn(arg_struct *arg);
int docmd_sx(arg_struct *arg);
int docmd_sx2(arg_struct *arg);
int docmd_sy(arg_struct *arg);
int docmd_sy2(arg_struct *arg);
int docmd_sxy(arg_struct *arg);
int docmd_slnx(arg_struct *arg);
int docmd_slnx2(arg_struct *arg);
int docmd_slny(arg_struct *arg);
int docmd_slny2(arg_struct *arg);
int docmd_slnxlny(arg_struct *arg);
int docmd_sxlny(arg_struct *arg);
int docmd_sylnx(arg_struct *arg);

int convert_helper(const vartype *X, const vartype *Y, phloat *res);
bool normalize_unit(std::string s, std::string *r);
bool is_unit(const char *text, int length);
int unit_compare(const vartype *x, const vartype *y, char which);
int unit_add(const vartype *x, const vartype *y, vartype **r);
int unit_sub(const vartype *x, const vartype *y, vartype **r);
int unit_mul(const vartype *x, const vartype *y, vartype **r);
int unit_div(const vartype *x, const vartype *y, vartype **r);
int unit_mod(const vartype *x, const vartype *y, vartype **r);
int unit_pow(vartype *x, phloat e, vartype **r);
int unit_to_angle(vartype *x, phloat *a);

int docmd_convert(arg_struct *arg);
int docmd_ubase(arg_struct *arg);
int docmd_uval(arg_struct *arg);
int docmd_ufact(arg_struct *arg);
int docmd_to_unit(arg_struct *arg);
int docmd_from_unit(arg_struct *arg);
int docmd_n_plus_u(arg_struct *arg);
int docmd_unit_t(arg_struct *arg);

#endif
