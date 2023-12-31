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

#include "core_math1.h"
#include "core_commands2.h"
#include "core_commands8.h"
#include "core_commandsa.h"
#include "core_display.h"
#include "core_globals.h"
#include "core_helpers.h"
#include "core_main.h"
#include "core_parser.h"
#include "core_variables.h"
#include "shell.h"

#define NUM_SHADOWS 10

struct caller_info {
    int keep_running;
    pgm_index prev_prgm;
    int4 prev_pc;
    void set(int prev) {
        if (prev != 0) {
            prev_prgm.set(0, prev);
            prev_pc = 0;
        } else {
            prev_prgm = current_prgm;
            prev_pc = pc;
        }
    }
    int ret(int err) {
        if (prev_prgm.idx == -5) {
            return return_to_plot(err != ERR_NONE, err == ERR_NONE && !keep_running);
        } else if (prev_prgm.idx == -3) {
            return return_to_integ(err == ERR_NONE && !keep_running);
        } else {
            current_prgm = prev_prgm;
            pc = prev_pc;
            return err != ERR_NONE ? err : keep_running ? ERR_NONE : ERR_STOP;
        }
    }
    void equation_deleted(int equation_index) {
        if (prev_prgm.dir == eq_dir->id && prev_prgm.idx == equation_index) {
            prev_prgm.dir = cwd->id;
            prev_prgm.idx = cwd->prgms_count - 1;
            prev_pc = cwd->prgms[cwd->prgms_count - 1].size - 2;
        }
    }
};

/* Solver */
struct solve_state {
    int version;
    vartype *eq;
    char prgm_name[7];
    int prgm_length;
    vartype *active_eq;
    char active_prgm_name[7];
    int active_prgm_length;
    vartype *saved_t;
    char var_name[7];
    int var_length;
    caller_info caller;
    int state;
    int which;
    int toggle;
    int retry_counter;
    int secant_impatience;
    phloat retry_value;
    phloat x1, x2, x3;
    phloat fx1, fx2;
    phloat prev_x, curr_x, curr_f;
    phloat xm, fxm;
    phloat best_f, best_x, second_f, second_x;
    char shadow_name[NUM_SHADOWS][7];
    int shadow_length[NUM_SHADOWS];
    vartype *shadow_value[NUM_SHADOWS];
    uint4 last_disp_time;
    int prev_sp;
    vartype *param_unit;
    phloat f_gap;
    int f_gap_worsening_counter;
    solve_state() : eq(NULL), active_eq(NULL), saved_t(NULL), param_unit(NULL) {
        prgm_length = 0;
        for (int i = 0; i < NUM_SHADOWS; i++) {
            shadow_length[i] = 0;
            shadow_value[i] = NULL;
        }
    }
};

static solve_state solve;

#define ROMB_K 5
// 1/2 million evals max!
#define ROMB_MAX 20

/* Integrator */
struct integ_state {
    int version;
    vartype *eq;
    char prgm_name[7];
    int prgm_length;
    vartype *active_eq;
    char active_prgm_name[7];
    int active_prgm_length;
    vartype *saved_t;
    char var_name[7];
    int var_length;
    caller_info caller;
    int state;
    phloat llim, ulim, acc;
    phloat a, b, eps;
    int n, m, i, k;
    phloat h, sum;
    phloat c[ROMB_K];
    phloat s[ROMB_K+1];
    int nsteps;
    phloat p;
    phloat t, u;
    phloat prev_int;
    phloat prev_res;
    int prev_sp;
    vartype *param_unit;
    vartype *result_unit;
    integ_state() : eq(NULL), active_eq(NULL), saved_t(NULL), param_unit(NULL), result_unit(NULL) {
        prgm_length = 0;
    }
};

static integ_state integ;


static void reset_solve();
static void reset_integ();


bool persist_math() {
    if (!write_int(solve.version)) return false;
    if (!persist_vartype(solve.eq)) return false;
    if (fwrite(solve.prgm_name, 1, 7, gfile) != 7) return false;
    if (!write_int(solve.prgm_length)) return false;
    if (!persist_vartype(solve.active_eq)) return false;
    if (fwrite(solve.active_prgm_name, 1, 7, gfile) != 7) return false;
    if (!write_int(solve.active_prgm_length)) return false;
    if (!persist_vartype(solve.saved_t)) return false;
    if (fwrite(solve.var_name, 1, 7, gfile) != 7) return false;
    if (!write_int(solve.var_length)) return false;
    if (!write_int(solve.caller.keep_running)) return false;
    if (solve_active()) {
        if (!write_int4(solve.caller.prev_prgm.dir)) return false;
        if (!write_int4(solve.caller.prev_prgm.idx)) return false;
        if (!write_int4(global_pc2line(solve.caller.prev_prgm, solve.caller.prev_pc))) return false;
    } else {
        if (!write_int4(0)) return false;
        if (!write_int4(0)) return false;
        if (!write_int4(0)) return false;
    }
    if (!write_int(solve.state)) return false;
    if (!write_int(solve.which)) return false;
    if (!write_int(solve.toggle)) return false;
    if (!write_int(solve.retry_counter)) return false;
    if (!write_int(solve.secant_impatience)) return false;
    if (!write_phloat(solve.retry_value)) return false;
    if (!write_phloat(solve.x1)) return false;
    if (!write_phloat(solve.x2)) return false;
    if (!write_phloat(solve.x3)) return false;
    if (!write_phloat(solve.fx1)) return false;
    if (!write_phloat(solve.fx2)) return false;
    if (!write_phloat(solve.prev_x)) return false;
    if (!write_phloat(solve.curr_x)) return false;
    if (!write_phloat(solve.curr_f)) return false;
    if (!write_phloat(solve.xm)) return false;
    if (!write_phloat(solve.fxm)) return false;
    if (!write_phloat(solve.best_f)) return false;
    if (!write_phloat(solve.best_x)) return false;
    if (!write_phloat(solve.second_f)) return false;
    if (!write_phloat(solve.second_x)) return false;
    for (int i = 0; i < NUM_SHADOWS; i++) {
        if (fwrite(solve.shadow_name[i], 1, 7, gfile) != 7) return false;
        if (!write_int(solve.shadow_length[i])) return false;
        if (!persist_vartype(solve.shadow_value[i])) return false;
    }
    if (!write_int4(solve.last_disp_time)) return false;
    if (!write_int(solve.prev_sp)) return false;
    if (!persist_vartype(solve.param_unit)) return false;

    if (!write_int(integ.version)) return false;
    if (!persist_vartype(integ.eq)) return false;
    if (fwrite(integ.prgm_name, 1, 7, gfile) != 7) return false;
    if (!write_int(integ.prgm_length)) return false;
    if (!persist_vartype(integ.active_eq)) return false;
    if (fwrite(integ.active_prgm_name, 1, 7, gfile) != 7) return false;
    if (!write_int(integ.active_prgm_length)) return false;
    if (!persist_vartype(integ.saved_t)) return false;
    if (fwrite(integ.var_name, 1, 7, gfile) != 7) return false;
    if (!write_int(integ.var_length)) return false;
    if (!write_int(integ.caller.keep_running)) return false;
    if (integ_active()) {
        if (!write_int4(integ.caller.prev_prgm.dir)) return false;
        if (!write_int4(integ.caller.prev_prgm.idx)) return false;
        if (!write_int4(global_pc2line(integ.caller.prev_prgm, integ.caller.prev_pc))) return false;
    } else {
        if (!write_int4(0)) return false;
        if (!write_int4(0)) return false;
        if (!write_int4(0)) return false;
    }
    if (!write_int(integ.state)) return false;
    if (!write_phloat(integ.llim)) return false;
    if (!write_phloat(integ.ulim)) return false;
    if (!write_phloat(integ.acc)) return false;
    if (!write_phloat(integ.a)) return false;
    if (!write_phloat(integ.b)) return false;
    if (!write_phloat(integ.eps)) return false;
    if (!write_int(integ.n)) return false;
    if (!write_int(integ.m)) return false;
    if (!write_int(integ.i)) return false;
    if (!write_int(integ.k)) return false;
    if (!write_phloat(integ.h)) return false;
    if (!write_phloat(integ.sum)) return false;
    for (int i = 0; i < ROMB_K; i++)
        if (!write_phloat(integ.c[i])) return false;
    for (int i = 0; i <= ROMB_K; i++)
        if (!write_phloat(integ.s[i])) return false;
    if (!write_int(integ.nsteps)) return false;
    if (!write_phloat(integ.p)) return false;
    if (!write_phloat(integ.t)) return false;
    if (!write_phloat(integ.u)) return false;
    if (!write_phloat(integ.prev_int)) return false;
    if (!write_phloat(integ.prev_res)) return false;
    if (!write_int(integ.prev_sp)) return false;
    if (!persist_vartype(integ.param_unit)) return false;
    if (!persist_vartype(integ.result_unit)) return false;
    return true;
}

bool unpersist_math(int ver) {
    if (!read_int(&solve.version)) return false;
    if (!unpersist_vartype(&solve.eq)) return false;
    if (fread(solve.prgm_name, 1, 7, gfile) != 7) return false;
    if (!read_int(&solve.prgm_length)) return false;
    if (!unpersist_vartype(&solve.active_eq)) return false;
    if (fread(solve.active_prgm_name, 1, 7, gfile) != 7) return false;
    if (!read_int(&solve.active_prgm_length)) return false;
    if (!unpersist_vartype(&solve.saved_t)) return false;
    if (fread(solve.var_name, 1, 7, gfile) != 7) return false;
    if (!read_int(&solve.var_length)) return false;
    if (!read_int(&solve.caller.keep_running)) return false;
    int4 dir, idx;
    if (ver < 9) {
        dir = root->id;
    } else {
        if (!read_int4(&dir)) return false;
    }
    if (!read_int4(&idx)) return false;
    solve.caller.prev_prgm.set(dir, idx);
    if (!read_int4(&solve.caller.prev_pc)) return false;
    if (solve_active())
        solve.caller.prev_pc = global_line2pc(solve.caller.prev_prgm, solve.caller.prev_pc);
    if (!read_int(&solve.state)) return false;
    if (!read_int(&solve.which)) return false;
    if (!read_int(&solve.toggle)) return false;
    if (!read_int(&solve.retry_counter)) return false;
    if (ver < 24) {
        solve.secant_impatience = 0;
    } else {
        if (!read_int(&solve.secant_impatience)) return false;
    }
    if (!read_phloat(&solve.retry_value)) return false;
    if (!read_phloat(&solve.x1)) return false;
    if (!read_phloat(&solve.x2)) return false;
    if (!read_phloat(&solve.x3)) return false;
    if (!read_phloat(&solve.fx1)) return false;
    if (!read_phloat(&solve.fx2)) return false;
    if (!read_phloat(&solve.prev_x)) return false;
    if (!read_phloat(&solve.curr_x)) return false;
    if (!read_phloat(&solve.curr_f)) return false;
    if (!read_phloat(&solve.xm)) return false;
    if (!read_phloat(&solve.fxm)) return false;
    if (!read_phloat(&solve.best_f)) return false;
    if (!read_phloat(&solve.best_x)) return false;
    if (!read_phloat(&solve.second_f)) return false;
    if (!read_phloat(&solve.second_x)) return false;
    for (int i = 0; i < NUM_SHADOWS; i++) {
        if (fread(solve.shadow_name[i], 1, 7, gfile) != 7) return false;
        if (!read_int(&solve.shadow_length[i])) return false;
        if (ver < 8) {
            phloat x;
            if (!read_phloat(&x)) return false;
            vartype *v = new_real(x);
            if (v == NULL)
                return false;
            solve.shadow_value[i] = v;
        } else {
            if (!unpersist_vartype(&solve.shadow_value[i])) return false;
        }
    }
    if (!read_int4((int4 *) &solve.last_disp_time)) return false;
    if (!read_int(&solve.prev_sp)) return false;
    if (ver < 8) {
        solve.param_unit = NULL;
    } else {
        if (!unpersist_vartype(&solve.param_unit)) return false;
    }
    solve.f_gap = NAN_PHLOAT;

    if (!read_int(&integ.version)) return false;
    if (!unpersist_vartype(&integ.eq)) return false;
    if (fread(integ.prgm_name, 1, 7, gfile) != 7) return false;
    if (!read_int(&integ.prgm_length)) return false;
    if (!unpersist_vartype(&integ.active_eq)) return false;
    if (fread(integ.active_prgm_name, 1, 7, gfile) != 7) return false;
    if (!read_int(&integ.active_prgm_length)) return false;
    if (!unpersist_vartype(&integ.saved_t)) return false;
    if (fread(integ.var_name, 1, 7, gfile) != 7) return false;
    if (!read_int(&integ.var_length)) return false;
    if (!read_int(&integ.caller.keep_running)) return false;
    if (ver < 9) {
        dir = root->id;
    } else {
        if (!read_int4(&dir)) return false;
    }
    if (!read_int4(&idx)) return false;
    integ.caller.prev_prgm.set(dir, idx);
    if (!read_int4(&integ.caller.prev_pc)) return false;
    if (integ_active())
        integ.caller.prev_pc = global_line2pc(integ.caller.prev_prgm, integ.caller.prev_pc);
    if (!read_int(&integ.state)) return false;
    if (!read_phloat(&integ.llim)) return false;
    if (!read_phloat(&integ.ulim)) return false;
    if (!read_phloat(&integ.acc)) return false;
    if (!read_phloat(&integ.a)) return false;
    if (!read_phloat(&integ.b)) return false;
    if (!read_phloat(&integ.eps)) return false;
    if (!read_int(&integ.n)) return false;
    if (!read_int(&integ.m)) return false;
    if (!read_int(&integ.i)) return false;
    if (!read_int(&integ.k)) return false;
    if (!read_phloat(&integ.h)) return false;
    if (!read_phloat(&integ.sum)) return false;
    for (int i = 0; i < ROMB_K; i++)
        if (!read_phloat(&integ.c[i])) return false;
    for (int i = 0; i <= ROMB_K; i++)
        if (!read_phloat(&integ.s[i])) return false;
    if (!read_int(&integ.nsteps)) return false;
    if (!read_phloat(&integ.p)) return false;
    if (!read_phloat(&integ.t)) return false;
    if (!read_phloat(&integ.u)) return false;
    if (!read_phloat(&integ.prev_int)) return false;
    if (!read_phloat(&integ.prev_res)) return false;
    if (!read_int(&integ.prev_sp)) return false;
    if (ver < 8) {
        integ.param_unit = NULL;
        integ.result_unit = NULL;
    } else {
        if (!unpersist_vartype(&integ.param_unit)) return false;
        if (!unpersist_vartype(&integ.result_unit)) return false;
    }
    return true;
}

void reset_math() {
    reset_solve();
    reset_integ();
}

void math_equation_deleted(int eqn_index) {
    solve.caller.equation_deleted(eqn_index);
    integ.caller.equation_deleted(eqn_index);
}

void clean_stack(int prev_sp) {
    if (flags.f.big_stack && prev_sp != -2 && sp > prev_sp) {
        int excess = sp - prev_sp;
        for (int i = 0; i < excess; i++)
            free_vartype(stack[sp - i]);
        sp -= excess;
    }
}

static void restore_t(vartype *t) {
    if (t == NULL || flags.f.big_stack)
        return;
    t = dup_vartype(t);
    if (t == NULL)
        return;
    free_vartype(stack[REG_X]);
    stack[REG_X] = stack[REG_Y];
    stack[REG_Y] = stack[REG_Z];
    stack[REG_Z] = stack[REG_T];
    stack[REG_T] = t;
}

static void reset_solve() {
    int i;
    for (i = 0; i < NUM_SHADOWS; i++)
        solve.shadow_length[i] = 0;
    free_vartype(solve.eq);
    solve.eq = NULL;
    solve.prgm_length = 0;
    free_vartype(solve.active_eq);
    solve.active_eq = NULL;
    solve.active_prgm_length = 0;
    free_vartype(solve.saved_t);
    solve.saved_t = NULL;
    solve.state = 0;
    free_vartype(solve.param_unit);
    solve.param_unit = NULL;
    if (mode_appmenu == MENU_SOLVE)
        set_menu_return_err(MENULEVEL_APP, MENU_NONE, true);
    solve.caller.prev_prgm.set(root->id, 0);
}

static int find_shadow(const char *name, int length) {
    int i;
    for (i = 0; i < NUM_SHADOWS; i++)
        if (string_equals(solve.shadow_name[i], solve.shadow_length[i],
                          name, length))
            return i;
    return -1;
}

void put_shadow(const char *name, int length, vartype *value) {
    remove_shadow(name, length);
    int i;
    for (i = 0; i < NUM_SHADOWS; i++)
        if (solve.shadow_length[i] == 0)
            goto do_insert;
    /* No empty slots available. Remove slot 0 (the oldest) and
     * move all subsequent ones down, freeing up slot NUM_SHADOWS - 1
     */
    free_vartype(solve.shadow_value[0]);
    for (i = 0; i < NUM_SHADOWS - 1; i++) {
        string_copy(solve.shadow_name[i], &solve.shadow_length[i],
                    solve.shadow_name[i + 1], solve.shadow_length[i + 1]);
        solve.shadow_value[i] = solve.shadow_value[i + 1];
    }
    do_insert:
    string_copy(solve.shadow_name[i], &solve.shadow_length[i], name, length);
    solve.shadow_value[i] = dup_vartype(value);
}

vartype *get_shadow(const char *name, int length) {
    int i = find_shadow(name, length);
    if (i == -1)
        return NULL;
    else
        return solve.shadow_value[i];
}

void remove_shadow(const char *name, int length) {
    int i = find_shadow(name, length);
    if (i == -1)
        return;
    free_vartype(solve.shadow_value[i]);
    while (i < NUM_SHADOWS - 1) {
        string_copy(solve.shadow_name[i], &solve.shadow_length[i],
                    solve.shadow_name[i + 1], solve.shadow_length[i + 1]);
        solve.shadow_value[i] = solve.shadow_value[i + 1];
        i++;
    }
    solve.shadow_value[NUM_SHADOWS - 1] = NULL;
    solve.shadow_length[NUM_SHADOWS - 1] = 0;
}

void set_solve_prgm(const char *name, int length) {
    string_copy(solve.prgm_name, &solve.prgm_length, name, length);
    free_vartype(solve.eq);
    solve.eq = NULL;
}

int set_solve_eqn(vartype *eq) {
    free_vartype(solve.eq);
    solve.eq = dup_vartype(eq);
    return solve.eq == NULL ? ERR_INSUFFICIENT_MEMORY : ERR_NONE;
}

static int call_solve_fn(int which, int state) {
    if (solve.active_eq == NULL && solve.active_prgm_length == 0)
        return ERR_NONEXISTENT;
    int err, i;
    arg_struct arg;
    vartype *v;
    phloat x = which == 1 ? solve.x1 : which == 2 ? solve.x2 : solve.x3;
    solve.prev_x = solve.curr_x;
    solve.curr_x = x;
    if (solve.var_length == 0) {
        if (solve.param_unit == 0) {
            v = new_real(x);
        } else {
            vartype_unit *u = (vartype_unit *) solve.param_unit;
            v = new_unit(x, u->text, u->length);
        }
        if (v == NULL)
            return ERR_INSUFFICIENT_MEMORY;
    } else if (solve.param_unit == NULL) {
        v = recall_var(solve.var_name, solve.var_length);
        if (v == NULL || v->type != TYPE_REAL) {
            v = new_real(x);
            if (v == NULL)
                return ERR_INSUFFICIENT_MEMORY;
            err = store_var(solve.var_name, solve.var_length, v);
            if (err != ERR_NONE) {
                free_vartype(v);
                return err;
            }
        } else
            ((vartype_real *) v)->x = x;
    } else {
        vartype_unit *u = (vartype_unit *) solve.param_unit;
        v = new_unit(x, u->text, u->length);
        if (v == NULL)
            return ERR_INSUFFICIENT_MEMORY;
        err = store_var(solve.var_name, solve.var_length, v);
        if (err != ERR_NONE) {
            free_vartype(v);
            return err;
        }
    }
    solve.which = which;
    solve.state = state;
    if (solve.active_eq == NULL) {
        arg.type = ARGTYPE_STR;
        arg.length = solve.active_prgm_length;
        for (i = 0; i < arg.length; i++)
            arg.val.text[i] = solve.active_prgm_name[i];
        clean_stack(solve.prev_sp);
        err = docmd_gto(&arg);
        if (err != ERR_NONE)
            return err;
    } else {
        clean_stack(solve.prev_sp);
        vartype_equation *eq = (vartype_equation *) solve.active_eq;
        current_prgm.set(eq_dir->id, eq->data->eqn_index);
        pc = 0;
    }
    if (solve.var_length == 0) {
        err = recall_result(v);
        if (err != ERR_NONE)
            return err;
    }
    pgm_index solve_index;
    solve_index.set(0, -2);
    err = push_rtn_addr(solve_index, 0);
    if (err == ERR_NONE) {
        if (solve.active_eq != NULL) {
            err = store_stack_reference(solve.active_eq);
            if (err != ERR_NONE)
                goto fail;
        }
        return ERR_RUN;
    } else {
        fail:
        return solve.caller.ret(err);
    }
}

static int start_solve_2(vartype *v1, vartype *v2, bool after_direct);

int start_solve(int prev, const char *name, int length, vartype *v1, vartype *v2, vartype **saved_inv) {
    if (solve_active())
        return ERR_SOLVE_SOLVE;
    string_copy(solve.var_name, &solve.var_length, name, length);
    string_copy(solve.active_prgm_name, &solve.active_prgm_length,
                solve.prgm_name, solve.prgm_length);
    free_vartype(solve.saved_t);
    if (!flags.f.big_stack && solve.eq != NULL)
        solve.saved_t = dup_vartype(stack[REG_T]);
    else
        solve.saved_t = NULL;
    solve.caller.set(prev);
    solve.prev_sp = flags.f.big_stack ? sp : -2;

    // Try direct solution
    if (solve.eq != NULL && flags.f.direct_solver) {
        vartype *inv;
        if (saved_inv != NULL && *saved_inv != NULL) {
            inv = *saved_inv;
        } else {
            inv = isolate(solve.eq, name, length);
            if (saved_inv != NULL)
                *saved_inv = inv;
        }
        if (inv != NULL) {
            solve.caller.keep_running = !should_i_stop_at_this_level() && program_running();

            solve.state = 8;
            clean_stack(solve.prev_sp);
            current_prgm.set(eq_dir->id, ((vartype_equation *) inv)->data->eqn_index);
            pc = 0;

            vartype *v = dup_vartype(v1);
            if (v != NULL)
                store_private_var("X1", 2, v);
            v = dup_vartype(v2);
            if (v != NULL)
                store_private_var("X2", 2, v);

            pgm_index solve_index;
            solve_index.set(0, -2);
            int err = push_rtn_addr(solve_index, 0);
            if (err == ERR_NONE) {
                if (saved_inv == NULL) {
                    err = store_private_var("REF", 3, inv);
                    if (err != ERR_NONE)
                        goto fail;
                }
                return ERR_RUN;
            } else {
                fail:
                if (saved_inv == NULL)
                    free_vartype(inv);
                return solve.caller.ret(err);
            }
        }
    }
    return start_solve_2(v1, v2, false);
}

static int start_solve_2(vartype *v1, vartype *v2, bool after_direct) {
    free_vartype(solve.param_unit);
    solve.param_unit = NULL;

    phloat x1, x2;
    if (v1 == NULL) {
        x1 = 0;
        x2 = 1;
    } else if (v1->type == TYPE_STRING) {
        return ERR_ALPHA_DATA_IS_INVALID;
    } else if (v2 == NULL) {
        one_guess:
        if (v1->type == TYPE_REAL) {
            x1 = ((vartype_real *) v1)->x;
            x2 = x1;
        } else if (v1->type == TYPE_UNIT) {
            solve.param_unit = dup_vartype(v1);
            if (solve.param_unit == NULL)
                return ERR_INSUFFICIENT_MEMORY;
            x1 = ((vartype_unit *) v1)->x;
            x2 = x1;
        } else
            return ERR_INVALID_TYPE;
    } else if (v2->type == TYPE_STRING) {
        return ERR_ALPHA_DATA_IS_INVALID;
    } else if ((v1->type == TYPE_REAL || v1->type == TYPE_UNIT)
            && (v2->type == TYPE_REAL || v2->type == TYPE_UNIT)) {
        x1 = ((vartype_real *) v1)->x;
        int err = convert_helper(v1, v2, &x2);
        if (err != ERR_NONE)
            goto one_guess;
        if (v1->type == TYPE_UNIT) {
            solve.param_unit = dup_vartype(v1);
            if (solve.param_unit == NULL)
                return ERR_INSUFFICIENT_MEMORY;
        }
    } else {
        return ERR_INVALID_TYPE;
    }

    if (solve.eq != NULL) {
        vartype *eq = dup_vartype(solve.eq);
        if (eq == NULL)
            return ERR_INSUFFICIENT_MEMORY;
        free_vartype(solve.active_eq);
        solve.active_eq = eq;
    } else {
        free_vartype(solve.active_eq);
        solve.active_eq = NULL;
    }

    if (x1 == x2) {
        if (x1 == 0) {
            x2 = 1;
            solve.retry_counter = 0;
        } else {
            x2 = x1 * 1.000001;
            if (p_isinf(x2))
                x2 = x1 * 0.999999;
            solve.retry_counter = -10;
        }
    } else {
        solve.retry_counter = 10;
        solve.retry_value = fabs(x1) < fabs(x2) ? x1 : x2;
    }
    if (x1 < x2) {
        solve.x1 = x1;
        solve.x2 = x2;
    } else {
        solve.x1 = x2;
        solve.x2 = x1;
    }
    solve.best_x = 0;
    solve.best_f = POS_HUGE_PHLOAT;
    solve.second_x = 0;
    solve.second_f = POS_HUGE_PHLOAT;
    solve.last_disp_time = 0;
    solve.toggle = 1;
    solve.secant_impatience = 0;
    solve.f_gap = NAN_PHLOAT;
    if (!after_direct)
        solve.caller.keep_running = !should_i_stop_at_this_level() && program_running();
    return call_solve_fn(1, 1);
}

const message_spec solve_message[] = {
    { NULL,             0 },
    { "Sign Reversal", 13 },
    { "Extremum",       8 },
    { "Bad Guess(es)", 13 },
    { "Constant?",      9 }
};

#define SOLVE_NOT_SURE -1

static int finish_solve(int message) {
    vartype *v, *new_x, *new_y, *new_z, *new_t;
    arg_struct arg;
    int dummy, print;

    phloat final_f = solve.curr_f;

    if (message == SOLVE_NOT_SURE)
        if (!p_isnan(solve.f_gap) && solve.f_gap_worsening_counter >= 3)
            message = SOLVE_SIGN_REVERSAL;
        else
            message = SOLVE_ROOT;

    if (solve.which == -1) {
        /* Ridders was terminated because it wasn't making progress; this does
         * not necessarily mean that x3 is the best guess so far. So, to be
         * sure, select the value with the lowest absolute function value.
         */
        phloat t1 = fabs(solve.fx1);
        phloat t2 = fabs(solve.fx2);
        phloat t3 = fabs(solve.curr_f);
        phloat t;
        if (t1 < t2) {
            solve.which = 1;
            t = t1;
            final_f = solve.fx1;
        } else {
            solve.which = 2;
            t = t2;
            final_f = solve.fx2;
        }
        if (t3 < t) {
            solve.which = 3;
            final_f = solve.curr_f;
        }
    }

    phloat b = solve.which == 1 ? solve.x1 :
                                solve.which == 2 ? solve.x2 : solve.x3;
    phloat s;
    if (p_isinf(solve.best_f))
        s = b;
    else if (solve.best_f > fabs(final_f))
        s = solve.best_x;
    else if (p_isinf(solve.second_f))
        s = solve.best_x;
    else
        s = solve.second_x;

    solve.state = 0;

    free_vartype(solve.active_eq);
    solve.active_eq = NULL;
    free_vartype(solve.saved_t);
    solve.saved_t = NULL;

    clean_stack(solve.prev_sp);
    if (solve.param_unit == NULL) {
        v = new_real(b);
        if (v == NULL)
            return ERR_INSUFFICIENT_MEMORY;
    } else {
        v = solve.param_unit;
        solve.param_unit = NULL;
        ((vartype_unit *) v)->x = b;
    }
    if (solve.var_length > 0) {
        int err = store_var(solve.var_name, solve.var_length, v);
        if (err != ERR_NONE) {
            free_vartype(v);
            return err;
        }
        v = dup_vartype(v);
    }

    if (flags.f.big_stack && !ensure_stack_capacity(4))
        return ERR_INSUFFICIENT_MEMORY;
    new_x = v;
    new_y = new_real(s);
    new_z = new_real(final_f);
    new_t = new_real(message);
    if (new_x == NULL || new_y == NULL || new_z == NULL || new_t == NULL) {
        free_vartype(new_x);
        free_vartype(new_y);
        free_vartype(new_z);
        free_vartype(new_t);
        return ERR_INSUFFICIENT_MEMORY;
    }
    if (flags.f.big_stack)
        sp += 4;
    else
        for (int i = 0; i < 4; i++)
            free_vartype(stack[i]);
    stack[sp] = new_x;
    stack[sp - 1] = new_y;
    stack[sp - 2] = new_z;
    stack[sp - 3] = new_t;

    arg.type = ARGTYPE_STR;
    string_copy(arg.val.text, &dummy, solve.var_name, solve.var_length);
    arg.length = solve.var_length;

    print = flags.f.trace_print && flags.f.printer_exists;

    if (!solve.caller.keep_running) {
        view_helper(&arg, print);
        if (message != SOLVE_ROOT)
            draw_message(1, solve_message[message].text,
                            solve_message[message].length);
    } else {
        if (print) {
            char namebuf[8];
            char *valbuf = (char *) malloc(disp_c);
            freer f(valbuf);
            int namelen = 0, vallen;
            string2buf(namebuf, 8, &namelen, solve.var_name, solve.var_length);
            char2buf(namebuf, 8, &namelen, '=');
            vallen = vartype2string(v, valbuf, disp_c);
            print_wide(namebuf, namelen, valbuf, vallen);
        }
    }

    if (print && message != SOLVE_ROOT)
        print_lines(solve_message[message].text,
                    solve_message[message].length, true);

    return solve.caller.ret(ERR_NONE);
}

static void track_f_gap() {
    phloat gap = solve.fx2 - solve.fx1;
    if (gap == 0 || p_isnan(gap)) {
        solve.f_gap = NAN_PHLOAT;
        return;
    }
    if (p_isnan(solve.f_gap)
            || (gap > 0) != (solve.f_gap > 0)
            || fabs(gap) < fabs(solve.f_gap))
        solve.f_gap_worsening_counter = 0;
    else
        solve.f_gap_worsening_counter++;
    solve.f_gap = gap;
}

int return_to_solve(bool failure, bool stop) {
    phloat f, slope, s, xnew, prev_f = solve.curr_f;
    uint4 now_time;

    if (stop)
        solve.caller.keep_running = 0;

    if (solve.state == 8) {
        // Direct solution
        if (failure) {
            // Proceed to numerical solver
            // TODO: What about stack state? Is it OK like this?
            vartype *v1 = recall_and_purge_private_var("X1", 2);
            vartype *v2 = recall_and_purge_private_var("X2", 2);
            int err = start_solve_2(v1, v2, true);
            free_vartype(v1);
            free_vartype(v2);
            return err;
        }
        if (sp == -1)
            return ERR_TOO_FEW_ARGUMENTS;
        if (flags.f.big_stack && !ensure_stack_capacity(1))
            return ERR_INSUFFICIENT_MEMORY;

        /* If the variable being solved for already exists, try to convert
         * the direct solver's result to the same unit as the existing value.
         */
        if (stack[sp]->type == TYPE_UNIT) {
            bool writable;
            vartype *prev = recall_var(solve.var_name, solve.var_length, &writable);
            if (prev != NULL && prev->type == TYPE_UNIT && writable) {
                phloat n;
                int err = convert_helper(prev, stack[sp], &n);
                if (err == ERR_NONE) {
                    vartype *nv = dup_vartype(prev);
                    if (nv != NULL) {
                        ((vartype_unit *) nv)->x = n;
                        free_vartype(stack[sp]);
                        stack[sp] = nv;
                    }
                }
            }
        }

        vartype *v = dup_vartype(stack[sp]);
        vartype *m = new_string("Direct", 6);
        if (v == NULL || m == NULL) {
            nomem:
            free_vartype(v);
            free_vartype(m);
            return ERR_INSUFFICIENT_MEMORY;
        }
        int err = store_var(solve.var_name, solve.var_length, v);
        if (err != ERR_NONE)
            goto nomem;
        /* Put the string "Direct" in Y, to signal that the solution was
         * obtained using the direct solver.
         */
        if (flags.f.big_stack) {
            sp++;
            stack[sp] = stack[sp - 1];
            stack[sp - 1] = m;
        } else {
            free_vartype(stack[REG_T]);
            stack[REG_T] = stack[REG_Z];
            stack[REG_Z] = stack[REG_Y];
            stack[REG_Y] = m;
        }

        if (!solve.caller.keep_running) {
            arg_struct arg;
            arg.type = ARGTYPE_STR;
            int len;
            string_copy(arg.val.text, &len, solve.var_name, solve.var_length);
            arg.length = len;

            int print = flags.f.trace_print && flags.f.printer_exists;
            view_helper(&arg, print);
        }

        return solve.caller.ret(ERR_NONE);
    }

    if (solve.state == 0)
        return ERR_INTERNAL_ERROR;
    if (!failure) {
        if (sp == -1)
            return ERR_TOO_FEW_ARGUMENTS;
        if (stack[sp]->type == TYPE_REAL) {
            real_result:
            f = ((vartype_real *) stack[sp])->x;
            solve.curr_f = f;
            if (f == 0)
                return finish_solve(SOLVE_ROOT);
            if (fabs(f) < fabs(solve.best_f)) {
                solve.second_f = solve.best_f;
                solve.second_x = solve.best_x;
                solve.best_f = fabs(f);
                solve.best_x = solve.curr_x;
            }
        } else if (stack[sp]->type == TYPE_UNIT) {
            bool saved_norm = flags.f.normal_print;
            bool saved_trace = flags.f.trace_print;
            flags.f.normal_print = false;
            flags.f.trace_print = false;
            int err = docmd_ubase(NULL);
            flags.f.normal_print = saved_norm;
            flags.f.trace_print = saved_trace;
            if (err == ERR_NONE)
                goto real_result;
            else
                goto bad_result;
        } else {
            bad_result:
            solve.curr_f = POS_HUGE_PHLOAT;
            failure = true;
        }
        restore_t(solve.saved_t);
    } else
        solve.curr_f = POS_HUGE_PHLOAT;

    if (!failure && solve.retry_counter != 0) {
        if (solve.retry_counter > 0)
            solve.retry_counter--;
        else
            solve.retry_counter++;
    }

    now_time = shell_milliseconds();
    if (now_time < solve.last_disp_time)
        // shell_milliseconds() wrapped around
        solve.last_disp_time = 0;
    if (!solve.caller.keep_running && solve.state > 1
                                && now_time >= solve.last_disp_time + 250) {
        /* Put on a show so the user won't think we're just drinking beer
         * while they're waiting anxiously for the solver to converge...
         */
        char *buf = (char *) malloc(disp_c);
        freer f(buf);
        int bufptr = 0, i;
        solve.last_disp_time = now_time;
        bufptr = phloat2string(solve.curr_x, buf, disp_c, 0, 0, 3,
                                    flags.f.thousands_separators);
        for (i = bufptr; i < disp_c - 1; i++)
            buf[i] = ' ';
        buf[disp_c - 1] = failure ? '?' : solve.curr_f > 0 ? '+' : '-';
        draw_message(0, buf, disp_c, false);
        bufptr = phloat2string(solve.prev_x, buf, disp_c, 0, 0, 3,
                                    flags.f.thousands_separators);
        for (i = bufptr; i < disp_c - 1; i++)
            buf[i] = ' ';
        buf[disp_c - 1] = prev_f == POS_HUGE_PHLOAT ? '?' : prev_f > 0 ? '+' : '-';
        draw_message(1, buf, disp_c);
    }

    switch (solve.state) {

        case 1:
            /* first evaluation of x1 */
            if (failure) {
                if (solve.retry_counter > 0)
                    solve.retry_counter = -solve.retry_counter;
                return call_solve_fn(2, 2);
            } else {
                solve.fx1 = f;
                return call_solve_fn(2, 3);
            }

        case 2:
            /* first evaluation of x2 after x1 was unsuccessful */
            if (failure)
                return finish_solve(SOLVE_BAD_GUESSES);
            solve.fx2 = f;
            solve.x1 = (solve.x1 + solve.x2) / 2;
            if (solve.x1 == solve.x2)
                return finish_solve(SOLVE_BAD_GUESSES);
            return call_solve_fn(1, 3);

        case 3:
            /* make sure f(x1) != f(x2) */
            if (failure) {
                if (solve.which == 1)
                    solve.x1 = (solve.x1 + solve.x2) / 2;
                else
                    solve.x2 = (solve.x1 + solve.x2) / 2;
                if (solve.x1 == solve.x2)
                    return finish_solve(SOLVE_BAD_GUESSES);
                return call_solve_fn(solve.which, 3);
            }
            if (solve.which == 1)
                solve.fx1 = f;
            else
                solve.fx2 = f;
            if (solve.fx1 == solve.fx2) {
                /* If f(x1) == f(x2), we assume we're in a local flat spot.
                 * We extend the interval exponentially until we have two
                 * values of x, both of which are evaluated successfully,
                 * and yielding different values; from that moment on, we can
                 * apply the secant method.
                 */
                int which;
                phloat x;
                if (solve.toggle) {
                    x = solve.x2 + 100 * (solve.x2 - solve.x1);
                    if (p_isinf(x)) {
                        if (solve.retry_counter != 0)
                            goto retry_solve;
                        return finish_solve(SOLVE_CONSTANT);
                    }
                    which = 2;
                    solve.x2 = x;
                } else {
                    x = solve.x1 - 100 * (solve.x2 - solve.x1);
                    if (p_isinf(x)) {
                        if (solve.retry_counter != 0)
                            goto retry_solve;
                        return finish_solve(SOLVE_CONSTANT);
                    }
                    which = 1;
                    solve.x1 = x;
                }
                solve.toggle = !solve.toggle;
                return call_solve_fn(which, 3);
            }
            /* When we get here, f(x1) != f(x2), and we can start
             * applying the secant method.
             */
            goto do_secant;

        case 4:
            /* secant method, evaluated x3 */
        case 5:
            /* just performed bisection */
            if (failure) {
                if (solve.x3 > solve.x2) {
                    /* Failure outside [x1, x2]; approach x2 */
                    solve.x3 = (solve.x2 + solve.x3) / 2;
                    if (solve.x3 == solve.x2)
                        return finish_solve(SOLVE_EXTREMUM);
                } else if (solve.x3 < solve.x1) {
                    /* Failure outside [x1, x2]; approach x1 */
                    solve.x3 = (solve.x1 + solve.x3) / 2;
                    if (solve.x3 == solve.x1)
                        return finish_solve(SOLVE_EXTREMUM);
                } else {
                    /* Failure inside [x1, x2];
                     * alternately approach x1 and x2 */
                    if (solve.toggle) {
                        phloat old_x3 = solve.x3;
                        if (solve.x3 <= (solve.x1 + solve.x2) / 2)
                            solve.x3 = (solve.x1 + solve.x3) / 2;
                        else
                            solve.x3 = (solve.x2 + solve.x3) / 2;
                        if (solve.x3 == old_x3)
                            return finish_solve(SOLVE_SIGN_REVERSAL);
                    } else
                        solve.x3 = solve.x1 + solve.x2 - solve.x3;
                    solve.toggle = !solve.toggle;
                    if (solve.x3 == solve.x1 || solve.x3 == solve.x2)
                        return finish_solve(SOLVE_SIGN_REVERSAL);
                }
                return call_solve_fn(3, 4);
            } else if (solve.fx1 > 0 && solve.fx2 > 0) {
                if (f > 0) {
                    if (f > solve.best_f) {
                        if (++solve.secant_impatience > 30) {
                            solve.which = -1;
                            return finish_solve(SOLVE_EXTREMUM);
                        }
                    } else {
                        solve.secant_impatience = 0;
                    }
                }
                if (solve.fx1 > solve.fx2) {
                    if (f >= solve.fx1 && solve.state != 5)
                        goto do_bisection;
                    solve.x1 = solve.x3;
                    solve.fx1 = f;
                } else {
                    if (f >= solve.fx2 && solve.state != 5)
                        goto do_bisection;
                    solve.x2 = solve.x3;
                    solve.fx2 = f;
                }
            } else if (solve.fx1 < 0 && solve.fx2 < 0) {
                if (f < 0) {
                    if (-f > solve.best_f) {
                        if (++solve.secant_impatience > 30) {
                            solve.which = -1;
                            return finish_solve(SOLVE_EXTREMUM);
                        }
                    } else {
                        solve.secant_impatience = 0;
                    }
                }
                if (solve.fx1 < solve.fx2) {
                    if (f <= solve.fx1 && solve.state != 5)
                        goto do_bisection;
                    solve.x1 = solve.x3;
                    solve.fx1 = f;
                } else {
                    if (f <= solve.fx2 && solve.state != 5)
                        goto do_bisection;
                    solve.x2 = solve.x3;
                    solve.fx2 = f;
                }
            } else {
                /* f(x1) and f(x2) have opposite signs; assuming f is
                 * continuous on the interval [x1, x2], there is at least
                 * one root. We use x3 to replace x1 or x2 and narrow the
                 * interval, even if f(x3) is actually worse than f(x1) and
                 * f(x2). This way we're guaranteed to home in on the root
                 * (but of course we'll get stuck if we encounter a
                 * discontinuous sign reversal instead, e.g. 1/x at x = 0).
                 * Such is life.
                 */
                if ((solve.fx1 > 0 && f > 0) || (solve.fx1 < 0 && f < 0)) {
                    solve.x1 = solve.x3;
                    solve.fx1 = f;
                } else {
                    solve.x2 = solve.x3;
                    solve.fx2 = f;
                }
            }
            if (solve.x2 < solve.x1) {
                /* Make sure x1 is always less than x2 */
                phloat tmp = solve.x1;
                solve.x1 = solve.x2;
                solve.x2 = tmp;
                tmp = solve.fx1;
                solve.fx1 = solve.fx2;
                solve.fx2 = tmp;
            }
            track_f_gap();
            do_secant:
            if (solve.fx1 == solve.fx2)
                return finish_solve(SOLVE_EXTREMUM);
            if ((solve.fx1 > 0 && solve.fx2 < 0)
                    || (solve.fx1 < 0 && solve.fx2 > 0))
                goto do_ridders;
            slope = (solve.fx2 - solve.fx1) / (solve.x2 - solve.x1);
            if (p_isinf(slope)) {
                solve.x3 = (solve.x1 + solve.x2) / 2;
                if (solve.x3 == solve.x1 || solve.x3 == solve.x2)
                    return finish_solve(SOLVE_NOT_SURE);
                else
                    return call_solve_fn(3, 4);
            } else if (slope == 0) {
                /* Underflow caused by x2 - x1 being too big.
                 * We're changing the calculation sequence to steer
                 * clear of trouble.
                 */
                solve.x3 = solve.x1 - solve.fx1 * (solve.x2 - solve.x1)
                                                / (solve.fx2 - solve.fx1);

                goto finish_secant;
            } else {
                int inf;
                solve.x3 = solve.x1 - solve.fx1 / slope;
                finish_secant:
                if ((inf = p_isinf(solve.x3)) != 0) {
                    if (solve.retry_counter != 0)
                        goto retry_solve;
                    return finish_solve(SOLVE_EXTREMUM);
                }
                /* The next two 'if' statements deal with the case that the
                 * secant extrapolation returns one of the points we already
                 * had. We assume this means no improvement is possible.
                 * We fudge the 'solve' struct a bit to make sure we don't
                 * return the 'bad' value as the root.
                 */
                if (solve.x3 == solve.x1) {
                    if (fabs(slope) > 1e50) {
                        // Not improving because slope too steep
                        solve.x3 = solve.x1 - (solve.x2 - solve.x1) / 100;
                        return call_solve_fn(3, 4);
                    }
                    solve.which = 1;
                    solve.curr_f = solve.fx1;
                    solve.prev_x = solve.x2;
                    return finish_solve(SOLVE_NOT_SURE);
                }
                if (solve.x3 == solve.x2) {
                    if (fabs(slope) > 1e50) {
                        // Not improving because slope too steep
                        solve.x3 = solve.x2 + (solve.x2 - solve.x1) / 100;
                        return call_solve_fn(3, 4);
                    }
                    solve.which = 2;
                    solve.curr_f = solve.fx2;
                    solve.prev_x = solve.x1;
                    return finish_solve(SOLVE_NOT_SURE);
                }
                /* If we're extrapolating, make sure we don't race away from
                 * the current interval too quickly */
                if (solve.x3 < solve.x1) {
                    phloat min = solve.x1 - 100 * (solve.x2 - solve.x1);
                    if (solve.x3 < min)
                        solve.x3 = min;
                } else if (solve.x3 > solve.x2) {
                    phloat max = solve.x2 + 100 * (solve.x2 - solve.x1);
                    if (solve.x3 > max)
                        solve.x3 = max;
                } else {
                    /* If we're interpolating, make sure we actually make
                     * some progress. Enforce a minimum distance between x3
                     * and the edges of the interval.
                     */
                    phloat eps = (solve.x2 - solve.x1) / 10;
                    if (solve.x3 < solve.x1 + eps)
                        solve.x3 = solve.x1 + eps;
                    else if (solve.x3 > solve.x2 - eps)
                        solve.x3 = solve.x2 - eps;
                }
                return call_solve_fn(3, 4);
            }

            retry_solve:
            /* We hit infinity without finding two values of x where f(x) has
             * opposite signs, but we got to infinity suspiciously quickly.
             * If we started with two guesses, we now retry with only the lower
             * of the two; if we started with one guess, we now retry with
             * starting guesses of 0 and 1.
             */
            if (solve.retry_counter > 0) {
                solve.x1 = solve.retry_value;
                solve.x2 = solve.x1 * 1.000001;
                if (p_isinf(solve.x2))
                    solve.x2 = solve.x1 * 0.999999;
                if (solve.x1 > solve.x2) {
                    phloat tmp = solve.x1;
                    solve.x1 = solve.x2;
                    solve.x2 = tmp;
                }
                solve.retry_counter = -10;
            } else {
                solve.x1 = 0;
                solve.x2 = 1;
                solve.retry_counter = 0;
            }
            return call_solve_fn(1, 1);

            do_bisection:
            solve.x3 = (solve.x1 + solve.x2) / 2;
            return call_solve_fn(3, 5);

        case 6:
            /* Ridders' method, evaluated midpoint */
            if (failure)
                goto do_bisection;
            s = sqrt(f * f - solve.fx1 * solve.fx2);
            if (s == 0) {
                /* Mathematically impossible, but numerically possible if
                 * the function is so close to zero that f^2 underflows.
                 * We could handle this better but this seems adequate.
                 */
                solve.which = -1;
                return finish_solve(SOLVE_NOT_SURE);
            }
            solve.xm = solve.x3;
            solve.fxm = f;
            if (solve.fx1 < solve.fx2)
                s = -s;
            xnew = solve.xm + (solve.xm - solve.x1) * (solve.fxm / s);
            if (xnew == solve.x1 || xnew == solve.x2) {
                solve.which = -1;
                return finish_solve(SOLVE_NOT_SURE);
            }
            solve.x3 = xnew;
            return call_solve_fn(3, 7);

        case 7:
            /* Ridders' method, evaluated xnew */
            if (failure)
                goto do_bisection;
            if ((f > 0 && solve.fxm < 0) || (f < 0 && solve.fxm > 0)) {
                if (solve.xm < solve.x3) {
                    solve.x1 = solve.xm;
                    solve.fx1 = solve.fxm;
                    solve.x2 = solve.x3;
                    solve.fx2 = f;
                } else {
                    solve.x1 = solve.x3;
                    solve.fx1 = f;
                    solve.x2 = solve.xm;
                    solve.fx2 = solve.fxm;
                }
            } else if ((f > 0 && solve.fx1 < 0) || (f < 0 && solve.fx1 > 0)) {
                solve.x2 = solve.x3;
                solve.fx2 = f;
            } else /* f > 0 && solve.fx2 < 0 || f < 0 && solve.fx2 > 0 */ {
                solve.x1 = solve.x3;
                solve.fx1 = f;
            }
            track_f_gap();
            do_ridders:
            solve.x3 = (solve.x1 + solve.x2) / 2;
            // TODO: The following termination condition should really be
            //
            //  if (solve.x3 == solve.x1 || solve.x3 == solve.x2)
            //
            // since it is mathematically impossible for x3 to lie outside the
            // interval [x1, x2], but due to decimal round-off, it is, in fact,
            // possible, e.g. consider x1 = 7...2016 and x2 = 7...2018; this
            // results in x3 = 7...2015. Fixing this will require either
            // extended-precision math for solver internals, or at least a
            // slightly smarter implementation of the midpoint calculation,
            // e.g. fall back on x3 = x1 + (x2 - x1) / 2 if x3 = (x1 + x2) / 2
            // returns an incorrect result.
            if (solve.x3 <= solve.x1 || solve.x3 >= solve.x2) {
                solve.which = -1;
                return finish_solve(SOLVE_NOT_SURE);
            } else
                return call_solve_fn(3, 6);

        default:
            return ERR_INTERNAL_ERROR;
    }
}

bool is_solve_var(const char *name, int length) {
    return string_equals(solve.var_name, solve.var_length, name, length);
}

static void reset_integ() {
    free_vartype(integ.eq);
    integ.eq = NULL;
    integ.prgm_length = 0;
    free_vartype(integ.active_eq);
    integ.active_eq = NULL;
    integ.active_prgm_length = 0;
    free_vartype(integ.saved_t);
    integ.saved_t = NULL;
    integ.state = 0;
    free_vartype(integ.param_unit);
    integ.param_unit = NULL;
    free_vartype(integ.result_unit);
    integ.result_unit = NULL;
    if (mode_appmenu == MENU_INTEG || mode_appmenu == MENU_INTEG_PARAMS)
        set_menu_return_err(MENULEVEL_APP, MENU_NONE, true);
    integ.caller.prev_prgm.set(root->id, 0);
}

void set_integ_prgm(const char *name, int length) {
    string_copy(integ.prgm_name, &integ.prgm_length, name, length);
    free_vartype(integ.eq);
    integ.eq = NULL;
}

int set_integ_eqn(vartype *eq) {
    free_vartype(integ.eq);
    integ.eq = dup_vartype(eq);
    return integ.eq == NULL ? ERR_INSUFFICIENT_MEMORY : ERR_NONE;
}

void get_integ_prgm_eqn(char *name, int *length, vartype **eqn) {
    string_copy(name, length, integ.prgm_name, integ.prgm_length);
    *eqn = dup_vartype(integ.eq);
}

void set_integ_var(const char *name, int length) {
    string_copy(integ.var_name, &integ.var_length, name, length);
}

void get_integ_var(char *name, int *length) {
    string_copy(name, length, integ.var_name, integ.var_length);
}

static int call_integ_fn() {
    if (integ.active_eq == NULL && integ.active_prgm_length == 0)
        return ERR_NONEXISTENT;
    int err, i;
    arg_struct arg;
    phloat x = integ.u;
    vartype *v;

    if (integ.var_length == 0) {
        if (integ.param_unit == 0) {
            v = new_real(x);
        } else {
            vartype_unit *u = (vartype_unit *) integ.param_unit;
            v = new_unit(x, u->text, u->length);
        }
        if (v == NULL)
            return ERR_INSUFFICIENT_MEMORY;
    } else if (integ.param_unit == NULL) {
        v = recall_var(integ.var_name, integ.var_length);
        if (v == NULL || v->type != TYPE_REAL) {
            v = new_real(x);
            if (v == NULL)
                return ERR_INSUFFICIENT_MEMORY;
            err = store_var(integ.var_name, integ.var_length, v);
            if (err != ERR_NONE) {
                free_vartype(v);
                return err;
            }
        } else
            ((vartype_real *) v)->x = x;
    } else {
        vartype_unit *u = (vartype_unit *) integ.param_unit;
        v = new_unit(x, u->text, u->length);
        if (v == NULL)
            return ERR_INSUFFICIENT_MEMORY;
        err = store_var(integ.var_name, integ.var_length, v);
        if (err != ERR_NONE) {
            free_vartype(v);
            return err;
        }
    }

    pgm_index integ_index;
    integ_index.set(0, -3);

    if (integ.active_eq == NULL) {
        arg.type = ARGTYPE_STR;
        arg.length = integ.active_prgm_length;
        for (i = 0; i < arg.length; i++)
            arg.val.text[i] = integ.active_prgm_name[i];
        clean_stack(integ.prev_sp);
        err = docmd_gto(&arg);
        if (err != ERR_NONE)
            return err;
    } else if (integ.active_eq->type == TYPE_LIST) {
        /* Combined INTEG(SOLVE), used by the plot viewer */
        // TODO: Stack reference needed for eq in this case?
        // err = store_stack_reference(integ.active_eq);
        // if (err != ERR_NONE)
        //     goto fail;
        vartype_list *list = (vartype_list *) integ.active_eq;
        vartype_string *fname = (vartype_string *) list->array->data[0];
        err = start_solve(-3, fname->txt(), fname->length, list->array->data[1], list->array->data[2], &list->array->data[3]);
        if (err != ERR_NONE && err != ERR_RUN)
            goto fail;
        return err;
    } else {
        clean_stack(integ.prev_sp);
        vartype_equation *eq = (vartype_equation *) integ.active_eq;
        current_prgm.set(eq_dir->id, eq->data->eqn_index);
        pc = 0;
    }
    if (integ.var_length == 0) {
        err = recall_result(v);
        if (err != ERR_NONE)
            return err;
    }
    err = push_rtn_addr(integ_index, 0);
    if (err == ERR_NONE) {
        if (integ.active_eq != NULL) {
            err = store_stack_reference(integ.active_eq);
            if (err != ERR_NONE)
                goto fail;
        }
        return ERR_RUN;
    } else {
        fail:
        return integ.caller.ret(err);
    }
}

int start_integ(int prev, const char *name, int length, vartype *solve_info) {
    if (integ_active())
        return ERR_INTEG_INTEG;

    vartype *llim = recall_var("LLIM", 4);
    if (llim == NULL)
        return ERR_NONEXISTENT;
    else if (llim->type == TYPE_STRING)
        return ERR_ALPHA_DATA_IS_INVALID;
    else if (llim->type != TYPE_REAL && llim->type != TYPE_UNIT)
        return ERR_INVALID_TYPE;
    vartype *ulim = recall_var("ULIM", 4);
    if (ulim == NULL)
        return ERR_NONEXISTENT;
    else if (ulim->type == TYPE_STRING)
        return ERR_ALPHA_DATA_IS_INVALID;
    else if (ulim->type != TYPE_REAL && ulim->type != TYPE_UNIT)
        return ERR_INVALID_TYPE;

    integ.llim = ((vartype_real *) llim)->x;
    int err = convert_helper(llim, ulim, &integ.ulim);
    if (err != ERR_NONE)
        return err;
    free_vartype(integ.param_unit);
    if (llim->type == TYPE_REAL) {
        integ.param_unit = NULL;
    } else {
        integ.param_unit = dup_vartype(llim);
        if (integ.param_unit == NULL)
            return ERR_INSUFFICIENT_MEMORY;
    }

    free_vartype(integ.result_unit);
    integ.result_unit = NULL;

    vartype *acc = recall_var("ACC", 3);
    if (acc == NULL)
        integ.acc = 0;
    else if (acc->type == TYPE_STRING)
        return ERR_ALPHA_DATA_IS_INVALID;
    else if (acc->type != TYPE_REAL)
        return ERR_INVALID_TYPE;
    else
        integ.acc = ((vartype_real *) acc)->x;
    if (integ.acc < 0)
        integ.acc = 0;
    string_copy(integ.var_name, &integ.var_length, name, length);
    string_copy(integ.active_prgm_name, &integ.active_prgm_length,
                integ.prgm_name, integ.prgm_length);
    free_vartype(integ.saved_t);
    if (!flags.f.big_stack && integ.eq != NULL)
        integ.saved_t = dup_vartype(stack[REG_T]);
    else
        integ.saved_t = NULL;
    if (solve_info != NULL) {
        free_vartype(integ.active_eq);
        integ.active_eq = solve_info;
    } else if (integ.eq != NULL) {
        vartype *eq = dup_vartype(integ.eq);
        if (eq == NULL)
            return ERR_INSUFFICIENT_MEMORY;
        free_vartype(integ.active_eq);
        integ.active_eq = eq;
    } else {
        free_vartype(integ.active_eq);
        integ.active_eq = NULL;
    }
    integ.caller.set(prev);
    integ.prev_sp = flags.f.big_stack ? sp : -2;

    integ.a = integ.llim;
    integ.b = integ.ulim - integ.llim;
    integ.h = 2;
    integ.prev_int = 0;
    integ.nsteps = 1;
    integ.n = 1;
    integ.state = 1;
    integ.s[0] = 0;
    integ.k = 1;
    integ.prev_res = 0;

    integ.caller.keep_running = !should_i_stop_at_this_level() && program_running();
    if (!integ.caller.keep_running)
        draw_message(0, "Integrating", 11);
    return return_to_integ(false);
}

static int finish_integ() {
    vartype *x, *y;
    int saved_trace = flags.f.trace_print;
    integ.state = 0;

    clean_stack(integ.prev_sp);
    if (integ.param_unit == NULL && (integ.result_unit == NULL || integ.result_unit->type == TYPE_REAL)) {
        real_result:
        x = new_real(integ.sum * integ.b * 0.75);
        y = new_real(integ.eps);
    } else {
        std::string pu("1"), ru("1");
        if (integ.param_unit != NULL) {
            vartype_unit *u = (vartype_unit *) integ.param_unit;
            pu = std::string(u->text, u->length);
        }
        if (integ.result_unit != NULL && integ.result_unit->type == TYPE_UNIT) {
            vartype_unit *u = (vartype_unit *) integ.result_unit;
            ru = std::string(u->text, u->length);
        }
        normalize_unit(pu + "*" + ru, &ru);
        if (ru == "")
            goto real_result;
        x = new_unit(integ.sum * integ.b * 0.75, ru.c_str(), ru.length());
        y = new_unit(integ.eps, ru.c_str(), ru.length());
    }
    if (x == NULL || y == NULL) {
        free_vartype(x);
        free_vartype(y);
        return ERR_INSUFFICIENT_MEMORY;
    }
    flags.f.trace_print = 0;
    if (recall_two_results(x, y) != ERR_NONE)
        return ERR_INSUFFICIENT_MEMORY;
    flags.f.trace_print = saved_trace;

    free_vartype(integ.active_eq);
    integ.active_eq = NULL;
    free_vartype(integ.saved_t);
    integ.saved_t = NULL;
    free_vartype(integ.param_unit);
    integ.param_unit = NULL;
    free_vartype(integ.result_unit);
    integ.result_unit = NULL;

    if (!integ.caller.keep_running) {
        char *buf = (char *) malloc(disp_c);
        freer f(buf);
        int bufptr = 0;
        string2buf(buf, disp_c, &bufptr, "\003=", 2);
        bufptr += vartype2string(x, buf + bufptr, disp_c - bufptr);
        draw_message(0, buf, bufptr);
        if (flags.f.trace_print && flags.f.printer_exists)
            print_wide(buf, 2, buf + 2, bufptr - 2);
    }

    return integ.caller.ret(ERR_NONE);
}


/* approximate integral of `f' between `a' and `b' subject to a given
 * error. Use Romberg method with refinement substitution, x = (3u-u^3)/2
 * which prevents endpoint evaluation and causes non-uniform sampling.
 */

int return_to_integ(bool stop) {
    if (stop)
        integ.caller.keep_running = 0;

    vartype *r;
    phloat pr;

    switch (integ.state) {
    case 0:
        return ERR_INTERNAL_ERROR;

    case 1:
        integ.state = 2;

    loop1:

        integ.p = integ.h / 2 - 1;
        integ.sum = 0.0;
        integ.i = 0;

    loop2:

        integ.t = 1 - integ.p * integ.p;
        integ.u = integ.p + integ.t * integ.p / 2;
        integ.u = (integ.u * integ.b + integ.b) / 2 + integ.a;
        return call_integ_fn();

    case 2:
        if (sp == -1)
            return ERR_TOO_FEW_ARGUMENTS;
        if (stack[sp]->type == TYPE_STRING)
            return ERR_ALPHA_DATA_IS_INVALID;
        r = stack[sp];
        if (r->type != TYPE_REAL && r->type != TYPE_UNIT)
            return ERR_INVALID_TYPE;
        if (integ.result_unit == NULL) {
            integ.result_unit = dup_vartype(r);
            if (integ.result_unit == NULL)
                return ERR_INSUFFICIENT_MEMORY;
            pr = ((vartype_real *) r)->x;
        } else {
            int err = convert_helper(integ.result_unit, r, &pr);
            if (err != ERR_NONE)
                return err;
        }
        integ.sum += integ.t * pr;
        restore_t(integ.saved_t);
        integ.p += integ.h;
        if (++integ.i < integ.nsteps)
            goto loop2;

        // update integral moving resuslt
        integ.prev_int = (integ.prev_int + integ.sum*integ.h)/2;
        integ.s[integ.k++] = integ.prev_int;

        if (integ.n >= ROMB_K-1) {
            int i, m;
            int ns = ROMB_K-1;
            phloat dm = 1;
            for (i = 0; i < ROMB_K; ++i) integ.c[i] = integ.s[i];
            integ.sum = integ.s[ns];
            for (m = 1; m < ROMB_K; ++m) {
                dm /= 4;
                for (i = 0; i < ROMB_K-m; ++i)
                    integ.c[i] = (integ.c[i+1]-integ.c[i]*dm*4)/(1-dm);
                integ.sum += integ.c[--ns]*dm;
            }

            phloat res = integ.sum * integ.b * 0.75;
            integ.eps = fabs(integ.prev_res - res);
            integ.prev_res = res;
            if (integ.eps <= integ.acc * fabs(res))
                // done!
                return finish_integ();

            for (i = 0; i < ROMB_K-1; ++i) integ.s[i] = integ.s[i+1];
            integ.k = ROMB_K-1;
        }

        integ.nsteps <<= 1;
        integ.h /= 2.0;

        if (++integ.n >= ROMB_MAX)
            return finish_integ(); // too many

        goto loop1;
    default:
        return ERR_INTERNAL_ERROR;
    }
}
