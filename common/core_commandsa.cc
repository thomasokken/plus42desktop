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
#include <limits.h>

#include "core_commandsa.h"
#include "core_commands2.h"
#include "core_commands7.h"
#include "core_commands8.h"
#include "core_display.h"
#include "core_helpers.h"
#include "core_main.h"
#include "core_math1.h"

#define PLOT_STATE_IDLE 0
#define PLOT_STATE_SCANNING 1
#define PLOT_STATE_PLOTTING 2
#define PLOT_STATE_EVAL_MARK1 3
#define PLOT_STATE_EVAL_MARK2 4
#define PLOT_STATE_SOLVE 5
#define PLOT_STATE_INTEG 6

#define PLOT_RESULT_NONE 0
#define PLOT_RESULT_EVAL 1
#define PLOT_RESULT_SOLVE 2
#define PLOT_RESULT_SOLVE_DIRECT 3
#define PLOT_RESULT_SOLVE_SIGN_REVERSAL 4
#define PLOT_RESULT_SOLVE_EXTREMUM 5
#define PLOT_RESULT_SOLVE_FAIL 6
#define PLOT_RESULT_INTEG 7

#define PLOT_FUN 0
#define PLOT_X_VAR 1
#define PLOT_X_UNIT 2
#define PLOT_X_MIN 3
#define PLOT_X_MAX 4
#define PLOT_Y_VAR 5
#define PLOT_Y_UNIT 6
#define PLOT_Y_MIN 7
#define PLOT_Y_MAX 8
#define PLOT_STATE 9
#define PLOT_X_PIXEL 10
#define PLOT_LAST_Y 11
#define PLOT_MARK1_X 12
#define PLOT_MARK1_Y 13
#define PLOT_MARK2_X 14
#define PLOT_MARK2_Y 15
#define PLOT_RESULT 16
#define PLOT_RESULT_TYPE 17
#define PLOT_SIZE 18

struct axis {
    char name[7];
    unsigned char len;
    vartype *unit;
    phloat min;
    phloat max;
};

struct PlotData {
    vartype_list *ppar;
    int err;
    vartype *fun;
    axis axes[2];
    int state;
    int x_pixel;
    phloat last_y;
    phloat mark[4];
    phloat result;
    int result_type;
    PlotData() {
        vartype *v = recall_var("PPAR", 4);
        err = ERR_NONEXISTENT;
        if (v == NULL) {
            problem:
            v = new_list(PLOT_SIZE);
            if (v == NULL) {
                err = ERR_INSUFFICIENT_MEMORY;
                return;
            }
            ppar = (vartype_list *) v;
            for (int i = 0; i < PLOT_SIZE; i++) {
                ppar->array->data[i] = new_real(0);
                if (ppar->array->data[i] == NULL) {
                    free_vartype(v);
                    err = ERR_INSUFFICIENT_MEMORY;
                    return;
                }
            }
            err = store_var("PPAR", 4, v);
            if (err != ERR_NONE) {
                free_vartype(v);
                return;
            }
            goto init_all;
        }
        if (v->type != TYPE_LIST)
            goto problem;
        err = ERR_INSUFFICIENT_MEMORY;
        if (!disentangle(v))
            return;
        ppar = (vartype_list *) v;
        int sz;
        sz = ppar->size;
        if (sz > PLOT_SIZE)
            sz = PLOT_SIZE;
        if (ppar->size < PLOT_SIZE) {
            vartype **new_data = (vartype **) realloc(ppar->array->data, PLOT_SIZE * sizeof(vartype *));
            if (new_data == NULL)
                return;
            ppar->array->data = new_data;
            while (ppar->size < PLOT_SIZE) {
                ppar->array->data[ppar->size] = new_real(0);
                if (ppar->array->data[ppar->size] == NULL)
                    return;
                ppar->size++;
            }
        }
        switch (sz) {
            init_all:
            case PLOT_FUN: if (!set_fun(PLOT_FUN, NULL)) return;
            case PLOT_X_VAR: if (!set_var(PLOT_X_VAR, NULL, 0)) return;
            case PLOT_X_UNIT: if (!set_unit(PLOT_X_UNIT, NULL)) return;
            case PLOT_X_MIN: if (!set_phloat(PLOT_X_MIN, -1)) return;
            case PLOT_X_MAX: if (!set_phloat(PLOT_X_MAX, 1)) return;
            case PLOT_Y_VAR: if (!set_var(PLOT_Y_VAR, NULL, 0)) return;
            case PLOT_Y_UNIT: if (!set_unit(PLOT_Y_UNIT, NULL)) return;
            case PLOT_Y_MIN: if (!set_phloat(PLOT_Y_MIN, -1)) return;
            case PLOT_Y_MAX: if (!set_phloat(PLOT_Y_MAX, 1)) return;
            case PLOT_STATE: if (!set_int(PLOT_STATE, PLOT_STATE_IDLE)) return;
            case PLOT_X_PIXEL: if (!set_int(PLOT_X_PIXEL, 0)) return;
            case PLOT_LAST_Y: if (!set_phloat(PLOT_LAST_Y, 0)) return;
            case PLOT_MARK1_X: if (!set_phloat(PLOT_MARK1_X, 0)) return;
            case PLOT_MARK1_Y: if (!set_phloat(PLOT_MARK1_Y, 0)) return;
            case PLOT_MARK2_X: if (!set_phloat(PLOT_MARK2_X, 0)) return;
            case PLOT_MARK2_Y: if (!set_phloat(PLOT_MARK2_Y, 0)) return;
            case PLOT_RESULT: if (!set_phloat(PLOT_RESULT, 0)) return;
            case PLOT_RESULT_TYPE: if (!set_int(PLOT_RESULT_TYPE, PLOT_RESULT_NONE)) return;
            default:;
        }
        err = ERR_NONE;
        fun = get_fun(PLOT_FUN);
        get_var(PLOT_X_VAR, 0);
        axes[0].unit = get_unit(PLOT_X_UNIT);
        axes[0].min = get_phloat(PLOT_X_MIN, -1);
        axes[0].max = get_phloat(PLOT_X_MAX, 1);
        get_var(PLOT_Y_VAR, 1);
        axes[1].unit = get_unit(PLOT_Y_UNIT);
        axes[1].min = get_phloat(PLOT_Y_MIN, -1);
        axes[1].max = get_phloat(PLOT_Y_MAX, 1);
        state = get_int(PLOT_STATE, 0);
        x_pixel = get_int(PLOT_X_PIXEL, 0);
        last_y = get_phloat(PLOT_LAST_Y, NAN_PHLOAT);
        mark[0] = get_phloat(PLOT_MARK1_X, NAN_PHLOAT);
        mark[1] = get_phloat(PLOT_MARK1_Y, NAN_PHLOAT);
        mark[2] = get_phloat(PLOT_MARK2_X, NAN_PHLOAT);
        mark[3] = get_phloat(PLOT_MARK2_Y, NAN_PHLOAT);
        result = get_phloat(PLOT_RESULT, 0);
        result_type = get_int(PLOT_RESULT_TYPE, 0);
    }
    int get_int(int index, int def) {
        vartype *v = ppar->array->data[index];
        if (v->type == TYPE_REAL)
            return to_int(((vartype_real *) v)->x);
        else
            return def;
    }
    vartype *get_fun(int index) {
        vartype *v = ppar->array->data[index];
        if (v != NULL && (v->type == TYPE_STRING || v->type == TYPE_EQUATION))
            return v;
        else
            return NULL;
    }
    phloat get_phloat(int index, phloat def) {
        vartype *v = ppar->array->data[index];
        if (v->type == TYPE_REAL)
            return ((vartype_real *) v)->x;
        else
            return def;
    }
    vartype *get_unit(int index) {
        return ppar->array->data[index];
    }
    void get_var(int index, int a) {
        vartype *v = ppar->array->data[index];
        if (v->type == TYPE_STRING) {
            vartype_string *s = (vartype_string *) v;
            int slen = s->length;
            if (slen > 7)
                slen = 7;
            string_copy(axes[a].name, &axes[a].len, s->txt(), slen);
        } else {
            axes[a].len = 0;
        }
    }
    bool set_int(int index, int val) {
        if (ppar->array->data[index] != NULL && ppar->array->data[index]->type == TYPE_REAL) {
            ((vartype_real *) ppar->array->data[index])->x = val;
            return true;
        }
        vartype *v = new_real(val);
        if (v == NULL)
            return false;
        free_vartype(ppar->array->data[index]);
        ppar->array->data[index] = v;
        return true;
    }
    bool set_fun(int index, vartype *v) {
        if (v == NULL) {
            v = new_real(0);
            if (v == NULL)
                return false;
        }
        free_vartype(ppar->array->data[index]);
        ppar->array->data[index] = v;
        return true;
    }
    bool set_phloat(int index, phloat val) {
        if (ppar->array->data[index] != NULL && ppar->array->data[index]->type == TYPE_REAL) {
            ((vartype_real *) ppar->array->data[index])->x = val;
            return true;
        }
        vartype *v = new_real(val);
        if (v == NULL)
            return false;
        free_vartype(ppar->array->data[index]);
        ppar->array->data[index] = v;
        return true;
    }
    bool set_unit(int index, vartype *v) {
        if (v == NULL)
            v = new_real(0);
        else
            v = dup_vartype(v);
        if (v == NULL)
            return false;
        ((vartype_real *) v)->x = 0;
        free_vartype(ppar->array->data[index]);
        ppar->array->data[index] = v;
        return true;
    }
    bool set_var(int index, char *name, int len) {
        vartype *v = new_string(name, len);
        if (v == NULL)
            return false;
        free_vartype(ppar->array->data[index]);
        ppar->array->data[index] = v;
        return true;
    }
    int conv_x(phloat x) {
        return to_int((x - axes[0].min) / (axes[0].max - axes[0].min) * (disp_w - 1) + 0.5);
    }
    int conv_y(phloat y) {
        return to_int((axes[1].max - y) / (axes[1].max - axes[1].min) * (disp_h - 1) + 0.5);
    }
};

int docmd_pgmplot(arg_struct *arg) {
    int err;
    if (arg->type == ARGTYPE_IND_NUM
            || arg->type == ARGTYPE_IND_STK
            || arg->type == ARGTYPE_IND_STR) {
        err = resolve_ind_arg(arg);
        if (err != ERR_NONE)
            return err;
    }
    if (arg->type != ARGTYPE_STR)
        return ERR_INVALID_TYPE;
    PlotData data;
    if (data.err != ERR_NONE)
        return data.err;
    vartype *s = new_string(arg->val.text, arg->length);
    if (s == NULL || !data.set_fun(PLOT_FUN, s)) {
        free_vartype(s);
        return ERR_INSUFFICIENT_MEMORY;
    }
    return ERR_NONE;
}

int docmd_eqnplot(arg_struct *arg) {
    vartype *eq;
    int err = get_arg_equation(arg, (vartype_equation **) &eq);
    if (err != ERR_NONE)
        return err;
    PlotData data;
    if (data.err != ERR_NONE)
        return data.err;
    eq = dup_vartype(eq);
    if (eq == NULL || !data.set_fun(PLOT_FUN, eq)) {
        free_vartype(eq);
        return ERR_INSUFFICIENT_MEMORY;
    }
    return ERR_NONE;
}

static void add_axis(char *buf, int buflen, int *pos, PlotData *data, int a) {
    char2buf(buf, buflen, pos, 'X' + a);
    char2buf(buf, buflen, pos, ':');
    if (data->axes[a].len > 0)
        string2buf(buf, buflen, pos, data->axes[a].name, data->axes[a].len);
    else
        string2buf(buf, 22, pos, "<STK>", 5);
}

static void display_axes(int row, PlotData *data) {
    char buf[22];
    int pos = 0;
    add_axis(buf, 22, &pos, data, 0);
    char2buf(buf, 22, &pos, ' ');
    add_axis(buf, 22, &pos, data, 1);
    draw_message(row, buf, pos);
}

static void validate_axes(PlotData *data) {
    std::vector<std::string> params;
    if (data->fun == NULL) {
        /* Nope */
    } else if (data->fun->type == TYPE_STRING) {
        vartype_string *s = (vartype_string *) data->fun;
        params = get_mvars(s->txt(), s->length);
    } else if (data->fun->type == TYPE_EQUATION) {
        equation_data *eqd = ((vartype_equation *) data->fun)->data;
        params = get_parameters(eqd);
    }
    if (data->axes[0].len == 0
            || string_equals(data->axes[0].name, data->axes[0].len, data->axes[1].name, data->axes[1].len)) {
        data->axes[1].len = 0;
        data->set_var(PLOT_Y_VAR, NULL, 0);
    }
    for (int i = 0; i < 2; i++) {
        if (data->axes[i].len == 0)
            break;
        bool found = false;
        for (int j = 0; j < params.size(); j++) {
            std::string s = params[j];
            if (string_equals(data->axes[i].name, data->axes[i].len, s.c_str(), s.length())) {
                found = true;
                break;
            }
        }
        if (!found) {
            data->axes[i].len = 0;
            data->set_var(i == 0 ? PLOT_X_VAR : PLOT_Y_VAR, NULL, 0);
        }
    }
}

static int do_axes_menu(PlotData *data) {
    set_menu(MENULEVEL_APP, MENU_GRAPH_AXES);
    if (flags.f.prgm_mode)
        return ERR_NONE;
    validate_axes(data);
    display_axes(0, data);
    return ERR_NONE;
}

int docmd_plot_m(arg_struct *arg) {
    set_menu(MENULEVEL_APP, MENU_GRAPH);
    if (!flags.f.prgm_mode)
        display_plot_params(-1);
    return ERR_NONE;
}

int docmd_param(arg_struct *arg) {
    PlotData data;
    if (data.err != ERR_NONE)
        return data.err;
    return do_axes_menu(&data);
}

int appmenu_exitcallback_7(int menuid, bool exitall) {
    if (menuid == MENU_NONE && !exitall)
        return docmd_param(NULL);
    else
        mode_appmenu = menuid;
    return ERR_NONE;
}

int appmenu_exitcallback_8(int menuid, bool exitall) {
    if (menuid == MENU_NONE && !exitall)
        set_menu(MENULEVEL_APP, MENU_GRAPH);
    else
        mode_appmenu = menuid;
    return ERR_NONE;
}

int start_graph_varmenu(int role, int exit_cb) {
    PlotData data;
    if (data.err != ERR_NONE)
        return data.err;
    int err;
    if (data.fun == NULL)
        return ERR_NONEXISTENT;
    if (role == 5 && data.axes[0].len == 0)
        return ERR_RESTRICTED_OPERATION;
    if (data.fun->type == TYPE_STRING) {
        vartype_string *s = (vartype_string *) data.fun;
        err = start_varmenu_lbl(s->txt(), s->length, role);
    } else {
        err = start_varmenu_eqn(data.fun, role);
    }
    if (err == ERR_NONE)
        set_appmenu_exitcallback(exit_cb);
    return err;
}

static int axis2(arg_struct *arg, int which) {
    int err;
    if (arg->type == ARGTYPE_IND_NUM
            || arg->type == ARGTYPE_IND_STK
            || arg->type == ARGTYPE_IND_STR) {
        err = resolve_ind_arg(arg);
        if (err != ERR_NONE)
            return err;
    }
    PlotData data;
    if (data.err != ERR_NONE)
        return data.err;
    if (which != 'X' && which != 'Y')
        return ERR_INVALID_CONTEXT;
    int a = which - 'X';
    int idx = PLOT_X_VAR + a * (PLOT_Y_VAR - PLOT_X_VAR);
    string_copy(data.axes[a].name, &data.axes[a].len, arg->val.text, arg->length);
    if (!data.set_var(idx, arg->val.text, arg->length))
        return ERR_INSUFFICIENT_MEMORY;
    if (get_front_menu() == MENU_VARMENU)
        return do_axes_menu(&data);
    return ERR_NONE;
}

int docmd_xaxis(arg_struct *arg) {
    return axis2(arg, 'X');
}

int docmd_yaxis(arg_struct *arg) {
    return axis2(arg, 'Y');
}

int docmd_const(arg_struct *arg) {
    return start_graph_varmenu(7, 8);
}

static void display_view() {
    PlotData data;
    if (data.err != ERR_NONE)
        return;
    char *buf = (char *) malloc(disp_c);
    if (buf == NULL) {
        draw_message(0, "<Low Mem>", 9);
        return;
    }
    int pos;
    if (disp_r >= 4) {
        for (int i = 0; i < 4; i++) {
            pos = 0;
            char2buf(buf, disp_c, &pos, i < 2 ? 'X' : 'Y');
            string2buf(buf, disp_c, &pos, i % 2 == 0 ? "MIN=" : "MAX=", 4);
            axis *a = data.axes + (i / 2);
            phloat p = i % 2 == 0 ? a->min : a->max;
            pos += easy_phloat2string(p, buf + pos, disp_c - pos, 0);
            if (a->unit->type == TYPE_UNIT) {
                vartype_unit *u = (vartype_unit *) a->unit;
                char2buf(buf, disp_c, &pos, '_');
                string2buf(buf, disp_c, &pos, u->text, u->length);
            }
            draw_message(i, buf, pos);
        }
    } else {
        vartype_complex c;
        c.type = TYPE_COMPLEX;
        bool saved_polar = flags.f.polar;
        flags.f.polar = 0;
        for (int i = 0; i < 2; i++) {
            pos = 0;
            string2buf(buf, disp_c, &pos, i == 0 ? "X:" : "Y:", 2);
            axis *a = data.axes + i;
            c.re = a->min;
            c.im = a->max;
            pos += vartype2string((vartype *) &c, buf + pos, disp_c - pos);
            for (int j = 0; j < pos - 2; j++) {
                if (buf[j] == ' ') {
                    buf[j] = 26;
                    memmove(buf + j + 1, buf + j + 2, pos - j - 2);
                    pos--;
                    break;
                }
            }
            if (a->unit->type == TYPE_UNIT) {
                char2buf(buf, disp_c, &pos, '_');
                vartype_unit *u = (vartype_unit *) a->unit;
                string2buf(buf, disp_c, &pos, u->text, u->length);
            }
            draw_message(i, buf, pos);
        }
        flags.f.polar = saved_polar;
    }
    free(buf);
}

int docmd_view_p(arg_struct *arg) {
    PlotData data;
    if (data.err != ERR_NONE)
        return data.err;
    set_menu(MENULEVEL_APP, MENU_GRAPH_VIEW);
    if (!flags.f.prgm_mode)
        display_view();
    return ERR_NONE;
}

static int plot_view_helper(bool do_x, bool min) {
    PlotData data;
    if (data.err != ERR_NONE)
        return data.err;
    vartype *sx = stack[sp];
    phloat p = ((vartype_real *) sx)->x;
    int offset = do_x ? PLOT_X_MIN : PLOT_Y_MIN;
    if (!min)
        offset++;
    if (!data.set_phloat(offset, p))
        return ERR_INSUFFICIENT_MEMORY;

    axis *a = &data.axes[do_x ? 0 : 1];
    phloat converted_other;
    ((vartype_real *) a->unit)->x = min ? a->max : a->min;
    int err = convert_helper(sx, a->unit, &converted_other);
    ((vartype_real *) a->unit)->x = 0;

    if (!data.set_unit(do_x ? PLOT_X_UNIT : PLOT_Y_UNIT, sx))
        return ERR_INSUFFICIENT_MEMORY;

    if (err == ERR_NONE) {
        if (min) {
            offset++;
            a->max = converted_other;
        } else {
            offset--;
            a->min = converted_other;
        }
        data.set_phloat(offset, converted_other);
    }

    char *buf = (char *) malloc(disp_c);
    if (buf == NULL)
        return ERR_NONE;
    int pos = 0;
    char2buf(buf, disp_c, &pos, do_x ? 'X' : 'Y');
    string2buf(buf, disp_c, &pos, min ? "MIN=" : "MAX=", 4);
    pos += easy_phloat2string(p, buf + pos, disp_c - pos, 0);
    if (sx->type == TYPE_UNIT) {
        char2buf(buf, disp_c, &pos, '_');
        vartype_unit *u = (vartype_unit *) sx;
        string2buf(buf, disp_c, &pos, u->text, u->length);
    }
    draw_message(0, buf, pos);
    free(buf);
    return ERR_NONE;
}

int docmd_xmin(arg_struct *arg) {
    return plot_view_helper(true, true);
}

int docmd_xmax(arg_struct *arg) {
    return plot_view_helper(true, false);
}

int docmd_ymin(arg_struct *arg) {
    return plot_view_helper(false, true);
}

int docmd_ymax(arg_struct *arg) {
    return plot_view_helper(false, false);
}

static int call_plot_function(PlotData *data, phloat x) {
    vartype *eq = NULL;
    int err;
    pgm_index prev_prgm = current_prgm;
    int4 prev_pc = pc;

    vartype *v;
    if (data->axes[0].unit->type == TYPE_REAL) {
        v = new_real(x);
    } else {
        vartype_unit *u = (vartype_unit *) data->axes[0].unit;
        v = new_unit(x, u->text, u->length);
    }
    if (v == NULL)
        return ERR_INSUFFICIENT_MEMORY;

    if (data->axes[0].len > 0) {
        err = store_var(data->axes[0].name, data->axes[0].len, v);
    } else {
        err = recall_result(v);
        v = NULL;
    }
    if (err != ERR_NONE) {
        free_vartype(v);
        return err;
    }

    if (data->axes[1].len > 0) {
        vartype_unit ymin, ymax;
        ymin.type = ymax.type = data->axes[1].unit->type;
        if (ymin.type == TYPE_UNIT) {
            vartype_unit *u = (vartype_unit *) data->axes[1].unit;
            ymin.text = ymax.text = u->text;
            ymin.length = ymax.length = u->length;
        }
        if (data->state == PLOT_STATE_SCANNING) {
            /* Scanning */
            ymin.x = data->mark[1];
            ymax.x = data->mark[3];
        } else {
            /* Plotting, evaluating, etc. */
            ymin.x = data->axes[1].min;
            ymax.x = data->axes[1].max;
        }
        if (p_isnan(ymin.x) || p_isnan(ymax.x))
            ymin.x = ymax.x = 0;
        return start_solve(-5, data->axes[1].name, data->axes[1].len, (vartype *) &ymin, (vartype *) &ymax, &mode_plot_inv);
    }

    if (data->fun == NULL) {
        return ERR_NONEXISTENT;
    } else if (data->fun->type == TYPE_STRING) {
        vartype_string *s = (vartype_string *) data->fun;
        if (s->length > 7)
            return ERR_NAME_TOO_LONG;
        if (s->length == 0)
            return ERR_LABEL_NOT_FOUND;
        arg_struct arg;
        arg.type = ARGTYPE_STR;
        string_copy(arg.val.text, &arg.length, s->txt(), s->length);
        err = docmd_gto(&arg);
        if (err != ERR_NONE)
            return err;
    } else {
        eq = data->fun;
        equation_data *eqd = ((vartype_equation *) data->fun)->data;
        current_prgm.set(eq_dir->id, eqd->eqn_index);
        pc = 0;
    }
    pgm_index plot_index;
    plot_index.set(0, -5);
    err = push_rtn_addr(plot_index, 0);
    if (err == ERR_NONE) {
        if (eq != NULL) {
            err = store_stack_reference(eq);
            if (err != ERR_NONE)
                goto fail;
        }
        return ERR_RUN;
    } else {
        fail:
        current_prgm = prev_prgm;
        pc = prev_pc;
        return err;
    }
}

static int do_it(PlotData *data) {
    phloat xmin = data->axes[0].min;
    phloat xmax = data->axes[0].max;
    int pixel = data->x_pixel;
    phloat x = xmin + (xmax - xmin) * ((phloat) pixel) / (disp_w - 1);
    return call_plot_function(data, x);
}

static int prepare_plot(PlotData *data) {
    if (data->axes[1].len > 0) {
        /* Named dependent; that means we're going to use the solver, and
         * *that* means the independent must have a name as well...
         */
        if (data->axes[0].len == 0)
            return ERR_INVALID_DATA;
    }

    std::vector<std::string> params;
    vartype_string *pgm_name;

    /* Check for existence of program or equation */
    if (data->fun == NULL) {
        return ERR_NONEXISTENT;
    } else if (data->fun->type == TYPE_STRING) {
        pgm_name = (vartype_string *) data->fun;
        if (pgm_name->length > 7)
            return ERR_NAME_TOO_LONG;
        if (pgm_name->length == 0)
            return ERR_LABEL_NOT_FOUND;
        arg_struct arg;
        string_copy(arg.val.text, &arg.length, pgm_name->txt(), pgm_name->length);
        pgm_index dummy_idx;
        int4 dummy_pc;
        if (!find_global_label(&arg, &dummy_idx, &dummy_pc))
            return ERR_LABEL_NOT_FOUND;
        if (data->axes[0].len > 0)
            params = get_mvars(pgm_name->txt(), pgm_name->length);
    } else {
        equation_data *eqd = ((vartype_equation *) data->fun)->data;
        if (data->axes[0].len > 0)
            params = get_parameters(eqd);
    }

    /* Make sure named parameters, if any, exist */
    if (data->axes[0].len > 0) {
        bool x_found = false, y_found = data->axes[1].len == 0;
        for (int i = 0; i < params.size(); i++) {
            std::string s = params[i];
            if (!x_found && string_equals(data->axes[0].name, data->axes[0].len, s.c_str(), s.length())) {
                x_found = true;
                if (y_found)
                    break;
            }
            if (!y_found && string_equals(data->axes[1].name, data->axes[1].len, s.c_str(), s.length())) {
                y_found = true;
                if (x_found)
                    break;
            }
        }
        if (!x_found || !y_found)
            return ERR_INVALID_DATA;
    }

    /* Preserve stack, and program location */
    if (program_running()) {
        int err = push_rtn_addr(current_prgm, pc);
        if (err != ERR_NONE)
            return err;
    } else {
        clear_all_rtns();
        return_here_after_last_rtn();
        set_running(true);
    }
    int err = push_func_state(0);
    if (err != ERR_NONE)
        return err;

    /* Save RPN stack pointer, which we'll use to remove leftovers
     * from the stack between function invocations
     */
    mode_plot_sp = flags.f.big_stack ? sp : -2;

    /* mode_plot_inv is used to preserve the generated inverse,
     * when plotting a function using the direct solver.
     */
    free_vartype(mode_plot_inv);
    mode_plot_inv = NULL;

    /* Prep solver, if needed */
    if (data->axes[1].len > 0) {
        if (data->fun->type == TYPE_STRING)
            set_solve_prgm(pgm_name->txt(), pgm_name->length);
        else
            set_solve_eqn(data->fun);
    }
    return ERR_NONE;
}

int docmd_scan(arg_struct *arg) {
    PlotData data;
    if (data.err != ERR_NONE)
        return data.err;
    data.set_int(PLOT_STATE, data.state = PLOT_STATE_SCANNING);
    data.set_int(PLOT_X_PIXEL, data.x_pixel = 0);
    data.set_phloat(PLOT_MARK1_X, data.mark[0] = NAN_PHLOAT);
    data.set_phloat(PLOT_MARK1_Y, data.mark[1] = data.axes[1].min);
    data.set_phloat(PLOT_MARK2_X, data.mark[2] = NAN_PHLOAT);
    data.set_phloat(PLOT_MARK2_Y, data.mark[3] = data.axes[1].max);
    data.set_phloat(PLOT_Y_MIN, data.axes[1].min = NAN_PHLOAT);
    data.set_phloat(PLOT_Y_MAX, data.axes[1].max = NAN_PHLOAT);

    int err = prepare_plot(&data);
    if (err != ERR_NONE)
        return err;
    return do_it(&data);
}

void display_view_param(int key) {
    if (key == 4) {
        display_view();
        return;
    }
    PlotData data;
    if (data.err != ERR_NONE)
        return;
    char *buf = (char *) malloc(disp_c);
    if (buf == NULL)
        return;
    int pos = 0;
    char2buf(buf, disp_c, &pos, key < 2 ? 'X' : 'Y');
    string2buf(buf, disp_c, &pos, key % 2 == 0 ? "MIN=" : "MAX=", 4);
    axis *a = data.axes + (key / 2);
    phloat p = key % 2 == 0 ? a->min : a->max;
    pos += easy_phloat2string(p, buf + pos, disp_c - pos, 0);
    if (a->unit->type == TYPE_UNIT) {
        vartype_unit *u = (vartype_unit *) a->unit;
        char2buf(buf, disp_c, &pos, '_');
        string2buf(buf, disp_c, &pos, u->text, u->length);
    }
    draw_message(0, buf, pos);
    free(buf);
    vartype *v = dup_vartype(a->unit);
    if (v != NULL) {
        ((vartype_real *) v)->x = p;
        recall_result(v);
    }
    redisplay();
}

void display_plot_params(int key) {
    PlotData data;
    if (data.err != ERR_NONE)
        return;

    switch (key) {
        case -1:
        case 0:
        case 1: {
            vartype *v = NULL;
            char *buf = (char *) malloc(disp_c);
            if (buf == NULL)
                return;
            int pos = 0;
            if (data.fun == NULL) {
                /* Nothing to see here */
            } else if (data.fun->type == TYPE_STRING) {
                vartype_string *s = (vartype_string *) data.fun;
                char2buf(buf, disp_c, &pos, '"');
                string2buf(buf, disp_c, &pos, s->txt(), s->length);
                char2buf(buf, disp_c, &pos, '"');
                v = dup_vartype(data.fun);
            } else if (data.fun->type == TYPE_EQUATION) {
                vartype_equation *eq = (vartype_equation *) data.fun;
                char d = eq->data->compatMode ? '`' : '\'';
                char2buf(buf, disp_c, &pos, d);
                string2buf(buf, disp_c, &pos, eq->data->text, eq->data->length);
                char2buf(buf, disp_c, &pos, d);
                v = dup_vartype(data.fun);
            }
            if (pos == 0) {
                draw_message(0, "No Plot Function Set", 20);
                flush_display();
            } else {
                draw_message(0, buf, pos);
                if (key == -1) {
                    validate_axes(&data);
                    display_axes(1, &data);
                    redisplay();
                } else if (v != NULL && key != -1) {
                    if (recall_result(v) == ERR_NONE)
                        redisplay();
                }
            }
            free(buf);
            return;
        }
        case 2: {
            display_axes(0, &data);
            return;
        }
        case 3: {
            if (data.fun == NULL)
                return;
            int err;
            if (data.fun->type == TYPE_STRING) {
                vartype_string *s = (vartype_string *) data.fun;
                if (s->length == 0)
                    err = ERR_NONEXISTENT;
                else if (s->length > 7)
                    err = ERR_NAME_TOO_LONG;
                else {
                    arg_struct arg;
                    arg.type = ARGTYPE_STR;
                    string_copy(arg.val.text, &arg.length, s->txt(), s->length);
                    err = docmd_pgmvar(&arg);
                }
            } else {
                vartype *saved_lastx = lastx;
                lastx = data.fun;
                arg_struct arg;
                arg.type = ARGTYPE_STK;
                arg.val.stk = 'L';
                err = docmd_eqnvar(&arg);
                lastx = saved_lastx;
            }
            if (err == ERR_NONE)
                draw_message(0, "Variables Printed", 17);
            else {
                display_error(err, false);
                flush_display();
            }
            return;
        }
        case 4: {
            display_view();
            return;
        }
    }
}

static int plot_helper(bool reset);

static vartype *integ_result_unit(PlotData *data) {
    vartype_unit *ux = (vartype_unit *) data->axes[0].unit;
    vartype_unit *uy = (vartype_unit *) data->axes[1].unit;
    if (ux->type == TYPE_UNIT) {
        if (uy->type == TYPE_UNIT) {
            std::string r;
            if (!normalize_unit(std::string(ux->text, ux->length) + "*" + std::string(uy->text, uy->length), &r))
                return NULL;
            return new_unit(0, r.c_str(), r.length());
        } else {
            return dup_vartype((vartype *) ux);
        }
    } else {
        return dup_vartype((vartype *) uy);
    }
}

static int phloat2string_four_digits(phloat p, char *buf, int buflen) {
    int dispmode;
    if (flags.f.fix_or_all)
        dispmode = flags.f.eng_or_all ? 3 : 0;
    else
        dispmode = flags.f.eng_or_all ? 2 : 1;
    int digits = 0;
    if (flags.f.digits_bit3)
        digits += 8;
    if (flags.f.digits_bit2)
        digits += 4;
    if (flags.f.digits_bit1)
        digits += 2;
    if (flags.f.digits_bit0)
        digits += 1;
    if (digits > 4)
        digits = 4;
    return phloat2string(p, buf, buflen, 0, digits, dispmode, 0, 4);
}

int return_to_plot(bool failure, bool stop) {
    PlotData data;
    if (data.err != ERR_NONE)
        return data.err;

    int pixel = data.x_pixel;
    int state = data.state;
    vartype *result = NULL;
    bool replot = false;
    // In case we were interrupted...
    mode_message_lines = ALL_LINES;

    vartype *res = stack[sp];
    phloat ymin = data.axes[1].min;
    phloat ymax = data.axes[1].max;

    if (!failure && sp != -1 && (res->type == TYPE_REAL || res->type == TYPE_UNIT)) {
        phloat errp;
        if ((state == PLOT_STATE_SOLVE || state != PLOT_STATE_INTEG && data.axes[1].len > 0)
                && stack[sp - 1]->type != TYPE_STRING
                && (errp = ((vartype_real *) stack[sp - 3])->x) != SOLVE_ROOT) {
            // Not an error, but the solver didn't find a root
            if (state == PLOT_STATE_SOLVE) {
                replot = true;
                if (errp != SOLVE_SIGN_REVERSAL && errp != SOLVE_EXTREMUM) {
                    // Bad Guess(es) or Constant? -- actual failures to find anything
                    data.set_phloat(PLOT_RESULT, data.result = errp);
                    data.set_int(PLOT_RESULT_TYPE, data.result_type = PLOT_RESULT_SOLVE_FAIL);
                } else {
                    // Sign Reversal or Extremum -- we want to return the actual point
                    // the solver ended up converging on
                    phloat x;
                    int err = convert_helper(data.axes[0].unit, res, &x);
                    if (err != ERR_NONE)
                        return err;
                    data.set_phloat(PLOT_RESULT, data.result = x);
                    if (errp == SOLVE_SIGN_REVERSAL)
                        data.set_int(PLOT_RESULT_TYPE, data.result_type = PLOT_RESULT_SOLVE_SIGN_REVERSAL);
                    else
                        data.set_int(PLOT_RESULT_TYPE, data.result_type = PLOT_RESULT_SOLVE_EXTREMUM);
                    if (data.axes[0].unit->type == TYPE_UNIT) {
                        vartype_unit *u = (vartype_unit *) data.axes[0].unit;
                        result = new_unit(x, u->text, u->length);
                    } else {
                        result = new_real(x);
                    }
                }
            }
            goto fail;
        }
        phloat y;
        if (state == PLOT_STATE_INTEG) {
            // Special case: this returns a result whose unit is the product
            // of the X and Y axis units
            vartype *u = integ_result_unit(&data);
            if (u == NULL)
                return ERR_INVALID_UNIT;
            int err = convert_helper(u, res, &y);
            if (err != ERR_NONE) {
                free_vartype(u);
                return err;
            }
            ((vartype_real *) u)->x = y;
            result = u;
        } else if (state == PLOT_STATE_SOLVE) {
            // And, of course, in the case of SOLVE, the result will have X axis units
            int err = convert_helper(data.axes[0].unit, res, &y);
            if (err != ERR_NONE)
                return err;
        } else if (state == PLOT_STATE_SCANNING) {
            if (data.axes[1].len != 0 && stack[sp - 1]->type != TYPE_STRING) {
                // Result from the numerical solver; this means the unit must
                // already be determined, and we just use the converted result
                if (data.axes[1].unit->type == TYPE_REAL) {
                    y = ((vartype_real *) res)->x;
                } else {
                    int err = convert_helper(data.axes[1].unit, res, &y);
                    if (err != ERR_NONE)
                        return err;
                }
                if (p_isnan(ymin) || y < ymin)
                    data.set_phloat(PLOT_Y_MIN, data.axes[1].min = y);
                if (p_isnan(ymax) || y > ymax)
                    data.set_phloat(PLOT_Y_MAX, data.axes[1].max = y);
            } else {
                // If the solver isn't used, or if we're getting a result from
                // the direct solver (which you can tell by there being a string
                // in the Y register), then we can just take the unit of the
                // result.
                int err = convert_helper(data.axes[1].unit, res, &y);
                if (err == ERR_NONE) {
                    if (p_isnan(ymin) || y < ymin)
                        data.set_phloat(PLOT_Y_MIN, data.axes[1].min = y);
                    if (p_isnan(ymax) || y > ymax)
                        data.set_phloat(PLOT_Y_MAX, data.axes[1].max = y);
                } else {
                    vartype *u = dup_vartype(res);
                    y = ((vartype_real *) u)->x;
                    ((vartype_real *) u)->x = 0;
                    data.set_unit(PLOT_Y_UNIT, data.axes[1].unit = u);
                    data.set_phloat(PLOT_Y_MIN, data.axes[1].min = y);
                    data.set_phloat(PLOT_Y_MAX, data.axes[1].max = y);
                }
            }
        } else if (data.axes[1].unit->type == TYPE_REAL) {
            if (res->type == TYPE_UNIT) {
                vartype *u = dup_vartype(res);
                if (u == NULL)
                    return ERR_INSUFFICIENT_MEMORY;
                ((vartype_unit *) u)->x = 0;
                data.set_unit(PLOT_Y_UNIT, data.axes[1].unit = u);
            }
            y = ((vartype_real *) res)->x;
        } else {
            int err = convert_helper(data.axes[1].unit, res, &y);
            if (err != ERR_NONE)
                return err;
        }

        if (state == PLOT_STATE_PLOTTING) {
            int v = to_int(floor((ymax - y) / (ymax - ymin) * (disp_h - 1) + 0.5));
            phloat lasty = data.last_y;
            data.set_phloat(PLOT_LAST_Y, data.last_y = y);
            if (p_isnan(lasty)) {
                if (v >= 0 && v < disp_h && pixel >= 0) {
                    draw_pixel(to_int(pixel), v);
                    flush_display();
                }
            } else {
                int lv = to_int(floor((ymax - lasty) / (ymax - ymin) * (disp_h - 1) + 0.5));
                /* Don't draw lines if both endpoints are off-screen */
                if (lv >= 0 && lv < disp_h || v >= 0 && v < disp_h) {
                    int x = to_int(pixel);
                    draw_line(x - 1, lv, x, v);
                    flush_display();
                }
            }
            int mark = 0;
            phloat x, xm1, xm2;
            if (!p_isnan(data.mark[0]) && data.conv_x(data.mark[0]) == pixel) {
                mark = 1;
                goto draw_dotted_line;
            } else if (!p_isnan(data.mark[2]) && data.conv_x(data.mark[2]) == pixel) {
                mark = 2;
                goto draw_dotted_line;
            }
            if (data.result_type == PLOT_RESULT_INTEG) {
                x = data.axes[0].min + ((phloat) pixel) / (disp_w - 1) * (data.axes[0].max - data.axes[0].min);
                xm1 = data.mark[0];
                xm2 = data.mark[2];
                if (xm1 > xm2) {
                    phloat t = xm1;
                    xm1 = xm2;
                    xm2 = t;
                }
                if (x >= xm1 && x <= xm2) {
                    draw_dotted_line:
                    int vz = data.conv_y(0);
                    if (v > vz) {
                        int t = vz;
                        vz = v;
                        v = t;
                    }
                    if (v < 0)
                        v = 0;
                    if (vz >= disp_h)
                        vz = disp_h - 1;
                    if (mark != 0) {
                        int vm = data.conv_y(data.mark[(mark - 1) * 2 + 1]);
                        for (int Y = vm - 1; Y <= vm + 1; Y++)
                            for (int X = pixel - 1; X <= pixel + 1; X++)
                                draw_pixel(X, Y);
                    }
                    bool solid = mark != 0 && data.result_type == PLOT_RESULT_INTEG;
                    for (int j = v; j <= vz; j++)
                        if (solid || ((pixel + j) & 1) != 0)
                            draw_pixel(pixel, j);
                }
            }
        } else if (state == PLOT_STATE_EVAL_MARK1 || state == PLOT_STATE_EVAL_MARK2) {
            int k = 2 * (state - PLOT_STATE_EVAL_MARK1);
            replot = true;
            data.set_phloat(PLOT_RESULT, data.result = y);
            data.set_int(PLOT_RESULT_TYPE, data.result_type = PLOT_RESULT_EVAL);
            result = new_complex(data.mark[k], y);
        } else if (state == PLOT_STATE_SOLVE) {
            replot = true;
            data.set_phloat(PLOT_RESULT, data.result = y);
            data.set_int(PLOT_RESULT_TYPE, data.result_type =
                    stack[sp - 1]->type == TYPE_STRING ? PLOT_RESULT_SOLVE_DIRECT : PLOT_RESULT_SOLVE);
            if (data.axes[0].unit->type == TYPE_UNIT) {
                vartype_unit *u = (vartype_unit *) data.axes[0].unit;
                result = new_unit(y, u->text, u->length);
            } else {
                result = new_real(y);
            }
        } else if (state == PLOT_STATE_INTEG) {
            replot = true;
            data.set_phloat(PLOT_RESULT, data.result = y);
            data.set_int(PLOT_RESULT_TYPE, data.result_type = PLOT_RESULT_INTEG);
            // already set result earlier
        }
    } else {
        fail:
        if (state == PLOT_STATE_PLOTTING) {
            data.set_phloat(PLOT_LAST_Y, data.last_y = NAN_PHLOAT);
        }
    }
    clean_stack(mode_plot_sp);
    pixel += state == PLOT_STATE_SCANNING ? 10 : 1;
    data.set_int(PLOT_X_PIXEL, data.x_pixel = pixel);
    int err;
    switch (state) {
        case PLOT_STATE_SCANNING:
            if (pixel >= disp_w) {
                phloat ymin = data.axes[1].min;
                phloat ymax = data.axes[1].max;
                if (p_isnan(ymin)) {
                    ymin = -1;
                    ymax = 1;
                } else if (ymax == ymin) {
                    ymin -= 1;
                    ymax += 1;
                } else {
                    phloat h = (ymax - ymin) / 10;
                    ymax += h;
                    ymin -= h;
                }
                data.set_phloat(PLOT_Y_MIN, data.axes[1].min = ymin);
                data.set_phloat(PLOT_Y_MAX, data.axes[1].max = ymax);
                display_view();
                break;
            } else {
                err = do_it(&data);
                if (err == ERR_NONE && stop)
                    err = ERR_STOP;
                return err;
            }
        case PLOT_STATE_PLOTTING:
            if (pixel > disp_w) {
                if (state == PLOT_STATE_PLOTTING && data.result_type != PLOT_RESULT_NONE) {
                    char buf[100];
                    int pos = 0;
                    vartype *result_unit = NULL;
                    switch (data.result_type) {
                        case PLOT_RESULT_EVAL: {
                            if (data.axes[1].len == 0)
                                string2buf(buf, 100, &pos, "<Y>", 3);
                            else
                                string2buf(buf, 100, &pos, data.axes[1].name, data.axes[1].len);
                            char2buf(buf, 100, &pos, '=');
                            result_unit = data.axes[1].unit;
                            break;
                        }
                        case PLOT_RESULT_SOLVE:
                        case PLOT_RESULT_SOLVE_DIRECT:
                        case PLOT_RESULT_SOLVE_SIGN_REVERSAL:
                        case PLOT_RESULT_SOLVE_EXTREMUM: {
                            if (data.axes[0].len == 0)
                                string2buf(buf, 100, &pos, "<X>", 3);
                            else
                                string2buf(buf, 100, &pos, data.axes[0].name, data.axes[0].len);
                            char2buf(buf, 100, &pos, '=');
                            int x = data.conv_x(data.result);
                            int y = data.conv_y(0);
                            for (int i = 2; i <= 4; i++) {
                                draw_pixel(x + i, y + i);
                                draw_pixel(x + i, y - i);
                                draw_pixel(x - i, y - i);
                                draw_pixel(x - i, y + i);
                            }
                            result_unit = data.axes[0].unit;
                            break;
                        }
                        case PLOT_RESULT_SOLVE_FAIL: {
                            int m = to_int(data.result);
                            string2buf(buf, 100, &pos, solve_message[m].text, solve_message[m].length);
                            break;
                        }
                        case PLOT_RESULT_INTEG: {
                            string2buf(buf, 100, &pos, "\3=", 2);
                            result_unit = integ_result_unit(&data);
                            break;
                        }
                    }
                    if (data.result_type != PLOT_RESULT_SOLVE_FAIL) {
                        pos += phloat2string_four_digits(data.result, buf + pos, 100 - pos);
                        if (result_unit != NULL && result_unit->type == TYPE_UNIT) {
                            vartype_unit *u = (vartype_unit *) result_unit;
                            char2buf(buf, 100, &pos, '_');
                            string2buf(buf, 100, &pos, u->text, u->length);
                            if (data.result_type == PLOT_RESULT_INTEG)
                                free_vartype(result_unit);
                        }
                        if (data.result_type == PLOT_RESULT_SOLVE_DIRECT
                                || data.result_type == PLOT_RESULT_SOLVE_SIGN_REVERSAL
                                || data.result_type == PLOT_RESULT_SOLVE_EXTREMUM) {
                            const char *text;
                            int len;
                            switch (to_int(data.result_type)) {
                                case PLOT_RESULT_SOLVE_DIRECT: {
                                    vartype_string *s = (vartype_string *) stack[sp - 1];
                                    text = s->txt();
                                    len = s->length;
                                    break;
                                }
                                case PLOT_RESULT_SOLVE_SIGN_REVERSAL: {
                                    text = solve_message[SOLVE_SIGN_REVERSAL].text;
                                    len = solve_message[SOLVE_SIGN_REVERSAL].length;
                                    break;
                                }
                                case PLOT_RESULT_SOLVE_EXTREMUM: {
                                    text = solve_message[SOLVE_EXTREMUM].text;
                                    len = solve_message[SOLVE_EXTREMUM].length;
                                    break;
                                }
                            }
                            char2buf(buf, 100, &pos, ' ');
                            string2buf(buf, 100, &pos, text, len);
                        }
                    }
                    int w = small_string_width(buf, pos);
                    if (w > mode_plot_result_width)
                        mode_plot_result_width = w;
                    fill_rect(0, 0, mode_plot_result_width + 5, 6, 0);
                    mode_plot_result_width = w;
                    draw_small_string(0, -2, buf, pos, disp_w);
                }
            } else {
                err = do_it(&data);
                if (err == ERR_NONE && stop)
                    err = ERR_STOP;
                return err;
            }
    }

    free_vartype(mode_plot_inv);
    mode_plot_inv = NULL;
    err = docmd_rtn(NULL);

    if (result != NULL)
        recall_result(result);
    if (replot) {
        if (stop || err == ERR_STOP)
            set_running(false);
        return plot_helper(false);
    }

    if (state != PLOT_STATE_SCANNING && (!program_running() || stop || err == ERR_STOP))
        mode_plot_viewer = true;

    if (err == ERR_NONE && stop)
        err = ERR_STOP;
    return err;
}

static void make_tick_label(int mant, int exp, char **text, int *len) {
    char buf[32];
    int pos = 0;
    if (exp >= -3 && exp <= 2) {
        while (exp > 0) {
            mant *= 10;
            exp--;
        }
        pos += int2string(mant, buf + pos, 32 - pos);
        char *b = buf;
        if (mant < 0) {
            b++;
            pos--;
        }
        if (exp < 0) {
            exp = -exp;
            while (pos <= exp) {
                memmove(b + 1, b, pos++);
                b[0] = '0';
            }
            memmove(b + pos - exp + 1, b + pos - exp, exp);
            b[pos - exp] = flags.f.decimal_point ? '.' : ',';
            pos++;
        }
        if (mant < 0)
            pos++;
    } else {
        pos += int2string(mant, buf + pos, 32 - pos);
        if (mant != 0) {
            char2buf(buf, 32, &pos, 24);
            pos += int2string(exp, buf + pos, 32 - pos);
        }
    }
    *text = (char *) malloc(pos);
    if (*text != NULL) {
        memcpy(*text, buf, pos);
        *len = pos;
    } else {
        *len = 0;
    }
}

static void axis_ticks(bool hor, int c, phloat min, phloat max, int *t1pos, int *t2pos,
                        char **t1text, char **t2text, int *t1len, int *t2len) {
    phloat s1 = fabs(min);
    phloat s2 = fabs(max);
    int m1 = s1 == 0 ? INT_MIN : to_int(log10(s1));
    int m2 = s2 == 0 ? INT_MIN : to_int(log10(s2));
    int m = m1 > m2 ? m1 : m2;
    phloat scale = pow(10, m);
    int4 a1, a2;
    phloat min2 = min, max2 = max, p = (max - min) / ((hor ? disp_w : disp_h) - 1);
    min2 -= p * 0.49;
    max2 += p * 0.49;
    while (true) {
        a1 = to_int4(ceil(min2 / scale));
        a2 = to_int4(floor(max2 / scale));
        if (a2 > a1)
            break;
        scale /= 10;
        m--;
    }
    phloat x = a1 * scale;
    int first = -1, last;
    if (hor) {
        do {
            int r = to_int((x - min) / (max - min) * (disp_w - 1) + 0.5);
            draw_line(r, c - 1, r, c + 1);
            x += scale;
            if (first == -1)
                first = r;
            last = r;
        } while (x <= max);
    } else {
        do {
            int r = to_int((max - x) / (max - min) * (disp_h - 1) + 0.5);
            draw_line(c - 1, r, c + 1, r);
            x += scale;
            if (first == -1)
                first = r;
            last = r;
        } while (x <= max);
    }
    *t1pos = first;
    *t2pos = last;
    make_tick_label(a1, m, t1text, t1len);
    make_tick_label(a2, m, t2text, t2len);
}

static int plot_helper(bool reset) {
    PlotData data;
    if (data.err != ERR_NONE)
        return data.err;

    for (int i = 0; i < 2; i++) {
        if (data.axes[i].min == data.axes[i].max
                || p_isnan(data.axes[i].min) || p_isnan(data.axes[i].max)) {
            return ERR_INVALID_DATA;
        } else if (data.axes[i].min > data.axes[i].max) {
            phloat temp = data.axes[i].min;
            int index = i == 0 ? PLOT_X_MIN : PLOT_Y_MIN;
            data.set_phloat(index, data.axes[i].min = data.axes[i].max);
            data.set_phloat(index + 1, data.axes[i].max = temp);
        }
    }

    data.set_int(PLOT_STATE, data.state = PLOT_STATE_PLOTTING);
    data.set_int(PLOT_X_PIXEL, data.x_pixel = -1);
    data.set_phloat(PLOT_LAST_Y, data.last_y = NAN_PHLOAT);
    if (reset) {
        for (int i = 0; i < 4; i++)
            data.set_phloat(PLOT_MARK1_X + i, data.mark[i] = NAN_PHLOAT);
        data.set_int(PLOT_RESULT_TYPE, data.result_type = PLOT_RESULT_NONE);
    }

    int err = prepare_plot(&data);
    if (err != ERR_NONE)
        return err;

    clear_display();
    mode_message_lines = ALL_LINES;
    mode_plot_result_width = 0;

    int xo = data.conv_x(0);
    if (xo < 0)
        xo = 0;
    else if (xo > disp_w - 1)
        xo = disp_w - 1;
    int yo = data.conv_y(0);
    if (yo < 0)
        yo = 0;
    else if (yo > disp_h - 1)
        yo = disp_h - 1;
    draw_line(xo, 0, xo, disp_h - 1);
    draw_line(0, yo, disp_w - 1, yo);

    int xt1, xt2, yt1, yt2;
    char *xt1s, *xt2s, *yt1s, *yt2s;
    int xt1l, xt2l, yt1l, yt2l;
    int xt1w, xt2w, yt1w, yt2w;
    axis_ticks(true, yo, data.axes[0].min, data.axes[0].max, &xt1, &xt2, &xt1s, &xt2s, &xt1l, &xt2l);
    axis_ticks(false, xo, data.axes[1].min, data.axes[1].max, &yt1, &yt2, &yt1s, &yt2s, &yt1l, &yt2l);
    xt1w = small_string_width(xt1s, xt1l);
    xt2w = small_string_width(xt2s, xt2l);
    yt1w = small_string_width(yt1s, yt1l);
    yt2w = small_string_width(yt2s, yt2l);

    int xty = yo;
    if (xty + 8 > disp_h)
        xty -= 8;
    if (xt1 == xo) {
        if (xt1w + xo + 2 > disp_w)
            xt1 = xo - xt1w - 2;
        else
            xt1 = xo + 2;
    } else {
        xt1 -= xt1w / 2;
        if (xt1 < 0)
            xt1 = 0;
    }
    if (xt2 == xo) {
        if (xt2w + xo + 2 > disp_w)
            xt2 = xo - xt2w - 2;
        else
            xt2 = xo + 2;
    } else {
        xt2 -= xt2w / 2;
        if (xt2 + xt2w > disp_w)
            xt2 = disp_w - xt2w;
    }

    int yt1x, yt2x;
    int ytw = yt1w > yt2w ? yt1w : yt2w;
    if (ytw + xo + 2 > disp_w) {
        yt1x = xo - yt1w - 2;
        yt2x = xo - yt2w - 2;
    } else {
        yt1x = yt2x = xo + 2;
    }
    if (yt1 == yo) {
        if (yt1 + 8 > disp_h)
            yt1 -= 8;
    } else {
        yt1 -= 5;
        if (yt1 + 7 > disp_h)
            yt1 = disp_h - 7;
    }
    if (yt2 == yo) {
        if (yt2 + 8 > disp_h)
            yt2 -= 8;
    } else {
        yt2 -= 5;
        if (yt2 < -2)
            yt2 = -2;
    }

    draw_small_string(xt1, xty, xt1s, xt1l, disp_w - xt1);
    draw_small_string(xt2, xty, xt2s, xt2l, disp_w - xt2);
    draw_small_string(yt1x, yt1, yt1s, yt1l, disp_w - yt1x);
    draw_small_string(yt2x, yt2, yt2s, yt2l, disp_w - yt2x);

    free(xt1s);
    free(xt2s);
    free(yt1s);
    free(yt2s);

    return do_it(&data);
}

int docmd_plot(arg_struct *arg) {
    move_crosshairs(disp_w / 2, disp_h / 2, false);
    return plot_helper(true);
}

static bool run_plot(bool reset) {
    mode_plot_viewer = false;
    int err = plot_helper(reset);
    if (err != ERR_NONE && err != ERR_RUN) {
        display_error(err, false);
        flush_display();
        return false;
    }
    return err == ERR_RUN;
}

static void draw_coordinates() {
    PlotData data;
    if (data.err != ERR_NONE)
        return;
    int x, y;
    if (!get_crosshairs(&x, &y))
        return;
    phloat px = data.axes[0].min + ((phloat) x) / (disp_w - 1) * (data.axes[0].max - data.axes[0].min);
    phloat py = data.axes[1].max - ((phloat) y) / (disp_h - 1) * (data.axes[1].max - data.axes[1].min);
    char buf[100];
    int pos = 0;
    for (int i = 0; i < 2; i++) {
        if (data.axes[i].len == 0)
            string2buf(buf, 100, &pos, i == 0 ? "<X>" : "<Y>", 3);
        else
            string2buf(buf, 100, &pos, data.axes[i].name, data.axes[i].len);
        char2buf(buf, 100, &pos, '=');
        pos += phloat2string_four_digits(i == 0 ? px : py, buf + pos, 100 - pos);
        if (data.axes[i].unit->type == TYPE_UNIT) {
            vartype_unit *u = (vartype_unit *) data.axes[i].unit;
            char2buf(buf, 100, &pos, '_');
            string2buf(buf, 100, &pos, u->text, u->length);
        }
        if (i == 0)
            char2buf(buf, 100, &pos, ' ');
    }
    int w = small_string_width(buf, pos);
    if (w > mode_plot_result_width)
        mode_plot_result_width = w;
    fill_rect(0, 0, mode_plot_result_width + 5, 6, 0);
    mode_plot_result_width = w;
    draw_small_string(0, -2, buf, pos, disp_w);
}

static int plot_move(int key, bool repeating) {
    /* Returns: 0=stop, 1=repeat, 2=request cpu */
    int x, y, dx = 0, dy = 0;
    switch (key) {
        case KEY_7:
            dx = dy = -1;
            goto move;
        case KEY_8:
            dy = -1;
            goto move;
        case KEY_9:
            dx = 1;
            dy = -1;
            goto move;
        case KEY_4:
            dx = -1;
            goto move;
        case KEY_6:
            dx = 1;
            goto move;
        case KEY_1:
            dx = -1;
            dy = 1;
            goto move;
        case KEY_2:
            dy = 1;
            goto move;
        case KEY_3:
            dx = dy = 1;
            move:
            get_crosshairs(&x, &y);
            if (repeating) {
                dx *= 5;
                dy *= 5;
            }
            x += dx;
            y += dy;
            if (x >= 0 && x < disp_w && y >= 0 && y < disp_h) {
                move_crosshairs(x, y);
                draw_coordinates();
                flush_display();
                return 1;
            } else if (repeating) {
                if (x < 0)
                    x = 0;
                else if (x >= disp_w)
                    x = disp_w - 1;
                if (y < 0)
                    y = 0;
                else if (y >= disp_h)
                    y = disp_h - 1;
                move_crosshairs(x, y);
                draw_coordinates();
                flush_display();
                return 0;
            } else {
                PlotData data;
                if (data.err != ERR_NONE) {
                    squeak();
                    return 0;
                }
                if (x < 0 || x >= disp_w) {
                    phloat pw = (data.axes[0].max - data.axes[0].min) / 4;
                    if (x < 0) {
                        x += disp_w / 4;
                        pw = -pw;
                    } else {
                        x -= disp_w / 4;
                    }
                    data.set_phloat(PLOT_X_MIN, data.axes[0].min += pw);
                    data.set_phloat(PLOT_X_MAX, data.axes[0].max += pw);
                }
                if (y < 0 || y >= disp_h) {
                    phloat ph = (data.axes[1].max - data.axes[1].min) / 4;
                    if (y < 0) {
                        y += disp_h / 4;
                        ph = -ph;
                    } else {
                        y -= disp_h / 4;
                    }
                    data.set_phloat(PLOT_Y_MIN, data.axes[1].min -= ph);
                    data.set_phloat(PLOT_Y_MAX, data.axes[1].max -= ph);
                }
                move_crosshairs(x, y, false);
                return run_plot(true) ? 2 : 0;
            }
        default:
            /* should never get here */
            return 0;
    }
}

static bool plot_solve() {
    PlotData data;
    if (data.err != ERR_NONE || p_isnan(data.mark[0]) || p_isnan(data.mark[2])
            || (data.fun->type != TYPE_STRING && data.fun->type != TYPE_EQUATION)) {
        fail:
        squeak();
        return false;
    }

    if (data.fun->type == TYPE_STRING) {
        vartype_string *s = (vartype_string *) data.fun;
        set_solve_prgm(s->txt(), s->length);
    } else {
        set_solve_eqn(data.fun);
    }
    if (data.axes[1].len > 0) {
        // Set Y variable to zero, with the appropriate unit
        vartype *z = dup_vartype(data.axes[1].unit);
        if (z == NULL)
            goto fail;
        ((vartype_real *) z)->x = 0;
        if (store_var(data.axes[1].name, data.axes[1].len, z) != ERR_NONE) {
            free_vartype(z);
            goto fail;
        }
    }

    vartype_unit x1, x2;
    x1.type = x2.type = data.axes[0].unit->type;
    if (x1.type == TYPE_UNIT) {
        vartype_unit *u = (vartype_unit *) data.axes[0].unit;
        x1.text = x2.text = u->text;
        x1.length = x2.length = u->length;
    }
    x1.x = data.mark[0];
    x2.x = data.mark[2];

    clear_all_rtns();
    return_here_after_last_rtn();
    set_running(true);
    int err = push_func_state(0);
    if (err != ERR_NONE)
        goto fail;
    err = start_solve(-5, data.axes[0].name, data.axes[0].len, (vartype *) &x1, (vartype *) &x2);
    if (err == ERR_RUN || err == ERR_NONE) {
        mode_plot_viewer = false;
        data.set_int(PLOT_STATE, data.state = PLOT_STATE_SOLVE);
        return true;
    } else {
        set_running(false);
        squeak();
        return false;
    }
}

static bool plot_integ() {
    PlotData data;
    if (data.err != ERR_NONE || p_isnan(data.mark[0]) || p_isnan(data.mark[2])
            || (data.fun->type != TYPE_STRING && data.fun->type != TYPE_EQUATION)) {
        fail:
        squeak();
        return false;
    }
    phloat low = data.mark[0];
    phloat high = data.mark[2];
    if (low == high)
        goto fail;
    if (low > high) {
        phloat t = low;
        low = high;
        high = t;
    }
    if (!ensure_var_space(2))
        goto fail;
    vartype *llim, *ulim;
    if (data.axes[0].unit->type == TYPE_REAL) {
        llim = new_real(low);
        ulim = new_real(high);
    } else {
        vartype_unit *u = (vartype_unit *) data.axes[0].unit;
        llim = new_unit(low, u->text, u->length);
        ulim = new_unit(high, u->text, u->length);
    }
    if (llim == NULL || ulim == NULL) {
        free_vartype(llim);
        free_vartype(ulim);
        goto fail;
    }
    vartype *solve_info = NULL;
    if (data.axes[1].len > 0) {
        solve_info = new_list(4);
        if (solve_info == NULL)
            goto fail;
        vartype_list *list = (vartype_list *) solve_info;
        free_vartype(list->array->data[3]);
        list->array->data[3] = NULL; // this will be used for the inverse
        list->array->data[0] = new_string(data.axes[1].name, data.axes[1].len);
        if (data.axes[1].unit->type == TYPE_REAL) {
            list->array->data[1] = new_real(data.axes[1].min);
            list->array->data[2] = new_real(data.axes[1].max);
        } else {
            vartype_unit *u = (vartype_unit *) data.axes[1].unit;
            list->array->data[1] = new_unit(data.axes[1].min, u->text, u->length);
            list->array->data[2] = new_unit(data.axes[1].max, u->text, u->length);
        }
        for (int i = 0; i < 3; i++)
            if (list->array->data[i] == NULL) {
                free_vartype(solve_info);
                goto fail;
            }
        if (data.fun->type == TYPE_STRING) {
            vartype_string *s = (vartype_string *) data.fun;
            set_solve_prgm(s->txt(), s->length);
        } else {
            set_solve_eqn(data.fun);
        }
    }
    clear_all_rtns();
    return_here_after_last_rtn();
    set_running(true);
    int err = push_func_state(0);
    if (err != ERR_NONE)
        goto fail;
    store_var("LLIM", 4, llim, true);
    store_var("ULIM", 4, ulim, true);
    if (data.fun->type == TYPE_STRING) {
        vartype_string *s = (vartype_string *) data.fun;
        set_integ_prgm(s->txt(), s->length);
    } else {
        set_integ_eqn(data.fun);
    }
    err = start_integ(-5, data.axes[0].name, data.axes[0].len, solve_info);
    if (err == ERR_RUN || err == ERR_NONE) {
        mode_plot_viewer = false;
        data.set_int(PLOT_STATE, data.state = PLOT_STATE_INTEG);
        return true;
    } else {
        set_running(false);
        squeak();
        return false;
    }
}

static bool plot_zoom(bool in) {
    PlotData data;
    if (data.err != ERR_NONE) {
        squeak();
        return false;
    }
    phloat dw = (data.axes[0].max - data.axes[0].min) / (in ? 4 : -2);
    data.set_phloat(PLOT_X_MIN, data.axes[0].min += dw);
    data.set_phloat(PLOT_X_MAX, data.axes[0].max -= dw);
    phloat dh = (data.axes[1].max - data.axes[1].min) / (in ? 4 : -2);
    data.set_phloat(PLOT_Y_MIN, data.axes[1].min += dh);
    data.set_phloat(PLOT_Y_MAX, data.axes[1].max -= dh);
    return run_plot(true);
}

bool plot_keydown(int key, int *repeat) {
    if (key == KEY_SHIFT) {
        set_shift(!mode_shift);
        return false;
    }
    if (key == 0)
        return false;
    int shift = mode_shift;
    set_shift(false);

    switch (key) {
        case KEY_7:
        case KEY_8:
        case KEY_9:
        case KEY_4:
        case KEY_6:
        case KEY_1:
        case KEY_2:
        case KEY_3: {
            if (shift) {
                if (key == KEY_7)
                    return plot_solve();
                else if (key == KEY_8)
                    return plot_integ();
                squeak();
                return false;
            }
            int what = plot_move(key, false);
            if (what == 1) {
                mode_plot_key = key;
                *repeat = 2;
            }
            return what == 2;
        }
        case KEY_EXIT:
            if (shift) {
                docmd_off(NULL);
                return false;
            }
        case KEY_BSP:
            mode_plot_viewer = false;
            clear_message();
            redisplay();
            return false;
        case KEY_ENTER: {
            PlotData data;
            if (data.err != ERR_NONE) {
                mark_fail:
                squeak();
                return false;
            }
            int x, y;
            if (!get_crosshairs(&x, &y))
                goto mark_fail;
            phloat xx = data.axes[0].min + (data.axes[0].max - data.axes[0].min) * ((phloat) x) / (disp_w - 1);
            phloat yy = data.axes[1].max - (data.axes[1].max - data.axes[1].min) * ((phloat) y) / (disp_h - 1);
            if (p_isnan(data.mark[0])) {
                data.set_int(PLOT_STATE, data.state = PLOT_STATE_EVAL_MARK1);
                data.set_phloat(PLOT_MARK1_X, data.mark[0] = xx);
                data.set_phloat(PLOT_MARK1_Y, data.mark[1] = yy);
            } else {
                if (!p_isnan(data.mark[3])) {
                    data.set_phloat(PLOT_MARK1_X, data.mark[0] = data.mark[2]);
                    data.set_phloat(PLOT_MARK1_Y, data.mark[1] = data.mark[3]);
                }
                data.set_int(PLOT_STATE, data.state = PLOT_STATE_EVAL_MARK2);
                data.set_phloat(PLOT_MARK2_X, data.mark[2] = xx);
                data.set_phloat(PLOT_MARK2_Y, data.mark[3] = yy);
            }
            int err = prepare_plot(&data);
            if (err != ERR_NONE)
                goto mark_fail;
            if (call_plot_function(&data, xx) != ERR_RUN)
                goto mark_fail;
            mode_plot_viewer = false;
            return true;
        }
        case KEY_SUB:
            if (shift) {
                if (!flags.f.printer_exists) {
                    squeak();
                    return false;
                }
                print_text(NULL, 0, true);
                PlotData data;
                if (data.err == ERR_NONE && data.fun != NULL
                        && (data.fun->type == TYPE_STRING || data.fun->type == TYPE_EQUATION)) {
                    char *text;
                    int len;
                    char d;
                    if (data.fun->type == TYPE_STRING) {
                        vartype_string *s = (vartype_string *) data.fun;
                        text = s->txt();
                        len = s->length;
                        d = '"';
                    } else {
                        vartype_equation *eq = (vartype_equation *) data.fun;
                        text = eq->data->text;
                        len = eq->data->length;
                        d = eq->data->compatMode ? '`' : '\'';
                    }
                    char buf[24];
                    int pos = 0;
                    char2buf(buf, 24, &pos, d);
                    string2buf(buf, 24, &pos, text, len);
                    char2buf(buf, 24, &pos, d);
                    print_text(buf, pos, true);
                    print_text(NULL, 0, true);
                }

                docmd_prlcd(NULL);
                if (data.err == ERR_NONE) {
                    print_text(NULL, 0, true);
                    char buf[24];
                    for (int i = 0; i < 2; i++) {
                        int pos = 0;
                        char2buf(buf, 24, &pos, 'X' + i);
                        string2buf(buf, 24, &pos, " AXIS: ", 7);
                        if (data.axes[i].len == 0)
                            string2buf(buf, 24, &pos, "<STK>", 5);
                        else
                            string2buf(buf, 24, &pos, data.axes[i].name, data.axes[i].len);
                        print_text(buf, pos, true);
                        for (int j = 0; j < 2; j++) {
                            pos = easy_phloat2string(j == 0 ? data.axes[i].min : data.axes[i].max, buf, 24, 0);
                            if (data.axes[i].unit->type == TYPE_UNIT) {
                                char2buf(buf, 24, &pos, '_');
                                vartype_unit *u = (vartype_unit *) data.axes[i].unit;
                                string2buf(buf, 24, &pos, u->text, u->length);
                            }
                            print_wide(j == 0 ? " MIN=" : " MAX=", 5, buf, pos);
                        }
                    }
                }
            } else {
                return plot_zoom(false);
            }
            return false;
        case KEY_ADD:
            return plot_zoom(true);
        case KEY_0: {
            PlotData data;
            if (data.err != ERR_NONE || p_isnan(data.mark[0]) || p_isnan(data.mark[2])
                || data.mark[0] == data.mark[2] || data.mark[1] == data.mark[3]) {
                squeak();
                return false;
            } else {
                phloat xmin = data.mark[0];
                phloat xmax = data.mark[2];
                phloat ymin = data.mark[1];
                phloat ymax = data.mark[3];
                if (xmin > xmax) {
                    phloat t = xmin;
                    xmin = xmax;
                    xmax = t;
                }
                if (ymin > ymax) {
                    phloat t = ymin;
                    ymin = ymax;
                    ymax = t;
                }
                data.set_phloat(PLOT_X_MIN, data.axes[0].min = xmin);
                data.set_phloat(PLOT_X_MAX, data.axes[0].max = xmax);
                data.set_phloat(PLOT_Y_MIN, data.axes[1].min = ymin);
                data.set_phloat(PLOT_Y_MAX, data.axes[1].max = ymax);
                move_crosshairs(disp_w / 2, disp_h / 2, false);
                return run_plot(true);
            }
        }
        case KEY_5: {
            PlotData data;
            bool success = false;
            if (data.err == ERR_NONE) {
                int x, y;
                if (get_crosshairs(&x, &y)) {
                    x -= disp_w / 2;
                    y -= disp_h / 2;
                    phloat dx = x * (data.axes[0].max - data.axes[0].min) / (disp_w - 1);
                    phloat dy = y * (data.axes[1].max - data.axes[1].min) / (disp_h - 1);
                    data.set_phloat(PLOT_X_MIN, data.axes[0].min += dx);
                    data.set_phloat(PLOT_X_MAX, data.axes[0].max += dx);
                    data.set_phloat(PLOT_Y_MIN, data.axes[1].min -= dy);
                    data.set_phloat(PLOT_Y_MAX, data.axes[1].max -= dy);
                    success = true;
                }
            }
            if (success) {
                move_crosshairs(disp_w / 2, disp_h / 2, false);
                return run_plot(true);
            } else {
                squeak();
                return false;
            }
        }
        default:
            squeak();
            return false;
    }
}

int plot_repeat() {
    if (plot_move(mode_plot_key, true) == 1)
        return 2;
    else
        return 0;
}

int docmd_line(arg_struct *arg) {
    phloat x1, y1, x2, y2;
    if (stack[sp]->type == TYPE_COMPLEX && stack[sp - 1]->type == TYPE_COMPLEX) {
        x1 = ((vartype_complex *) stack[sp])->re;
        y1 = ((vartype_complex *) stack[sp])->im;
        x2 = ((vartype_complex *) stack[sp - 1])->re;
        y2 = ((vartype_complex *) stack[sp - 1])->im;
    } else {
        int s = sp < 3 ? sp : 3;
        for (int i = 0; i <= s; i++)
            if (stack[i]->type == TYPE_STRING)
                return ERR_ALPHA_DATA_IS_INVALID;
            else if (stack[i]->type != TYPE_REAL)
                return ERR_INVALID_TYPE;
        if (sp < 3)
            return ERR_TOO_FEW_ARGUMENTS;
        x1 = ((vartype_real *) stack[sp])->x;
        y1 = ((vartype_real *) stack[sp - 1])->x;
        x2 = ((vartype_real *) stack[sp - 2])->x;
        y2 = ((vartype_real *) stack[sp - 3])->x;
    }
    int xx1 = x1 < 0 ? -to_int(-x1 + 0.5) : to_int(x1 + 0.5);
    int yy1 = y1 < 0 ? -to_int(-y1 + 0.5) : to_int(y1 + 0.5);
    int xx2 = x2 < 0 ? -to_int(-x2 + 0.5) : to_int(x2 + 0.5);
    int yy2 = y2 < 0 ? -to_int(-y2 + 0.5) : to_int(y2 + 0.5);
    draw_line(xx1 - 1, yy1 - 1, xx2 - 1, yy2 - 1);
    flush_display();
    mode_message_lines = ALL_LINES;
    return ERR_NONE;
}
