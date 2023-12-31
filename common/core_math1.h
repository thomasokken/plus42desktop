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

#ifndef CORE_MATH1_H
#define CORE_MATH1_H 1

#include "free42.h"
#include "core_phloat.h"
#include "core_variables.h"

bool persist_math();
bool unpersist_math(int ver);
void reset_math();
void math_equation_deleted(int eqn_index);
void clean_stack(int prev_sp);

struct message_spec {
    const char *text;
    int length;
};

#define SOLVE_ROOT          0
#define SOLVE_SIGN_REVERSAL 1
#define SOLVE_EXTREMUM      2
#define SOLVE_BAD_GUESSES   3
#define SOLVE_CONSTANT      4

extern const message_spec solve_message[];

void put_shadow(const char *name, int length, vartype *value);
vartype *get_shadow(const char *name, int length);
void remove_shadow(const char *name, int length);
void set_solve_prgm(const char *name, int length);
int set_solve_eqn(vartype *eq);
int start_solve(int prev, const char *name, int length, vartype *v1, vartype *v2, vartype **saved_inv = NULL);
int return_to_solve(bool failure, bool stop);
bool is_solve_var(const char *name, int length);

void set_integ_prgm(const char *name, int length);
int set_integ_eqn(vartype *eq);
void get_integ_prgm_eqn(char *name, int *length, vartype **eqn);
void set_integ_var(const char *name, int length);
void get_integ_var(char *name, int *length);
int start_integ(int prev, const char *name, int length, vartype *solve_info = NULL);
int return_to_integ(bool stop);

#endif
