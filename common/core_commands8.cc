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

#include <string.h>

#include "core_commands8.h"
#include "core_display.h"
#include "core_equations.h"
#include "core_globals.h"
#include "core_helpers.h"
#include "core_main.h"
#include "core_math1.h"

int docmd_parse(arg_struct *arg) {
    vartype_string *s = (vartype_string *) stack[sp];
    int errpos;
    vartype *eq = new_equation(s->txt(), s->length, flags.f.eqn_compat, &errpos);
    if (eq == NULL)
        return errpos == -1 ? ERR_INSUFFICIENT_MEMORY : ERR_PARSE_ERROR;
    unary_result(eq);
    return ERR_NONE;
}

int docmd_unparse(arg_struct *arg) {
    vartype_equation *eq = (vartype_equation *) stack[sp];
    equation_data *eqd = eq->data;
    vartype *v = new_string(eqd->text, eqd->length);
    if (v == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    unary_result(v);
    return ERR_NONE;
}

int docmd_eval(arg_struct *arg) {
    if (!ensure_var_space(1))
        return ERR_INSUFFICIENT_MEMORY;
    vartype_equation *eq = (vartype_equation *) stack[sp];
    if (program_running()) {
        int err = push_rtn_addr(current_prgm, pc);
        if (err != ERR_NONE)
            return err;
        current_prgm.set(eq_dir->id, eq->data->eqn_index);
        pc = 0;
        store_stack_reference((vartype *) eq);
        return ERR_NONE;
    } else {
        clear_all_rtns();
        return_here_after_last_rtn();
        current_prgm.set(eq_dir->id, eq->data->eqn_index);
        pc = 0;
        store_stack_reference((vartype *) eq);
        return ERR_RUN;
    }
}

int docmd_evaln(arg_struct *arg) {
    vartype_equation *eq;
    int err = get_arg_equation(arg, &eq);
    if (err != ERR_NONE)
        return err;
    if (!ensure_var_space(1))
        return ERR_INSUFFICIENT_MEMORY;
    if (program_running()) {
        int err = push_rtn_addr(current_prgm, pc);
        if (err != ERR_NONE)
            return err;
        current_prgm.set(eq_dir->id, eq->data->eqn_index);
        pc = 0;
        store_stack_reference((vartype *) eq);
        return ERR_NONE;
    } else {
        clear_all_rtns();
        return_here_after_last_rtn();
        current_prgm.set(eq_dir->id, eq->data->eqn_index);
        pc = 0;
        store_stack_reference((vartype *) eq);
        return ERR_RUN;
    }
}

int docmd_evalni(arg_struct *arg) {
    if (arg->type == ARGTYPE_EQN) {
        clear_all_rtns();
        return_here_after_last_rtn();
        if (eqn_active()) {
            eqn_end();
            pgm_index idx;
            idx.set(0, -4);
            push_rtn_addr(idx, 0);
        }
        current_prgm.set(eq_dir->id, eq_dir->prgms[arg->val.num].eq_data->eqn_index);
        pc = 0;
        return ERR_RUN;
    } else
        return ERR_INVALID_TYPE;
}

int docmd_eqn_t(arg_struct *arg) {
    return stack[sp]->type == TYPE_EQUATION ? ERR_YES : ERR_NO;
}

int docmd_std(arg_struct *arg) {
    flags.f.eqn_compat = 0;
    return ERR_NONE;
}

int docmd_comp(arg_struct *arg) {
    flags.f.eqn_compat = 1;
    return ERR_NONE;
}

int docmd_direct(arg_struct *arg) {
    flags.f.direct_solver = 1;
    return ERR_NONE;
}

int docmd_numeric(arg_struct *arg) {
    flags.f.direct_solver = 0;
    return ERR_NONE;
}

int docmd_gtol(arg_struct *arg) {
    int running = program_running();
    if (!running)
        clear_all_rtns();

    if (!running || arg->target == -1)
        arg->target = line2pc(arg->val.num);
    pc = arg->target;
    move_prgm_highlight(1);
    return ERR_NONE;
}

int docmd_xeql(arg_struct *arg) {
    if (program_running()) {
        int err = push_rtn_addr(current_prgm, pc);
        if (err != ERR_NONE)
            return err;
        err = docmd_gtol(arg);
        if (err != ERR_NONE) {
            pgm_index dummy1;
            int4 dummy2;
            bool dummy3;
            pop_rtn_addr(&dummy1, &dummy2, &dummy3);
        } else
            save_csld();
        return err;
    } else {
        int err = docmd_gtol(arg);
        if (err != ERR_NONE)
            return err;
        clear_all_rtns();
        save_csld();
        return ERR_RUN;
    }
}

int docmd_gsto(arg_struct *arg) {
    if (arg->type != ARGTYPE_STR)
        return ERR_INVALID_TYPE;
    /* Only allow matrices to be stored in "REGS" */
    if (string_equals(arg->val.text, arg->length, "REGS", 4)
            && stack[sp]->type != TYPE_REALMATRIX
            && stack[sp]->type != TYPE_COMPLEXMATRIX)
        return ERR_RESTRICTED_OPERATION;
    vartype *newval = dup_vartype(stack[sp]);
    if (newval == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    int err = store_var(arg->val.text, arg->length, newval, false, true);
    if (err != ERR_NONE)
        free_vartype(newval);
    return err;
}

int docmd_grcl(arg_struct *arg) {
    if (arg->type != ARGTYPE_STR)
        return ERR_INVALID_TYPE;
    vartype *v = recall_global_var(arg->val.text, arg->length);
    if (v == NULL)
        v = new_real(0);
    else
        v = dup_vartype(v);
    if (v == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    return recall_result(v);
}

int docmd_svar(arg_struct *arg) {
    bool ret = solve_active();
    if (ret) {
        vartype_string *s = (vartype_string *) stack[sp];
        ret = is_solve_var(s->txt(), s->length);
    }
    vartype *v = new_real(ret ? 1 : 0);
    if (v == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    unary_result(v);
    return ERR_NONE;
}

static int item_helper(arg_struct *arg, bool get) {
    int off = get ? 0 : 1;
    int spo = sp - off;
    bool two_d;
    vartype_string *name;
    if (stack[spo]->type == TYPE_STRING)
        return ERR_ALPHA_DATA_IS_INVALID;
    if (stack[spo]->type != TYPE_REAL)
        return ERR_INVALID_TYPE;
    if (stack[spo - 1]->type == TYPE_STRING) {
        two_d = false;
        name = (vartype_string *) stack[spo - 1];
    } else if (stack[spo - 1]->type != TYPE_REAL) {
        return ERR_INVALID_TYPE;
    } else {
        two_d = true;
        if (spo < 2)
            return ERR_TOO_FEW_ARGUMENTS;
        if (stack[spo - 2]->type != TYPE_STRING)
            return ERR_INVALID_TYPE;
        name = (vartype_string *) stack[spo - 2];
    }

    vartype *v = recall_var(name->txt(), name->length);
    if (v == NULL)
        return ERR_NONEXISTENT;

    if (get) {
        if (v->type != TYPE_REALMATRIX
                && v->type != TYPE_COMPLEXMATRIX
                && (two_d || v->type != TYPE_LIST))
            return ERR_INVALID_TYPE;
    } else {
        if (v->type == TYPE_REALMATRIX) {
            if (stack[sp]->type != TYPE_REAL
                    && stack[sp]->type != TYPE_STRING)
                return ERR_INVALID_TYPE;
        } else if (v->type == TYPE_COMPLEXMATRIX) {
            if (stack[sp]->type != TYPE_REAL
                    && stack[sp]->type != TYPE_COMPLEX)
                return ERR_INVALID_TYPE;
        } else if (v->type == TYPE_LIST) {
            if (two_d)
                return ERR_INVALID_TYPE;
        } else {
            return ERR_INVALID_TYPE;
        }
    }

    phloat d = ((vartype_real *) stack[spo])->x;
    if (d <= -2147483648.0 || d >= 2147483648.0)
        return ERR_DIMENSION_ERROR;
    int4 n = to_int4(d);
    if (n < 0)
        n = -n;
    n--;
    if (n < 0)
        return ERR_DIMENSION_ERROR;

    if (two_d) {
        if (v->type == TYPE_LIST)
            return ERR_DIMENSION_ERROR;
        d = ((vartype_real *) stack[spo - 1])->x;
        if (d <= -2147483648.0 || d >= 2147483648.0)
            return ERR_DIMENSION_ERROR;
        int4 m = to_int4(d);
        if (m < 0)
            m = -m;
        m--;
        if (m < 0)
            return ERR_DIMENSION_ERROR;
        int4 cols = v->type == TYPE_REALMATRIX
                ? ((vartype_realmatrix *) v)->columns
                : ((vartype_complexmatrix *) v)->columns;
        if (n >= cols)
            return ERR_DIMENSION_ERROR;
        n += m * cols;
    }

    if (!get && !disentangle(v))
        return ERR_INSUFFICIENT_MEMORY;

    vartype *r = NULL;
    vartype *t1 = NULL, *t2 = NULL;
    if (!flags.f.big_stack) {
        t1 = dup_vartype(stack[REG_T]);
        if (t1 == NULL)
            return ERR_INSUFFICIENT_MEMORY;
        t2 = dup_vartype(stack[REG_T]);
        if (t2 == NULL) {
            free_vartype(t1);
            return ERR_INSUFFICIENT_MEMORY;
        }
    }

    switch (v->type) {
        case TYPE_REALMATRIX: {
            vartype_realmatrix *rm;
            rm = (vartype_realmatrix *) v;
            if (n >= rm->rows * rm->columns) {
                dim_fail:
                free_vartype(t1);
                free_vartype(t2);
                return ERR_DIMENSION_ERROR;
            }
            if (get) {
                if (rm->array->is_string[n]) {
                    const char *text;
                    int4 len;
                    get_matrix_string(rm, n, &text, &len);
                    r = new_string(text, len);
                } else {
                    r = new_real(rm->array->data[n]);
                }
                if (r == NULL)
                    return ERR_INSUFFICIENT_MEMORY;
            } else {
                if (stack[sp]->type == TYPE_REAL) {
                    if (rm->array->is_string[n] == 2)
                        free(*(void **) &rm->array->data[n]);
                    rm->array->data[n] = ((vartype_real *) stack[sp])->x;
                    rm->array->is_string[n] = 0;
                } else {
                    vartype_string *vs;
                    vs = (vartype_string *) stack[sp];
                    if (!put_matrix_string(rm, n, vs->txt(), vs->length)) {
                        put_fail:
                        free_vartype(t1);
                        free_vartype(t2);
                        return ERR_INSUFFICIENT_MEMORY;
                    }
                }
            }
            break;
        }
        case TYPE_COMPLEXMATRIX: {
            vartype_complexmatrix *cm = (vartype_complexmatrix *) v;
            if (n >= cm->rows * cm->columns)
                goto dim_fail;
            if (get) {
                r = new_complex(cm->array->data[2 * n], cm->array->data[2 * n + 1]);
            } else {
                if (stack[sp]->type == TYPE_REAL) {
                    cm->array->data[2 * n] = ((vartype_real *) stack[sp])->x;
                    cm->array->data[2 * n + 1] = 0;
                } else {
                    cm->array->data[2 * n] = ((vartype_complex *) stack[sp])->re;
                    cm->array->data[2 * n + 1] = ((vartype_complex *) stack[sp])->im;
                }
            }
            break;
        }
        case TYPE_LIST: {
            vartype_list *list = (vartype_list *) v;
            if (get) {
                if (n >= list->size)
                    goto dim_fail;
                r = dup_vartype(list->array->data[n]);
            } else {
                vartype *v2 = dup_vartype(stack[sp]);
                if (v2 == NULL)
                    goto put_fail;
                if (n >= list->size) {
                    vartype **new_data = (vartype **) realloc(list->array->data, (n + 1) * sizeof(vartype *));
                    if (new_data == NULL)
                        goto put_fail;
                    for (int i = list->size; i < n; i++) {
                        new_data[i] = new_real(0);
                        if (new_data[i] == NULL) {
                            while (--i >= list->size)
                                free_vartype(new_data[i]);
                            new_data = (vartype **) realloc(new_data, list->size * sizeof(vartype *));
                            if (new_data != NULL || list->size == 0)
                                list->array->data = new_data;
                            goto put_fail;
                        }
                    }
                    list->array->data = new_data;
                    list->size = n + 1;
                } else {
                    free_vartype(list->array->data[n]);
                }
                list->array->data[n] = v2;
            }
            break;
        }
    }

    if (get) {
        if (r == NULL)
            return ERR_INSUFFICIENT_MEMORY;
        if (two_d)
            return ternary_result(r);
        else
            return binary_result(r);
    } else if (flags.f.big_stack) {
        free_vartype(stack[sp - 1]);
        free_vartype(stack[sp - 2]);
        if (two_d) {
            free_vartype(stack[sp - 3]);
            stack[sp - 3] = stack[sp];
            sp -= 3;
        } else {
            stack[sp - 2] = stack[sp];
            sp -= 2;
        }
        return ERR_NONE;
    } else {
        // In the two_d case, I'm actually consuming Y, Z, and T,
        // but I need something to duplicate, so I duplicate T anyway.
        free_vartype(stack[REG_Z]);
        stack[REG_Z] = t1;
        free_vartype(stack[REG_Y]);
        stack[REG_Y] = t2;
    }
    return ERR_NONE;
}

int docmd_getitem(arg_struct *arg) {
    return item_helper(arg, true);
}

int docmd_putitem(arg_struct *arg) {
    return item_helper(arg, false);
}

static int maybe_binary_result(vartype *v) {
    if (v == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    else
        return binary_result(v);
}

/* Need this because numeric comparisons will return errors for mismatched units */
static int maybe_binary_or_error(int err) {
    if (err == ERR_YES || err == ERR_NO) {
        vartype *v = new_real(err == ERR_YES);
        if (v == NULL)
            return ERR_INSUFFICIENT_MEMORY;
        else
            return binary_result(v);
    } else
        return err;
}

int docmd_gen_eq(arg_struct *arg) {
    return maybe_binary_result(new_real(vartype_equals(stack[sp - 1], stack[sp])));
}

int docmd_gen_ne(arg_struct *arg) {
    return maybe_binary_result(new_real(!vartype_equals(stack[sp - 1], stack[sp])));
}

int docmd_gen_lt(arg_struct *arg) {
    return maybe_binary_or_error(generic_comparison(stack[sp - 1], stack[sp], 'L'));
}

int docmd_gen_gt(arg_struct *arg) {
    return maybe_binary_or_error(generic_comparison(stack[sp - 1], stack[sp], 'G'));
}

int docmd_gen_le(arg_struct *arg) {
    return maybe_binary_or_error(generic_comparison(stack[sp - 1], stack[sp], 'l'));
}

int docmd_gen_ge(arg_struct *arg) {
    return maybe_binary_or_error(generic_comparison(stack[sp - 1], stack[sp], 'g'));
}

int docmd_gen_and(arg_struct *arg) {
    vartype *v = new_real(((vartype_real *) stack[sp - 1])->x != 0 && ((vartype_real *) stack[sp])->x != 0);
    return maybe_binary_result(v);
}

int docmd_gen_or(arg_struct *arg) {
    vartype *v = new_real(((vartype_real *) stack[sp - 1])->x != 0 || ((vartype_real *) stack[sp])->x != 0);
    return maybe_binary_result(v);
}

int docmd_gen_xor(arg_struct *arg) {
    vartype *v = new_real((((vartype_real *) stack[sp - 1])->x != 0) != (((vartype_real *) stack[sp])->x != 0));
    return maybe_binary_result(v);
}

int docmd_gen_not(arg_struct *arg) {
    vartype *v = new_real(((vartype_real *) stack[sp])->x == 0);
    if (v == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    unary_result(v);
    return ERR_NONE;
}

int docmd_if_t(arg_struct *arg) {
    bool ret = ((vartype_real *) stack[sp])->x != 0;
    int err = unary_no_result();
    return err != ERR_NONE ? err : ret ? ERR_YES : ERR_NO;
}

int docmd_geteqn(arg_struct *arg) {
    vartype_string *s = (vartype_string *) stack[sp];
    equation_data *eqd = find_equation_data(s->txt(), s->length);
    if (eqd == NULL)
        return ERR_NONEXISTENT;
    vartype *eq = new_equation(eqd);
    if (eq == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    unary_result(eq);
    return ERR_NONE;
}

int docmd_to_par(arg_struct *arg) {
    return store_params();
}

int docmd_fstart(arg_struct *arg) {
    if (!program_running())
        return ERR_RESTRICTED_OPERATION;
    if (!need_fstart())
        return ERR_NONE;
    int err = push_func_state(1);
    if (err != ERR_NONE)
        return err;
    if (flags.f.big_stack)
        return ERR_NONE;
    else
        return push_stack_state(true);
}

int docmd_fstack(arg_struct *arg) {
    phloat plevel = ((vartype_real *) stack[sp])->x;
    if (plevel < 0)
        plevel = -plevel;
    int4 level;
    if (plevel >= 2147483648.0)
        level = 2147483647;
    else
        level = to_int4(plevel);
    vartype *res;
    int err = get_saved_stack_level(level, &res);
    if (err == ERR_NONE)
        unary_result(res);
    return err;
}

static int get_sum(int n) {
    vartype *v = recall_var("REGS", 4);
    if (v == NULL)
        return ERR_SIZE_ERROR;
    else if (v->type != TYPE_REALMATRIX)
        return ERR_INVALID_TYPE;
    vartype_realmatrix *rm = (vartype_realmatrix *) v;
    n += mode_sigma_reg;
    if (n >= rm->rows * rm->columns)
        return ERR_SIZE_ERROR;
    if (rm->array->is_string[n]) {
        char *text;
        int length;
        get_matrix_string(rm, n, &text, &length);
        v = new_string(text, length);
    } else {
        v = new_real(rm->array->data[n]);
    }
    if (v == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    return recall_result(v);
}

int docmd_sn(arg_struct *arg) {
    return get_sum(5);
}

int docmd_sx(arg_struct *arg) {
    return get_sum(0);
}

int docmd_sx2(arg_struct *arg) {
    return get_sum(1);
}

int docmd_sy(arg_struct *arg) {
    return get_sum(2);
}

int docmd_sy2(arg_struct *arg) {
    return get_sum(3);
}

int docmd_sxy(arg_struct *arg) {
    return get_sum(4);
}

int docmd_slnx(arg_struct *arg) {
    return get_sum(6);
}

int docmd_slnx2(arg_struct *arg) {
    return get_sum(7);
}

int docmd_slny(arg_struct *arg) {
    return get_sum(8);
}

int docmd_slny2(arg_struct *arg) {
    return get_sum(9);
}

int docmd_slnxlny(arg_struct *arg) {
    return get_sum(10);
}

int docmd_sxlny(arg_struct *arg) {
    return get_sum(11);
}

int docmd_sylnx(arg_struct *arg) {
    return get_sum(12);
}

//////////////////////////
/////  Unit Support  /////
//////////////////////////

struct unitdef {
    const char *name;
    const char *base;
    uint8 numer;
    uint8 denom;
    int exp;
};

const unitdef units[] = {
    { "m", NULL, 1, 1, 0 },
    { "cm", "m", 1, 1, -2 },
    { "mm", "m", 1, 1, -3 },
    { "yd", "m", 9144, 1, -4 },
    { "ft", "m", 3048, 1, -4 },
    { "in", "m", 254, 1, -4 },
    { "Mpc", "m", 308567818585, 1, 11 }, // Related to au?
    { "pc", "m", 308567818585, 1, 5 }, // Related to au?
    { "lyr", "m", 946052840488, 1, 4 }, // Julian year?
    { "au", "m", 1495979, 1, 5 },
    { "km", "m", 1, 1, 3 },
    { "mi", "m", 1609344, 1, -3 },
    { "nmi", "m", 1852, 1, 0 },
    { "miUS", "m", 6336, 3937, 3 },
    { "chain", "m", 792, 3937, 2 }, // 66 ftUS
    { "rd", "m", 198, 3937, 2 }, // 16.5 ftUS
    { "fath", "m", 72, 3937, 2 }, // 6 ftUS
    { "ftUS", "m", 12, 3937, 2 },
    { "mil", "m", 254, 1, -7 },
    { "\21", "m", 1, 1, -6 },
    { "\24", "m", 1, 1, -10 },
    { "fermi", "m", 1, 1, -15 },
    { "m^2", NULL, 1, 1, 0 },
    { "cm^2", "m^2", 1, 1, -4 },
    { "b", "m^2", 1, 1, -28 },
    { "yd^2", "m^2", 83612736, 1, -8 },
    { "ft^2", "m^2", 9290304, 1, -8 },
    { "in^2", "m^2", 64516, 1, -8 },
    { "km^2", "m^2", 1, 1, 6 },
    { "ha", "m^2", 1, 1, 4 },
    { "a", "m^2", 1, 1, 2 },
    { "mi^2", "m^2", 2589988110336, 1, -6 },
    { "miUS^2", "m^2", 40144896, 15499969, 6 },
    { "acre", "m^2", 627264, 15499969, 5 },
    { "m^3", NULL, 1, 1, 0 },
    { "st", "m^3", 1, 1, 0 },
    { "cm^3", "m^3", 1, 1, -6 },
    { "yd^3", "m^3", 764554857984, 1, -12 },
    { "ft^3", "m^3", 28316846592, 1, -12 },
    { "in^3", "m^3", 16387064, 1, -12 },
    { "l", "m^3", 1, 1, -3 },
    { "galUK", "m^3", 4546092, 1, -9 },
    { "galC", "m^3", 454609, 1, -8 },
    { "gal", "m^3", 3785411784LL, 1, -12 },
    { "qt", "m^3", 946352946, 1, -12 },
    { "pt", "m^3", 473176473, 1, -12 },
    { "ml", "m^3", 1, 1, -6 },
    { "cu", "m^3", 2365882365LL, 1, -13 },
    { "ozfl", "m^3", 295735295625, 1, -16 },
    { "ozUK", "m^3", 28413075, 1, -12 },
    { "tbsp", "m^3", 1478676478125, 1, -17 },
    { "tsp", "m^3", 492892159375, 1, -17 },
    { "bbl", "m^3", 158987294928, 1, -12 }, // 42 gal
    { "bu", "m^3", 3523907, 1, -8 },
    { "pk", "m^3", 88097675, 1, -10 },
    { "fbm", "m^3", 2359737216LL, 1, -12 }, // 144 in^3
    { "yr", "s", 315569259747, 1, -4 }, // tropical year
    { "d", "s", 864, 1, 2 },
    { "h", "s", 36, 1, 2 },
    { "min", "s", 6, 1, 1 },
    { "s", NULL, 1, 1, 0 },
    { "Hz", "1/s", 1, 1, 0 },
    { "m/s", NULL, 1, 1, 0 },
    { "cm/s", "m/s", 1, 1, -2 },
    { "ft/s", "m/s", 3048, 1, -4 },
    { "kph", "m/s", 1, 36, 1 },
    { "mph", "m/s", 44704, 1, -5 },
    { "knot", "m/s", 463, 900, 0 },
    { "c", "m/s", 299792458, 1, 0 },
    { "ga", "m/s^2", 980665, 1, -5 },
    { "kg", NULL, 1, 1, 0 },
    { "g", "kg", 1, 1, -3 },
    { "lb", "kg", 45359237, 1, -8 },
    { "oz", "kg", 28349523125, 1, -12 },
    { "slug", "kg", 145939029372, 1, -10 },
    { "lbt", "kg", 3732417216LL, 1, -10 },
    { "ton", "kg", 90718474, 1, -5 }, // 2000 lb
    { "tonUK", "kg", 10160469088, 1, -7 },
    { "t", "kg", 1, 1, 3 },
    { "ozt", "kg", 311034768, 1, -10 },
    { "ct", "kg", 2, 1, -4 },
    { "grain", "kg", 6479891, 1, -11 },
    { "u", "kg", 16605402, 1, -34 },
    { "mol", NULL, 1, 1, 0 },
    { "N", "kg*m/s^2", 1, 1, 0 },
    { "dyn", "kg*m/s^2", 1, 1, -5 },
    { "gf", "kg*m/s^2", 980665, 1, -8 },
    { "kip", "kg*m/s^2", 444822161526, 1, -8 },
    { "lbf", "kg*m/s^2", 444822161526, 1, -11 },
    { "pdl", "kg*m/s^2", 138254954376, 1, -12 },
    { "J", "kg*m^2/s^2", 1, 1, 0 },
    { "erg", "kg*m^2/s^2", 1, 1, -7 },
    { "kcal", "kg*m^2/s^2", 41868, 1, -1 },
    { "cal", "kg*m^2/s^2", 41868, 1, -4 },
    { "Btu", "kg*m^2/s^2", 105505585262, 1, -8 },
    { "ft*lbf", "kg*m^2/s^2", 135581794833, 1, -11 },
    { "therm", "kg*m^2/s^2", 105506, 1, 3 },
    { "MeV", "kg*m^2/s^2", 160217733, 1, -21 },
    { "eV", "kg*m^2/s^2", 160217733, 1, -27 },
    { "W", "kg*m^2/s^3", 1, 1, 0 },
    { "hp", "kg*m^2/s^3", 745699871582, 1, -9 },
    { "Pa", "kg/(m*s^2)", 1, 1, 0 },
    { "atm", "kg/(m*s^2)", 101325, 1, 0 },
    { "bar", "kg/(m*s^2)", 100000, 1, 0 },
    { "psi", "kg/(m*s^2)", 689475729317, 1, -8 },
    { "torr", "kg/(m*s^2)", 133322368421, 1, -9 },
    { "mmHg", "kg/(m*s^2)", 133322368421, 1, -9 },
    { "inHg", "kg/(m*s^2)", 338638815789, 1, -8 },
    { "inH2O", "kg/(m*s^2)", 24884, 1, -2 },
    { "\23C", "K", 1, 1, 0 }, // offset = 27315 / 100
    { "\23F", "K", 5, 9, 0 }, // offset = 229835 / 900
    { "K", NULL, 1, 1, 0 },
    { "\23R", "K", 5, 9, 0 },
    { "V", "kg*m^2/(A*s^3)", 1, 1, 0 },
    { "A", NULL, 1, 1, 0 },
    { "C", "A*s", 1, 1, 0 },
    { "\202", "kg*m^2/(A^2*s^3)", 1, 1, 0 },
    { "F", "A^2*s^4/(kg*m^2)", 1, 1, 0 },
    { "W", "kg*m^2/s^3", 1, 1, 0 },
    { "Fdy", "A*s", 96487, 1, 0 },
    { "H", "kg*m^2/(A^2*s^2)", 1, 1, 0 },
    { "mho", "A^2*s^3/(kg*m^2)", 1, 1, 0 },
    { "S", "A^2*s^3/(kg*m^2)", 1, 1, 0 },
    { "T", "kg/(A*s^2)", 1, 1, 0 },
    { "Wb", "kg*m^2/(A*s^2)", 1, 1, 0 },
    { "\23", "r", 0, 18, -1 }, // pi
    { "r", NULL, 1, 1, 0 },
    { "grad", "r", 0, 2, -2 }, // pi
    { "arcmin", "r", 0, 108, -2 }, // pi
    { "arcs", "r", 0, 648, -3 }, // pi
    { "sr", NULL, 1, 1, 0 },
    { "fc", "cd*sr/m^2", 107639104167, 1, -10 },
    { "flam", "cd/m^2", 342625909964, 1, -11 },
    { "lx", "cd*sr/m^2", 1, 1, 0 },
    { "ph", "cd*sr/m^2", 1, 1, 4 },
    { "sb", "cd/m^2", 1, 1, 4 },
    { "lm", "cd*sr", 1, 1, 0 },
    { "cd", NULL, 1, 1, 0 },
    { "lam", "cd/m^2", 1, 0, 4 }, // pi
    { "Gy", "m^2/s^2", 1, 1, 0 },
    { "rad", "m^2/s^2", 1, 1, -2 },
    { "rem", "m^2/s^2", 1, 1, -2 },
    { "Sv", "m^2/s^2", 1, 1, 0 },
    { "Bq", "1/s", 1, 1, 0 },
    { "Ci", "1/s", 37, 1, 9 },
    { "R", "A*s/kg", 258, 1, -6 },
    { "P", "kg/(m*s)", 1, 1, -1 },
    { "St", "m^2/s", 1, 1, -4 },
    { "one", "1", 1, 1, 0 },
    { NULL, NULL, 0, 0, 0 }
};

static const unitdef *find_unit(std::string s, int *exponent, vartype **user, std::string *un) {
    *exponent = 0;
    while (true) {
        int idx = 0;
        const char *cs = s.c_str();
        while (true) {
            const char *us = units[idx].name;
            if (us == NULL)
                break;
            if (strcmp(us, cs) == 0)
                return units + idx;
            idx++;
        }
        // Not in units table; look for user-defined unit...
        vartype *v = recall_var(s.c_str(), s.length());
        if (v != NULL && (v->type == TYPE_REAL || v->type == TYPE_UNIT)) {
            *user = v;
            *un = s;
            return NULL;
        }
        if (*exponent == -1 && s.length() > 1) {
            // Starts with a 'd' and still has 2 characters or more left;
            // try the 'da' prefix...
            if (s[0] == 'a') {
                *exponent = 1;
                s = s.substr(1, s.length() - 1);
                continue;
            }
        }
        if (*exponent != 0 || s.length() == 1) {
            *user = NULL;
            return NULL;
        }
        char prefix = s[0];
        switch (prefix) {
            case 'Q': *exponent = 30; break;
            case 'R': *exponent = 27; break;
            case 'Y': *exponent = 24; break;
            case 'Z': *exponent = 21; break;
            case 'E': *exponent = 18; break;
            case 'P': *exponent = 15; break;
            case 'T': *exponent = 12; break;
            case 'G': *exponent = 9; break;
            case 'M': *exponent = 6; break;
            case 'k': *exponent = 3; break;
            case 'h': *exponent = 2; break;
            case 'd': *exponent = -1; break;
            case 'c': *exponent = -2; break;
            case 'm': *exponent = -3; break;
            case '\21': *exponent = -6; break;
            case 'n': *exponent = -9; break;
            case 'p': *exponent = -12; break;
            case 'f': *exponent = -15; break;
            case 'a': *exponent = -18; break;
            case 'z': *exponent = -21; break;
            case 'y': *exponent = -24; break;
            case 'r': *exponent = -27; break;
            case 'q': *exponent = -30; break;
            default: *user = NULL; return NULL;
        }
        s = s.substr(1, s.length() - 1);
    }
}

/* Hack to get things to compile in Ubuntu 14.04 */
static std::string to_string(int i) {
    char buf[20];
    snprintf(buf, 20, "%d", i);
    return std::string(buf, strlen(buf));
}

struct UnitProduct {
    std::map<std::string, int> elem;
    UnitProduct() {}
    UnitProduct(const UnitProduct &that) {
        elem = that.elem;
    }
    UnitProduct(std::string s) {
        elem[s] = 1;
    }
    void mul(UnitProduct *that) {
        for (std::map<std::string, int>::iterator iter = that->elem.begin(); iter != that->elem.end(); iter++) {
            std::map<std::string, int>::iterator iter2 = elem.find(iter->first);
            if (iter2 != elem.end())
                iter2->second += iter->second;
            else
                elem[iter->first] = iter->second;
        }
    }
    void pow(int p) {
        for (std::map<std::string, int>::iterator iter = elem.begin(); iter != elem.end(); iter++)
            iter->second *= p;
    }
    bool root(int p) {
        for (std::map<std::string, int>::iterator iter = elem.begin(); iter != elem.end(); iter++)
            if (iter->second % p != 0)
                return false;
            else
                iter->second /= p;
        return true;
    }
    std::string str() {
        std::string numer;
        std::string denom;
        bool compoundDenom = false;
        for (std::map<std::string, int>::iterator iter = elem.begin(); iter != elem.end(); iter++) {
            if (iter->second == 0)
                continue;
            else if (iter->second == 1)
                numer = numer + "*" + iter->first;
            else if (iter->second > 1)
                numer = numer + "*" + iter->first + "^" + to_string(iter->second);
            else {
                if (denom != "")
                    compoundDenom = true;
                if (iter->second == -1)
                    denom = denom + "*" + iter->first;
                else
                    denom = denom + "*" + iter->first + "^" + to_string(-iter->second);
            }
        }
        if (numer != "")
            numer = numer.substr(1, numer.length() - 1);
        if (denom == "") {
            return numer;
        } else {
            denom = denom.substr(1, denom.length() - 1);
            if (compoundDenom)
                denom = std::string("(", 1) + denom + ")";
            if (numer == "")
                return "1/" + denom;
            else
                return numer + "/" + denom;
        }
    }

    bool toBase(phloat *f, std::string *s);
};

class UnitLexer {

    private:

    std::string text;
    int pos, prevpos;

    public:

    UnitLexer(std::string text) {
        this->text = text;
        pos = 0;
        prevpos = 0;
    }

    int lpos() {
        return prevpos;
    }

    int cpos() {
        return pos;
    }

    std::string substring(int start, int end) {
        return text.substr(start, end - start);
    }

    bool isIdentifierStartChar(char c) {
        return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c == '\21' || c == '\23' || c == '\24' || c == '\202';
    }

    bool isIdentifierContinuationChar(char c) {
        return isIdentifierStartChar(c) || isdigit(c);
    }

    bool isIdentifier(std::string s) {
        int len = s.length();
        if (len == 0 || !isIdentifierStartChar(s[0]))
            return false;
        for (int i = 1; i < len; i++)
            if (!isIdentifierContinuationChar(s[i]))
                return false;
        return true;
    }

    bool nextToken(std::string *tok, int *tpos) {
        prevpos = pos;
        while (pos < text.length() && text[pos] == ' ')
            pos++;
        if (pos == text.length()) {
            *tok = "";
            *tpos = pos;
            return true;
        }
        int start = pos;
        *tpos = start;
        char c = text[pos++];
        // Identifiers
        if (isIdentifierStartChar(c)) {
            while (pos < text.length() && isIdentifierContinuationChar(text[pos]))
                pos++;
            *tok = text.substr(start, pos - start);
            return true;
        }
        // Must be a number or a one-character symbol, then
        if (c == '-' || isdigit(c)) {
            // Number...
            bool neg = c == '-';
            while (pos < text.length() && isdigit(text[pos]))
                pos++;
            if (neg && pos == start + 1)
                // Lone minus sign; abort
                return false;
            *tok = text.substr(start, pos - start);
            return true;
        }
        // One-character symbol, by default
        *tok = text.substr(start, 1);
        return true;
    }
};

class UnitParser {
    private:

    UnitLexer *lex;
    std::string pb;
    int pbpos;

    public:

    static UnitProduct *parse(std::string expr, int *errpos) {
        std::string t, t2;
        int tpos;

        UnitLexer *lex = new UnitLexer(expr);
        UnitParser pz(lex);
        UnitProduct *u = pz.parseExpr();
        if (u == NULL) {
            *errpos = pz.lex->lpos();
            return NULL;
        }
        if (!pz.nextToken(&t, &tpos)) {
            delete u;
            return NULL;
        }
        if (t == "") {
            // Text consumed completely; this is the good scenario
            return u;
        } else {
            // Trailing garbage
            delete u;
            *errpos = tpos;
            return NULL;
        }
    }

    UnitParser(UnitLexer *lex) : lex(lex), pbpos(-1) {}

    ~UnitParser() {
        delete lex;
    }

    UnitProduct *parseExpr() {
        std::string t;
        int tpos;
        UnitProduct *u = parseFactor();
        if (u == NULL)
            return NULL;
        while (true) {
            if (!nextToken(&t, &tpos)) {
                fail:
                delete u;
                return NULL;
            }
            if (t == "")
                return u;
            if (t == "*" || t == "/" || t == "\1" || t.length() == 1 && t[0] == 0) {
                UnitProduct *u2 = parseFactor();
                if (u2 == NULL)
                    goto fail;
                if (t == "*" || t == "\1") {
                    u->mul(u2);
                    delete u2;
                } else {
                    u2->pow(-1);
                    u->mul(u2);
                    delete u2;
                }
            } else {
                pushback(t, tpos);
                return u;
            }
        }
    }

    UnitProduct *parseFactor() {
        UnitProduct *u = parseThing();
        if (u == NULL)
            return NULL;
        while (true) {
            std::string t;
            int tpos;
            if (!nextToken(&t, &tpos)) {
                fail:
                delete u;
                return NULL;
            }
            if (t == "^" || t == "\36") {
                std::string t2;
                int t2pos;
                if (!nextToken(&t2, &t2pos))
                    goto fail;
                int p;
                if (!get_int(t2, &p))
                    goto fail;
                u->pow(p);
            } else {
                pushback(t, tpos);
                return u;
            }
        }
    }

    static bool get_int(std::string tok, int *d) {
        char c = tok[0];
        if ((c < '0' || c > '9') && c != '-')
            return false;
        bool neg = c == '-';
        int r = 0;
        for (int i = neg ? 1 : 0; i < tok.length(); i++)
            r = r * 10 + tok[i] - '0';
        *d = neg ? -r : r;
        return true;
    }

    UnitProduct *parseThing() {
        std::string t;
        int tpos;
        if (!nextToken(&t, &tpos) || t == "")
            return NULL;
        int d;
        if (get_int(t, &d)) {
            if (d != 1)
                return NULL;
            else
                return new UnitProduct();
        } else if (t == "(") {
            UnitProduct *u = parseExpr();
            if (u == NULL)
                return NULL;
            std::string t2;
            int t2pos;
            if (!nextToken(&t2, &t2pos) || t2 != ")") {
                delete u;
                return NULL;
            }
            return u;
        } else if (lex->isIdentifier(t)) {
            return new UnitProduct(t);
        } else {
            return NULL;
        }
    }

    bool nextToken(std::string *tok, int *tpos) {
        if (pbpos != -1) {
            *tok = pb;
            *tpos = pbpos;
            pbpos = -1;
            return true;
        } else
            return lex->nextToken(tok, tpos);
    }

    void pushback(std::string o, int p) {
        pb = o;
        pbpos = p;
    }
};

bool UnitProduct::toBase(phloat *f, std::string *s) {
    phloat v = 1;
    int exp = 0;
    std::string us = "";
    for (std::map<std::string, int>::iterator iter = elem.begin(); iter != elem.end(); iter++) {
        int e;
        vartype *user;
        std::string userName;
        const unitdef *ud = find_unit(iter->first, &e, &user, &userName);
        int p = iter->second;
        if (ud == NULL) {
            if (user == NULL)
                return false;
            if (user->type == TYPE_UNIT) {
                vartype_unit *u = (vartype_unit *) user;
                phloat f2;
                std::string s2;
                if (u->x == 0 && string_equals(u->text, u->length, "one", 3)) {
                    // 0_one is magic value indicating a user-defined base unit
                    f2 = 1;
                    s2 = userName;
                } else {
                    int errpos;
                    UnitProduct *up = UnitParser::parse(std::string(u->text, u->length), &errpos);
                    bool success = up->toBase(&f2, &s2);
                    delete up;
                    if (!success)
                        return false;
                    v *= ::pow(((vartype_real *) user)->x * f2, p);
                }
                if (us.length() != 0)
                    us += "*";
                if (p == 1)
                    us += s2;
                else {
                    us += "(";
                    us += s2;
                    us += ")^";
                    us += to_string(p);
                }
            } else {
                v *= ::pow(((vartype_real *) user)->x, p);
            }
            exp += e * p;
        } else {
            const char *un = ud->base == NULL ? ud->name : ud->base;
            if (us.length() != 0)
                us += "*";
            if (p == 1)
                us += un;
            else {
                us += "(";
                us += un;
                us += ")^";
                us += to_string(p);
            }
            exp += (ud->exp + e) * p;
            phloat f;
            if (ud->numer == 0)
                f = PI / ud->denom;
            else if (ud->denom == 0)
                f = ((phloat) ud->numer) / PI;
            else
                f = ((phloat) ud->numer) / ud->denom;
            v *= ::pow(f, p);
        }
    }
    *f = v * ::pow(10, exp);
    normalize_unit(us, s);
    return true;
}

bool normalize_unit(std::string s, std::string *r) {
    int errpos;
    UnitProduct *u = UnitParser::parse(s, &errpos);
    if (u == NULL)
        return false;
    *r = u->str();
    delete u;
    return true;
}

bool is_unit(const char *text, int length) {
    int errpos;
    UnitProduct *up = UnitParser::parse(std::string(text, length), &errpos);
    if (up == NULL)
        return false;
    phloat f = 1;
    std::string s;
    int success = up->toBase(&f, &s);
    delete up;
    return success;
}

static int get_value_and_base(const vartype *v, phloat *value, std::string *baseUnit, phloat *factor) {
    if (v->type == TYPE_REAL) {
        *value = ((vartype_real *) v)->x;
        *baseUnit = "";
        *factor = 1;
        return true;
    }
    vartype_unit *u = (vartype_unit *) v;
    int errpos;
    UnitProduct *up = UnitParser::parse(std::string(u->text, u->length), &errpos);
    if (up == NULL)
        return false;
    bool success = up->toBase(factor, baseUnit);
    delete up;
    *value = u->x;
    return success;
}

static bool equiv_units(const std::string &x, const std::string &y) {
    return x == y || x == "" && y == "r" || x == "r" && y == "";
}

int convert_helper(const vartype *X, const vartype *Y, phloat *res) {
    /* Convert Y to match the units of X; used for CONVERT,
     * addition, and subtraction
     */
    phloat x, y, fx, fy;
    std::string bux, buy;
    if (!get_value_and_base(X, &x, &bux, &fx))
        return ERR_INVALID_UNIT;
    if (!get_value_and_base(Y, &y, &buy, &fy))
        return ERR_INVALID_UNIT;
    if (!equiv_units(bux, buy))
        return ERR_INCONSISTENT_UNITS;
    if (bux == "K") {
        // Look for special cases involving Celsius and Fahrenheit
        vartype_unit *sy = (vartype_unit *) Y;
        vartype_unit *sx = (vartype_unit *) X;
        buy = std::string(sy->text, sy->length);
        bux = std::string(sx->text, sx->length);
        if (bux != buy && (bux == "\23C" || bux == "\23F"
                           || buy == "\23C" || buy == "\23F")) {
            y = sy->x;
            // We know the units are consistent, so the other one can
            // only be another temperature unit
            if (bux == "\23C")
                if (buy == "\23F")
                    *res = (y - 32) / 1.8;
                else if (buy == "K")
                    *res = y - 273.15;
                else // buy == "\23R"
                    *res = y / 1.8 - 273.15;
            else if (bux == "\23F")
                if (buy == "\23C")
                    *res = y * 1.8 + 32;
                else if (buy == "K")
                    *res = y * 1.8 - 459.67;
                else // buy == "\23R"
                    *res = y - 459.67;
            else if (bux == "K")
                if (buy == "\23C")
                    *res = y + 273.15;
                else if (buy == "\23F")
                    *res = (y + 459.67) / 1.8;
                else // buy == "\23R" (can't get here)
                    *res = y / 1.8;
            else // bux == "\23R"
                if (buy == "\23C")
                    *res = (y + 273.15) * 1.8;
                else if (buy == "\23F")
                    *res = y + 459.67;
                else // buy == "K" (can't get here)
                    *res = y * 1.8;
        } else
            goto normal;
    } else {
        normal:
        if (bux == "" && buy == "r")
            fy = rad_to_angle(fy);
        else if (bux == "r" && buy == "")
            fx = rad_to_angle(fx);
        *res = y * fy / fx;
    }
    int inf;
    if ((inf = p_isinf(*res)) != 0) {
        if (flags.f.range_error_ignore)
            *res = inf > 0 ? POS_HUGE_PHLOAT : NEG_HUGE_PHLOAT;
        else
            return ERR_OUT_OF_RANGE;
    }
    return ERR_NONE;
}

int unit_compare(const vartype *x, const vartype *y, char which) {
    char saved_range_error_ignore = flags.f.range_error_ignore;
    flags.f.range_error_ignore = false;
    phloat px, py;
    int err = convert_helper(x, y, &py);
    if (err == ERR_OUT_OF_RANGE) {
        err = convert_helper(y, x, &px);
        py = ((vartype_real *) y)->x;
    } else {
        px = ((vartype_real *) x)->x;
    }
    flags.f.range_error_ignore = saved_range_error_ignore;
    if (err != ERR_NONE)
        return err;
    switch (which) {
        case 'E': return px == py ? ERR_YES : ERR_NO;
        case 'L': return px < py ? ERR_YES : ERR_NO;
        case 'l': return px <= py ? ERR_YES : ERR_NO;
        case 'G': return px > py ? ERR_YES : ERR_NO;
        case 'g': return px >= py ? ERR_YES : ERR_NO;
        default: return ERR_INTERNAL_ERROR;
    }
}

int docmd_convert(arg_struct *arg) {
    phloat res;
    int err = convert_helper(stack[sp], stack[sp - 1], &res);
    if (err != ERR_NONE)
        return err;
    vartype *r;
    if (stack[sp]->type == TYPE_REAL) {
        r = new_real(res);
    } else {
        vartype_unit *u = (vartype_unit *) stack[sp];
        r = new_unit(res, u->text, u->length);
    }
    if (r == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    return binary_result(r);
}

int docmd_ubase(arg_struct *arg) {
    if (stack[sp]->type == TYPE_REAL) {
        vartype *v = dup_vartype(stack[sp]);
        if (v == NULL)
            return ERR_INSUFFICIENT_MEMORY;
        unary_result(v);
        return ERR_NONE;
    }
    phloat x, f;
    std::string bu;
    if (!get_value_and_base(stack[sp], &x, &bu, &f))
        return ERR_INVALID_UNIT;

    phloat r;
    if (bu == "K") {
        vartype_unit *u = (vartype_unit *) stack[sp];
        std::string ou(u->text, u->length);
        if (ou == "\23C")
            r = x + 273.15;
        else if (ou == "\23F")
            r = (x + 459.67) / 1.8;
        else
            goto normal;
    } else {
        normal:
        r = x * f;
    }

    int inf;
    if ((inf = p_isinf(r)) != 0) {
        if (flags.f.range_error_ignore)
            r = inf > 0 ? POS_HUGE_PHLOAT : NEG_HUGE_PHLOAT;
        else
            return ERR_OUT_OF_RANGE;
    }
    vartype *res = new_unit(r, bu.c_str(), bu.length());
    if (res == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    unary_result(res);
    return ERR_NONE;
}

int docmd_uval(arg_struct *arg) {
    vartype *r;
    if (stack[sp]->type == TYPE_REAL)
        r = dup_vartype(stack[sp]);
    else
        r = new_real(((vartype_unit *) stack[sp])->x);
    if (r == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    unary_result(r);
    return ERR_NONE;
}

int docmd_ufact(arg_struct *arg) {
    if (stack[sp]->type == TYPE_REAL)
        return ERR_INVALID_DATA;
    if (stack[sp - 1]->type == TYPE_REAL)
        return binary_result(dup_vartype(stack[sp - 1]));

    vartype_unit *ux = (vartype_unit *) stack[sp];
    vartype_unit *uy = (vartype_unit *) stack[sp - 1];

    int errpos;
    UnitProduct *upx = UnitParser::parse(std::string(ux->text, ux->length), &errpos);
    if (upx == NULL)
        return ERR_INVALID_UNIT;
    UnitProduct *upy = UnitParser::parse(std::string(uy->text, uy->length), &errpos);
    if (upy == NULL) {
        delete upx;
        return ERR_INVALID_UNIT;
    }

    phloat fx, fy;
    std::string bux, buy;
    bool success = upx->toBase(&fx, &bux) && upy->toBase(&fy, &buy);
    delete upx;
    delete upy;
    if (!success)
        return ERR_INVALID_UNIT;

    std::string remUnit;
    remUnit += "(";
    remUnit += buy;
    remUnit += ")/(";
    remUnit += bux;
    remUnit += ")";

    std::string normRemUnit;
    normalize_unit(remUnit, &normRemUnit);
    std::string newUnit(ux->text, ux->length);
    if (normRemUnit.length() == 0)
        /* Nothing to do */;
    else if (normRemUnit[0] == '1')
        newUnit += normRemUnit.substr(1, normRemUnit.length() - 1);
    else {
        newUnit += "*";
        newUnit += normRemUnit;
    }
    phloat r = uy->x / (fx / fy);
    int inf;
    if ((inf = p_isinf(r)) != 0) {
        if (flags.f.range_error_ignore)
            r = inf > 0 ? POS_HUGE_PHLOAT : NEG_HUGE_PHLOAT;
        else
            return ERR_OUT_OF_RANGE;
    }
    vartype *res = new_unit(r, newUnit.c_str(), newUnit.length());
    if (res == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    return binary_result(res);
}

int docmd_to_unit(arg_struct *arg) {
    if ((stack[sp]->type != TYPE_UNIT && stack[sp]->type != TYPE_STRING)
            || stack[sp - 1]->type != TYPE_REAL)
        return ERR_INVALID_TYPE;
    phloat val = ((vartype_real *) stack[sp - 1])->x;
    vartype *r;
    if (stack[sp]->type == TYPE_UNIT) {
        r = dup_vartype(stack[sp]);
        if (r == NULL)
            return ERR_INSUFFICIENT_MEMORY;
        ((vartype_unit *) r)->x = val;
    } else {
        vartype_string *s = (vartype_string *) stack[sp];
        r = new_unit(val, s->txt(), s->length);
        if (r == NULL)
            return ERR_INSUFFICIENT_MEMORY;
    }
    return binary_result(r);
}

int docmd_from_unit(arg_struct *arg) {
    vartype *u = dup_vartype(stack[sp]);
    if (u == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    vartype *r = new_real(((vartype_unit *) u)->x);
    if (r == NULL) {
        free_vartype(u);
        return ERR_INSUFFICIENT_MEMORY;
    }
    ((vartype_unit *) u)->x = 1;
    return unary_two_results(u, r);
}

/* For the following utility functions, it is assumed that
 * neither x nor y are anything other than TYPE_REAL or TYPE_UNIT,
 * and that at least one of them is TYPE_UNIT.
 * The result may be TYPE_UNIT or TYPE_REAL, the latter being
 * possible when a multiplication or division cancels out two
 * units completely.
 */

static int unit_add_sub(const vartype *x, const vartype *y, vartype **r, bool add) {
    phloat vy;
    int err = convert_helper(x, y, &vy);
    if (err != ERR_NONE)
        return err;
    if (x->type == TYPE_REAL) {
        phloat vx = ((vartype_real *) x)->x;
        if (add)
            vy += vx;
        else
            vy -= vx;
        int inf;
        if ((inf = p_isinf(vy)) != 0) {
            if (flags.f.range_error_ignore)
                vy = inf > 0 ? POS_HUGE_PHLOAT : NEG_HUGE_PHLOAT;
            else
                return ERR_OUT_OF_RANGE;
        }
        *r = new_real(vy);
    } else {
        vartype_unit *u = (vartype_unit *) x;
        phloat vx = u->x;
        if (add)
            vy += vx;
        else
            vy -= vx;
        int inf;
        if ((inf = p_isinf(vy)) != 0) {
            if (flags.f.range_error_ignore)
                vy = inf > 0 ? POS_HUGE_PHLOAT : NEG_HUGE_PHLOAT;
            else
                return ERR_OUT_OF_RANGE;
        }
        *r = new_unit(vy, u->text, u->length);
    }
    if (r == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    return ERR_NONE;
}

int unit_add(const vartype *x, const vartype *y, vartype **r) {
    return unit_add_sub(x, y, r, true);
}

int unit_sub(const vartype *x, const vartype *y, vartype **r) {
    return unit_add_sub(x, y, r, false);
}

#define U_MUL 0
#define U_DIV 1
#define U_MOD 2

static int unit_mul_div(const vartype *x, const vartype *y, vartype **r, int mode) {
    phloat vx, vy;
    std::string ux, uy;
    if (x->type == TYPE_REAL) {
        vx = ((vartype_real *) x)->x;
        ux = "";
    } else {
        vartype_unit *u = (vartype_unit *) x;
        vx = u->x;
        ux = std::string(u->text, u->length);
    }
    if (y->type == TYPE_REAL) {
        vy = ((vartype_real *) y)->x;
        uy = "";
    } else {
        vartype_unit *u = (vartype_unit *) y;
        vy = u->x;
        uy = std::string(u->text, u->length);
    }

    std::string ru, nru;
    if (mode == U_MUL)
        ru = uy == "" ? ux : ux == "" ? uy : (uy + "*" + ux);
    else
        ru = uy == "" ? ("1/(" + ux + ")") : ux == "" ? uy : (uy + "/(" + ux + ")");
    if (!normalize_unit(ru, &nru))
        return ERR_INVALID_UNIT;

    phloat res;
    switch (mode) {
        case U_MUL:
            res = vy * vx;
            break;
        case U_DIV:
            if (vx == 0)
                return ERR_DIVIDE_BY_0;
            res = vy / vx;
            break;
        case U_MOD:
            if (vx == 0)
                res = vy;
            else if (vy == 0)
                res = 0;
            else {
                res = fmod(vy, vx);
                if (res != 0 && ((vx > 0 && vy < 0) || (vx < 0 && vy > 0)))
                    res += vx;
            }
            break;
    }
    int inf;
    if ((inf = p_isinf(res)) != 0) {
        if (flags.f.range_error_ignore)
            res = inf < 0 ? NEG_HUGE_PHLOAT : POS_HUGE_PHLOAT;
        else
            return ERR_OUT_OF_RANGE;
    }
    *r = new_unit(res, nru.c_str(), nru.length());
    if (*r == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    return ERR_NONE;
}

int unit_mul(const vartype *x, const vartype *y, vartype **r) {
    return unit_mul_div(x, y, r, U_MUL);
}

int unit_div(const vartype *x, const vartype *y, vartype **r) {
    return unit_mul_div(x, y, r, U_DIV);
}

int unit_mod(const vartype *x, const vartype *y, vartype **r) {
    return unit_mul_div(x, y, r, U_MOD);
}

int unit_pow(vartype *x, phloat e, vartype **r) {
    vartype_unit *u = (vartype_unit *) x;
    int4 ie = to_int4(e);
    UnitProduct *up;
    phloat e1;
    if (e == ie) {
        int errpos;
        up = UnitParser::parse(std::string(u->text, u->length), &errpos);
        if (up == NULL)
            return ERR_INVALID_UNIT;
        up->pow(ie);
        goto finish;
    } else {
        ie = to_int4(e > 0 ? 1 / e + 0.5 : 1 / e - 0.5);
        e1 = 1 / ((phloat) ie);
        if (e1 != e && e1 != nextafter(e, POS_HUGE_PHLOAT) && e1 != nextafter(e, NEG_HUGE_PHLOAT))
            // Not close enough to the reciprocal of an integer
            return ERR_INVALID_DATA;
        if (u->x < 0)
            return ERR_INVALID_DATA;
        int errpos;
        up = UnitParser::parse(std::string(u->text, u->length), &errpos);
        if (up == NULL)
            return ERR_INVALID_UNIT;
        if (!up->root(ie)) {
            delete up;
            return ERR_INCONSISTENT_UNITS;
        }
        finish:
        std::string nu = up->str();
        delete up;
        phloat res = pow(u->x, e);
        int inf;
        if ((inf = p_isinf(res)) != 0) {
            if (flags.f.range_error_ignore)
                res = inf < 0 ? NEG_HUGE_PHLOAT : POS_HUGE_PHLOAT;
            else
                return ERR_OUT_OF_RANGE;
        }
        *r = new_unit(res, nu.c_str(), nu.length());
        return *r == NULL ? ERR_INSUFFICIENT_MEMORY : ERR_NONE;
    }
}

int unit_to_angle(vartype *x, phloat *a) {
    phloat v, f;
    std::string bu;
    if (!get_value_and_base(x, &v, &bu, &f))
        return ERR_INVALID_UNIT;
    if (bu == "r")
        *a = rad_to_angle(v * f);
    else if (bu == "")
        *a = v * f;
    else
        return ERR_INCONSISTENT_UNITS;
    return ERR_NONE;
}

int docmd_n_plus_u(arg_struct *arg) {
    if (arg->type == ARGTYPE_NONE)
        /* This means the N+U is not followed by a NUMBER and an XSTR */
        return ERR_INVALID_DATA;
    if (p_isnan(arg->val_d)) {
        if (memcmp(&arg->val_d, &NAN_1_PHLOAT, sizeof(phloat)) == 0)
            return ERR_NUMBER_TOO_LARGE;
        else if (memcmp(&arg->val_d, &NAN_2_PHLOAT, sizeof(phloat)) == 0)
            return ERR_NUMBER_TOO_SMALL;
        else
            return ERR_INTERNAL_ERROR;
    }
    vartype *new_x = new_unit(arg->val_d, arg->val.xstr, arg->length);
    if (new_x == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    int err = recall_result_silently(new_x);
    if (err == ERR_NONE)
        print_stack_trace();
    return err;
}

int docmd_unit_t(arg_struct *arg) {
    return stack[sp]->type == TYPE_UNIT ? ERR_YES : ERR_NO;
}
