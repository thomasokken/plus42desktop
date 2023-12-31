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

#ifndef CORE_COMMANDS9_H
#define CORE_COMMANDS9_H 1

#include "free42.h"
#include "core_globals.h"

int docmd_crdir(arg_struct *arg);
int docmd_pgdir(arg_struct *arg);
int docmd_chdir(arg_struct *arg);
int docmd_updir(arg_struct *arg);
int docmd_home(arg_struct *arg);
int docmd_path(arg_struct *arg);
int docmd_rename(arg_struct *arg);
int docmd_refmove(arg_struct *arg);
int docmd_refcopy(arg_struct *arg);
int docmd_reffind(arg_struct *arg);
int docmd_prall(arg_struct *arg);

int docmd_width(arg_struct *arg);
int docmd_height(arg_struct *arg);
int docmd_header(arg_struct *arg);
int docmd_row_plus(arg_struct *arg);
int docmd_row_minus(arg_struct *arg);
int docmd_col_plus(arg_struct *arg);
int docmd_col_minus(arg_struct *arg);
int docmd_getds(arg_struct *arg);
int docmd_setds(arg_struct *arg);
int docmd_1line(arg_struct *arg);
int docmd_nline(arg_struct *arg);
int docmd_ltop(arg_struct *arg);
int docmd_atop(arg_struct *arg);
int docmd_hflags(arg_struct *arg);
int docmd_hpolar(arg_struct *arg);
int docmd_stk(arg_struct *arg);

int docmd_dirs(arg_struct *arg);
int docmd_dir_fcn(arg_struct *arg);
int docmd_units(arg_struct *arg);
int docmd_unit_fcn(arg_struct *arg);

int docmd_sppv(arg_struct *arg);
int docmd_spfv(arg_struct *arg);
int docmd_uspv(arg_struct *arg);
int docmd_usfv(arg_struct *arg);
int docmd_gen_n(arg_struct *arg);
int docmd_gen_i(arg_struct *arg);
int docmd_gen_pv(arg_struct *arg);
int docmd_gen_pmt(arg_struct *arg);
int docmd_gen_fv(arg_struct *arg);

int docmd_tvm(arg_struct *arg);
int docmd_eqn(arg_struct *arg);
int docmd_eqn_fcn(arg_struct *arg);
int docmd_n(arg_struct *arg);
int docmd_i_pct_yr(arg_struct *arg);
int docmd_pv(arg_struct *arg);
int docmd_pmt(arg_struct *arg);
int docmd_fv(arg_struct *arg);
int docmd_p_per_yr(arg_struct *arg);
int docmd_amort(arg_struct *arg);
int docmd_tbegin(arg_struct *arg);
int docmd_tend(arg_struct *arg);
int docmd_tclear(arg_struct *arg);
int docmd_treset(arg_struct *arg);
int docmd_tnum_p(arg_struct *arg);
int docmd_tint(arg_struct *arg);
int docmd_tprin(arg_struct *arg);
int docmd_tbal(arg_struct *arg);
int docmd_tnext(arg_struct *arg);
int docmd_tfirst(arg_struct *arg);
int docmd_tlast(arg_struct *arg);
int docmd_tincr(arg_struct *arg);
int docmd_tgo(arg_struct *arg);

void display_amort_status(int key);
void display_amort_table_param(int key);

#endif
