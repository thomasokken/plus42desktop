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

#include <string.h>

#include "core_commands1.h"
#include "core_commands2.h"
#include "core_commands9.h"
#include "core_display.h"
#include "core_equations.h"
#include "core_globals.h"
#include "core_helpers.h"
#include "core_main.h"
#include "shell.h"

static int find_child(const char *name, int length) {
    for (int i = 0; i < cwd->children_count; i++)
        if (string_equals(name, length, cwd->children[i].name, cwd->children[i].length))
            return i;
    return -1;
}

int docmd_crdir(arg_struct *arg) {
    if (arg->type == ARGTYPE_IND_NUM
            || arg->type == ARGTYPE_IND_STK
            || arg->type == ARGTYPE_IND_STR) {
        int err = resolve_ind_arg(arg);
        if (err != ERR_NONE)
            return err;
    }
    if (arg->type != ARGTYPE_STR)
        return ERR_INVALID_TYPE;
    int pos = find_child(arg->val.text, arg->length);
    if (pos != -1)
        return ERR_NONE;

    if (cwd->children_count == cwd->children_capacity) {
        int nc = cwd->children_capacity + 10;
        subdir_struct *nd = (subdir_struct *) realloc(cwd->children, nc * sizeof(subdir_struct));
        if (nd == NULL)
            return ERR_INSUFFICIENT_MEMORY;
        cwd->children = nd;
        cwd->children_capacity = nc;
    }

    int id = get_dir_id();
    directory *d = new (std::nothrow) directory(id);
    if (d == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    d->parent = cwd;
    map_dir(id, d);
    pgm_index saved_prgm = current_prgm;
    directory *saved_cwd = cwd;
    current_prgm.set(d->id, 0);
    cwd = d;
    loading_state = true;
    goto_dot_dot(true);
    loading_state = false;
    current_prgm = saved_prgm;
    cwd = saved_cwd;

    memmove(cwd->children + 1, cwd->children, cwd->children_count * sizeof(subdir_struct));
    string_copy(cwd->children[0].name, &cwd->children[0].length, arg->val.text, arg->length);
    cwd->children[0].dir = d;
    cwd->children_count++;
    return ERR_NONE;
}

int docmd_pgdir(arg_struct *arg) {
    if (arg->type == ARGTYPE_IND_NUM
            || arg->type == ARGTYPE_IND_STK
            || arg->type == ARGTYPE_IND_STR) {
        int err = resolve_ind_arg(arg);
        if (err != ERR_NONE)
            return err;
    }
    if (arg->type != ARGTYPE_STR)
        return ERR_INVALID_TYPE;
    int pos = find_child(arg->val.text, arg->length);
    if (pos == -1)
        return ERR_NONE;
    /* Yep, this is brutal!
     * But the RPL machines do this, too.
     * rm -rf with no confirmation...
     */
    bool running_before = program_running();
    delete cwd->children[pos].dir;
    memmove(cwd->children + pos, cwd->children + pos + 1, (cwd->children_count - pos - 1) * sizeof(subdir_struct));
    cwd->children_count--;
    if (running_before && !program_running())
        return ERR_INTERRUPTED;
    else
        return ERR_NONE;
}

int docmd_chdir(arg_struct *arg) {
    if (arg->type == ARGTYPE_IND_NUM
            || arg->type == ARGTYPE_IND_STK
            || arg->type == ARGTYPE_IND_STR) {
        int err = resolve_ind_arg(arg);
        if (err != ERR_NONE)
            return err;
    }
    if (arg->type != ARGTYPE_STR)
        return ERR_INVALID_TYPE;
    int pos = find_child(arg->val.text, arg->length);
    if (pos == -1)
        return ERR_NONEXISTENT;
    cwd = cwd->children[pos].dir;
    return ERR_NONE;
}

int docmd_updir(arg_struct *arg) {
    if (cwd->parent != NULL)
        cwd = cwd->parent;
    return ERR_NONE;
}

int docmd_home(arg_struct *arg) {
    while (cwd->parent != NULL)
        cwd = cwd->parent;
    return ERR_NONE;
}

int docmd_path(arg_struct *arg) {
    try {
        std::string path;
        directory *dir = cwd;
        while (dir->parent != NULL) {
            directory *parent = dir->parent;
            for (int i = 0; i < parent->children_count; i++) {
                if (parent->children[i].dir == dir) {
                    path = std::string(":", 1) + std::string(parent->children[i].name, parent->children[i].length) + path;
                    goto found;
                }
            }
            return ERR_INTERNAL_ERROR;
            found:
            dir = parent;
        }
        path = std::string("HOME", 4) + path;
        vartype *v = new_string(path.c_str(), (int) path.length());
        if (v == NULL)
            return ERR_INSUFFICIENT_MEMORY;
        return recall_result(v);
    } catch (std::bad_alloc &) {
        return ERR_INSUFFICIENT_MEMORY;
    }
    /*
    directory *dir = cwd;
    int depth = 0;
    while (dir->parent != NULL) {
        depth++;
        dir = dir->parent;
    }
    vartype_list *list = (vartype_list *) new_list(depth);
    dir = cwd;
    while (dir->parent != NULL) {
        directory *p = dir->parent;
        int i = 0;
        while (p->children[i].dir != dir)
            i++;
        vartype *s = new_string(p->children[i].name, p->children[i].length);
        if (s == NULL) {
            free_vartype((vartype *) list);
            return ERR_INSUFFICIENT_MEMORY;
        }
        list->array->data[--depth] = s;
        dir = p;
    }
    return recall_result((vartype *) list);
    */
}

int docmd_rename(arg_struct *arg) {
    if (arg->type == ARGTYPE_IND_NUM
            || arg->type == ARGTYPE_IND_STK
            || arg->type == ARGTYPE_IND_STR) {
        int err = resolve_ind_arg(arg);
        if (err != ERR_NONE)
            return err;
    }
    if (arg->type != ARGTYPE_STR)
        return ERR_INVALID_TYPE;
    int pos = find_child(arg->val.text, arg->length);
    if (pos == -1)
        return ERR_NONEXISTENT;
    if (reg_alpha_length == 0)
        return ERR_RESTRICTED_OPERATION;
    if (reg_alpha_length > 7)
        return ERR_NAME_TOO_LONG;
    int pos2 = find_child(reg_alpha, reg_alpha_length);
    if (pos2 == pos)
        return ERR_NONE;
    if (pos2 != -1)
        return ERR_DIRECTORY_EXISTS;
    string_copy(cwd->children[pos].name, &cwd->children[pos].length, reg_alpha, reg_alpha_length);
    return ERR_NONE;
}

static bool can_move(directory *dir, directory *cwd) {
    return cwd == NULL ? true : cwd == dir ? false : can_move(dir, cwd->parent);
}

struct nvar_struct {
    int4 dir;
    unsigned char length;
    char name[7];
    vartype *value;
    bool is_dup;
};

static int ref_move_copy(bool copy) {
    vartype_list *list;
    vartype *x = stack[sp];
    bool list_arg;
    if (x->type == TYPE_LIST) {
        list = (vartype_list *) x;
        list_arg = true;
    } else if (x->type == TYPE_DIR_REF || x->type == TYPE_PGM_REF || x->type == TYPE_VAR_REF) {
        list = (vartype_list *) new_list(1);
        if (list == NULL)
            return ERR_INSUFFICIENT_MEMORY;
        list->array->data[0] = x;
        list_arg = false;
    } else
        return ERR_INVALID_TYPE;
    int err = ERR_INVALID_DATA;

    int dirs = 0, pgms = 0, vars = 0;
    int new_children_capacity = 0;
    int new_children_count = 0;
    subdir_struct *new_children = NULL;
    int new_prgms_capacity = 0;
    int new_prgms_count = 0;
    prgm_struct *new_prgms = NULL;
    int new_current_prgm = -1;
    int prgms_moved_before_current_prgm = 0;
    int new_vars_capacity = 0;
    int new_vars_count = 0;
    nvar_struct *new_vars = NULL;
    var_struct *real_new_vars = NULL;
    vartype *t_dup = NULL;

    if (false) {
        error:
        if (copy) {
            for (int i = 0; i < new_children_count; i++)
                if (new_children[i].length >= 16)
                    delete new_children[i].dir;
            for (int i = 0; i < new_prgms_count; i++)
                free(new_prgms[i].text);
            for (int i = 0; i < new_vars_count; i++)
                if (new_vars[i].is_dup)
                    free_vartype(new_vars[i].value);
        }
        free(new_children);
        free(new_prgms);
        free(new_vars);
        free(real_new_vars);
        if (!list_arg) {
            list->array->data[0] = NULL;
            free_vartype((vartype *) list);
        }
        free_vartype(t_dup);
        return err;
    }

    if (!flags.f.big_stack) {
        t_dup = dup_vartype(stack[REG_T]);
        if (t_dup == NULL) {
            err = ERR_INSUFFICIENT_MEMORY;
            goto error;
        }
    }

    // Count number of references of each type, and allocate arrays as needed

    for (int i = 0; i < list->size; i++)
        if (list->array->data[i]->type == TYPE_DIR_REF)
            dirs++;
        else if (list->array->data[i]->type == TYPE_PGM_REF)
            pgms++;
        else if (list->array->data[i]->type == TYPE_VAR_REF)
            vars++;
        else
            goto error;
    if (dirs > 0) {
        new_children_capacity = cwd->children_count + dirs + 10;
        new_children = (subdir_struct *) malloc(new_children_capacity * sizeof(subdir_struct));
        if (new_children == NULL) {
            err = ERR_INSUFFICIENT_MEMORY;
            goto error;
        }
    }
    if (pgms > 0) {
        new_prgms_capacity = cwd->prgms_count + pgms + 10;
        new_prgms = (prgm_struct *) malloc(new_prgms_capacity * sizeof(prgm_struct));
        if (new_prgms == NULL) {
            err = ERR_INSUFFICIENT_MEMORY;
            goto error;
        }
    }
    if (vars > 0) {
        new_vars_capacity = cwd->vars_count + vars + 10;
        new_vars = (nvar_struct *) malloc(new_vars_capacity * sizeof(nvar_struct));
        real_new_vars = (var_struct *) malloc(new_vars_capacity * sizeof(var_struct));
        if (new_vars == NULL || real_new_vars == NULL) {
            err = ERR_INSUFFICIENT_MEMORY;
            goto error;
        }
    }

    // Second pass through the list, to copy some preliminary data and check for errors

    for (int i = 0; i < list->size; i++) {
        directory *dir;
        vartype *r = list->array->data[i];
        if (r->type == TYPE_DIR_REF) {
            dir = get_dir(((vartype_dir_ref *) r)->dir);
            if (dir == NULL)
                goto error;
            if (!can_move(dir, cwd))
                goto error;
            for (int j = 0; j < new_children_count; j++)
                if (new_children[j].dir == dir)
                    goto skip;
            bool found = false;
            directory *parent = dir->parent;
            for (int j = 0; j < parent->children_count; j++)
                if (parent->children[j].dir == dir) {
                    string_copy(new_children[new_children_count].name,
                                &new_children[new_children_count].length,
                                parent->children[j].name,
                                parent->children[j].length);
                    found = true;
                    break;
                }
            if (!found)
                goto error;
            for (int j = 0; j < new_children_count; j++)
                if (string_equals(new_children[new_children_count].name,
                                  new_children[new_children_count].length,
                                  new_children[j].name,
                                  new_children[j].length & 15)) {
                    err = ERR_DIRECTORY_EXISTS;
                    goto error;
                }
            if (copy && parent != cwd) {
                directory *clone = dir->clone();
                if (clone == NULL) {
                    err = ERR_INSUFFICIENT_MEMORY;
                    goto error;
                }
                new_children[new_children_count].dir = clone;
                new_children[new_children_count].length += 16;
            } else
                new_children[new_children_count].dir = dir;
            new_children_count++;
        } else if (r->type == TYPE_PGM_REF) {
            vartype_pgm_ref *p = (vartype_pgm_ref *) r;
            directory *dir = get_dir(p->dir);
            if (dir == NULL || p->pgm >= dir->prgms_count)
                goto error;
            for (int j = 0; j < new_prgms_count; j++)
                if (new_prgms[j].capacity == p->dir && new_prgms[j].size == p->pgm)
                    goto skip;
            // Hack alert: using 'capacity' to hold the directory id,
            // and 'size' to hold the program index. Will populate
            // the structure properly in the second pass.
            new_prgms[new_prgms_count].capacity = p->dir;
            new_prgms[new_prgms_count].size = p->pgm;
            if (copy) {
                int newsize = dir->prgms[p->pgm].size;
                unsigned char *newtext = (unsigned char *) malloc(newsize);
                if (newtext == NULL && newsize != 0) {
                    err = ERR_INSUFFICIENT_MEMORY;
                    goto error;
                }
                memcpy(newtext, dir->prgms[p->pgm].text, newsize);
                new_prgms[new_prgms_count].text = newtext;
            } else {
                new_prgms[new_prgms_count].text = NULL;
                if (p->dir == current_prgm.dir)
                    if (p->pgm == current_prgm.idx)
                        new_current_prgm = new_prgms_count;
                    else if (p->pgm < current_prgm.idx)
                        prgms_moved_before_current_prgm++;
            }
            new_prgms_count++;
        } else {
            vartype_var_ref *v = (vartype_var_ref *) r;
            directory *dir = get_dir(v->dir);
            if (dir == NULL)
                goto error;
            int pos = -1;
            for (int i = 0; i < dir->vars_count; i++)
                if (string_equals(dir->vars[i].name, dir->vars[i].length, v->name, v->length)) {
                    pos = i;
                    break;
                }
            if (pos == -1)
                goto error;
            for (int i = 0; i < new_vars_count; i++)
                if (string_equals(new_vars[i].name, new_vars[i].length, v->name, v->length)) {
                    // Same name... if they're from the same directory, it's just
                    // a redundant ref and we ignore it, but otherwise, it's a
                    // collision, and that's a fatal error.
                    if (new_vars[i].dir == v->dir)
                        goto skip;
                    else {
                        err = ERR_VARIABLE_EXISTS;
                        goto error;
                    }
                }
            // Also make sure that variables we're moving in from elsewhere
            // don't collide with the ones that are already here.
            if (v->dir != cwd->id)
                for (int i = 0; i < cwd->vars_count; i++)
                    if (string_equals(cwd->vars[i].name, cwd->vars[i].length, v->name, v->length)) {
                            err = ERR_VARIABLE_EXISTS;
                            goto error;
                        }
            new_vars[new_vars_count].dir = v->dir;
            string_copy(new_vars[new_vars_count].name, &new_vars[new_vars_count].length, v->name, v->length);
            if (copy && v->dir != cwd->id) {
                new_vars[new_vars_count].value = dup_vartype(dir->vars[pos].value);
                if (new_vars[new_vars_count].value == NULL) {
                    err = ERR_INSUFFICIENT_MEMORY;
                    goto error;
                }
                new_vars[new_vars_count].is_dup = true;
            } else {
                new_vars[new_vars_count].value = dir->vars[pos].value;
                new_vars[new_vars_count].is_dup = false;
            }
            new_vars_count++;
        }
        skip:;
    }

    // Once we get here, all the directories, programs, and variables
    // we've been asked to move exist, and all arrays have been sized
    // sufficiently large.

    if (dirs > 0) {

        // First, move the directories. Since no directories are created or
        // destroyed, and no directory IDs are changing, this is a fairly
        // simple operation, with no side effects.

        for (int i = 0; i < cwd->children_count; i++) {
            directory *dir = cwd->children[i].dir;
            for (int j = 0; j < new_children_count; j++)
                if (new_children[j].dir == dir)
                    goto skip2;
            for (int j = 0; j < new_children_count; j++)
                if (string_equals(cwd->children[i].name,
                                  cwd->children[i].length,
                                  new_children[j].name,
                                  new_children[j].length & 15)) {
                    err = ERR_DIRECTORY_EXISTS;
                    goto error;
                }
            string_copy(new_children[new_children_count].name,
                        &new_children[new_children_count].length,
                        cwd->children[i].name, cwd->children[i].length);
            new_children[new_children_count++].dir = dir;
            skip2:;
        }

        if (copy) {

            // No more risk of error aborts; time to remove the 'copied' flags

            for (int i = 0; i < new_children_count; i++) {
                new_children[i].dir->parent = cwd;
                new_children[i].length &= 15;
            }

        } else {

            // Remove all directories that were moved here from elsewhere from their parents

            for (int i = 0; i < new_children_count; i++) {
                directory *dir = new_children[i].dir;
                directory *parent = dir->parent;
                if (parent == cwd)
                    continue;
                bool found = false;
                for (int j = 0; j < parent->children_count; j++) {
                    if (parent->children[j].dir == dir) {
                        memmove(parent->children + j, parent->children + j + 1,
                                (parent->children_count - j - 1) * sizeof(subdir_struct));
                        parent->children_count--;
                        dir->parent = cwd;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    err = ERR_INTERNAL_ERROR;
                    goto error;
                }
            }
        }

        // ...and commit the changes...

        free(cwd->children);
        cwd->children = new_children;
        cwd->children_count = new_children_count;
        cwd->children_capacity = new_children_capacity;

        // Directories done!

    }

    if (pgms > 0) {

        // Next, programs...

        bool first = true;
        for (int i = cwd->prgms_count - 1; i >= 0; i--) {
            if (!copy) {
                for (int j = 0; j < new_prgms_count; j++)
                    if (new_prgms[j].capacity == cwd->id && new_prgms[j].size == i)
                        goto skip3;
            }
            if (first && cwd->prgms[i].is_end(0)) {
                // There is an empty program at the end of the current
                // directory, while we are moving or copying programs in:
                // remove the empty program, similarly to how we treat
                // such empty programs during import.
                free(cwd->prgms[i].text);
                goto skip3;
            }
            new_prgms[new_prgms_count].capacity = cwd->id;
            new_prgms[new_prgms_count].size = i;
            new_prgms[new_prgms_count].text = NULL;
            count_embed_references(cwd, new_prgms_count, true);
            new_prgms_count++;
            skip3:
            first = false;
        }
        for (int i = 0; i < new_prgms_count; i++) {
            directory *dir = get_dir(new_prgms[i].capacity);
            if (new_prgms[i].text != NULL) {
                unsigned char *newtext = new_prgms[i].text;
                int index = new_prgms[i].size;
                new_prgms[i] = dir->prgms[index];
                new_prgms[i].capacity = dir->prgms[index].size;
                new_prgms[i].text = newtext;
            } else {
                int index = new_prgms[i].size;
                new_prgms[i] = dir->prgms[index];
                if (!copy && dir != cwd)
                    dir->prgms[index].capacity = -1;
            }
        }

        if (!copy) {
            for (int i = 0; i < list->size; i++) {
                vartype *r = list->array->data[i];
                if (r->type != TYPE_PGM_REF)
                    continue;
                vartype_pgm_ref *p = (vartype_pgm_ref *) r;
                directory *dir = get_dir(p->dir);
                if (dir == cwd)
                    continue;
                int c = dir->prgms_count;
                dir->prgms_count = 0;
                for (int j = 0; j < c; j++)
                    if (dir->prgms[j].capacity != -1)
                        dir->prgms[dir->prgms_count++] = dir->prgms[j];
            }
            if (new_current_prgm != -1)
                current_prgm.set(cwd->id, new_prgms_count - new_current_prgm - 1);
            else
                current_prgm.idx -= prgms_moved_before_current_prgm;
        }

        for (int i = 0; i < new_prgms_count / 2; i++) {
            prgm_struct tmp = new_prgms[i];
            new_prgms[i] = new_prgms[new_prgms_count - i - 1];
            new_prgms[new_prgms_count - i - 1] = tmp;
        }

        cwd->prgms = new_prgms;
        cwd->prgms_count = new_prgms_count;
        cwd->prgms_capacity = new_prgms_capacity;
        if (!copy) {
            pgm_index saved_prgm = current_prgm;
            directory *saved_cwd = cwd;
            for (int i = 0; i < list->size; i++) {
                vartype *r = list->array->data[i];
                if (r->type == TYPE_DIR_REF)
                    continue;
                vartype_pgm_ref *p = (vartype_pgm_ref *) r;
                if (p->dir == saved_cwd->id)
                    continue;
                cwd = get_dir(p->dir);
                current_prgm.set(p->dir, 0);
                if (cwd->prgms_count == 0)
                    goto_dot_dot(true);
                rebuild_label_table();
            }
            cwd = saved_cwd;
            current_prgm = saved_prgm;
        }
        rebuild_label_table();
    }

    if (vars > 0) {

        // And finally, variables. First, remove the ones that were moved
        // here from elsewhere from their original directories, and populate
        // the 'real' new vars array.

        for (int i = 0; i < new_vars_count; i++) {
            directory *dir = get_dir(new_vars[i].dir);
            if (!copy && dir != cwd) {
                for (int j = 0; j < dir->vars_count; j++)
                    if (string_equals(new_vars[i].name, new_vars[i].length, dir->vars[j].name, dir->vars[j].length)) {
                        memmove(dir->vars + j, dir->vars + j + 1, (dir->vars_count - j - 1) * sizeof(var_struct));
                        dir->vars_count--;
                        break;
                    }
            }
            string_copy(real_new_vars[i].name, &real_new_vars[i].length,
                        new_vars[i].name, new_vars[i].length);
            real_new_vars[i].level = real_new_vars[i].flags = 0;
            real_new_vars[i].value = new_vars[i].value;
        }

        // Add the remaining variables from the current directory to the
        // 'real' new vars array as well.

        for (int i = cwd->vars_count - 1; i >= 0; i--) {
            for (int j = 0; j < new_vars_count; j++)
                if (new_vars[j].dir == cwd->id
                        && string_equals(new_vars[j].name, new_vars[j].length,
                                        cwd->vars[i].name, cwd->vars[i].length))
                    goto skip4;
            string_copy(real_new_vars[new_vars_count].name, &real_new_vars[new_vars_count].length,
                        cwd->vars[i].name, cwd->vars[i].length);
            real_new_vars[new_vars_count].value = cwd->vars[i].value;
            real_new_vars[new_vars_count].level = real_new_vars[new_vars_count].flags = 0;
            new_vars_count++;
            skip4:;
        }

        for (int i = 0; i < new_vars_count / 2; i++) {
            var_struct tmp = real_new_vars[i];
            real_new_vars[i] = real_new_vars[new_vars_count - i - 1];
            real_new_vars[new_vars_count - i - 1] = tmp;
        }

        free(cwd->vars);
        free(new_vars);
        cwd->vars = real_new_vars;
        cwd->vars_count = new_vars_count;
        cwd->vars_capacity = new_vars_capacity;

    }

    // Clean up, and done.

    if (!list_arg) {
        list->array->data[0] = NULL;
        free_vartype((vartype *) list);
    }
    free_vartype(lastx);
    lastx = stack[sp];
    if (flags.f.big_stack) {
        sp--;
    } else {
        stack[REG_X] = stack[REG_Y];
        stack[REG_Y] = stack[REG_Z];
        stack[REG_Z] = stack[REG_T];
        stack[REG_T] = t_dup;
    }
    return ERR_NONE;
}

int docmd_refmove(arg_struct *arg) {
    return ref_move_copy(false);
}

int docmd_refcopy(arg_struct *arg) {
    return ref_move_copy(true);
}

int docmd_reffind(arg_struct *arg) {
    switch (stack[sp]->type) {
        case TYPE_DIR_REF: {
            vartype_dir_ref *r = (vartype_dir_ref *) stack[sp];
            directory *dir = get_dir(r->dir);
            if (dir == NULL)
                return ERR_INVALID_DATA;
            cwd = dir;
            return ERR_NONE;
        }
        case TYPE_PGM_REF: {
            vartype_pgm_ref *r = (vartype_pgm_ref *) stack[sp];
            directory *dir = get_dir(r->dir);
            if (dir == NULL)
                return ERR_INVALID_DATA;
            if (r->pgm >= dir->prgms_count)
                return ERR_INVALID_DATA;
            cwd = dir;
            if (!program_running()) {
                current_prgm.set(r->dir, r->pgm);
                pc = 0;
                flags.f.prgm_mode = true;
            }
            return ERR_NONE;
        }
        case TYPE_VAR_REF: {
            vartype_var_ref *r = (vartype_var_ref *) stack[sp];
            directory *dir = get_dir(r->dir);
            if (dir == NULL)
                return ERR_INVALID_DATA;
            bool found = false;
            for (int i = 0; i < dir->vars_count; i++)
                if (string_equals(dir->vars[i].name, dir->vars[i].length, r->name, r->length)) {
                    found = true;
                    break;
                }
            if (!found)
                return ERR_INVALID_DATA;
            cwd = dir;
            if (!program_running()) {
                arg_struct arg2;
                arg2.type = ARGTYPE_STR;
                int len;
                string_copy(arg2.val.text, &len, r->name, r->length);
                arg2.length = len;
                docmd_view(&arg2);
            }
            return ERR_NONE;
        }
        default:
            return ERR_INVALID_TYPE;
    }
}

static directory *prall_dir;
static int prall_index;
static int prall_worker(bool interrupted);

int docmd_prall(arg_struct *arg) {
    if (!flags.f.printer_enable && program_running())
        return ERR_NONE;
    if (!flags.f.printer_exists)
        return ERR_PRINTING_IS_DISABLED;
    set_annunciators(-1, -1, 1, -1, -1, -1);
    print_text(NULL, 0, true);
    print_text("HOME: Dir", 9, true);
    prall_dir = root;
    prall_index = 0;
    mode_interruptible = prall_worker;
    mode_stoppable = true;
    return ERR_INTERRUPTIBLE;
}

static int prall_worker(bool interrupted) {
    if (interrupted) {
        set_annunciators(-1, -1, 0, -1, -1, -1);
        return ERR_STOP;
    }
    char buf[100];
    int p = 0;

    directory *d = prall_dir;
    while (d != NULL) {
        string2buf(buf, 100, &p, "  ", flags.f.double_wide_print ? 1 : 2);
        d = d->parent;
    }

    if (prall_index < prall_dir->children_count) {
        subdir_struct *sd = prall_dir->children + prall_index;
        string2buf(buf, 100, &p, sd->name, sd->length);
        string2buf(buf, 100, &p, ": Dir", 5);
        prall_dir = sd->dir;
        prall_index = -1;
    } else if (prall_index < prall_dir->children_count + prall_dir->labels_count) {
        label_struct *lbl = prall_dir->labels + prall_index - prall_dir->children_count;
        if (lbl->length > 0) {
            string2buf(buf, 100, &p, "LBL \"", 5);
            string2buf(buf, 100, &p, lbl->name, lbl->length);
            char2buf(buf, 100, &p, '"');
        } else {
            if (prall_index == prall_dir->children_count + prall_dir->labels_count - 1)
                string2buf(buf, 100, &p, ".END.", 5);
            else
                string2buf(buf, 100, &p, "END", 3);
            string2buf(buf, 100, &p, " (", 2);
            directory *saved_cwd = cwd;
            cwd = prall_dir;
            int size = core_program_size(lbl->prgm);
            cwd = saved_cwd;
            p += int2string(size, buf + p, 100 - p);
            char2buf(buf, 100, &p, ')');
        }
    } else if (prall_index < prall_dir->children_count + prall_dir->labels_count + prall_dir->vars_count) {
        var_struct *var = prall_dir->vars + prall_index - prall_dir->children_count - prall_dir->labels_count;
        string2buf(buf, 100, &p, var->name, var->length);
        string2buf(buf, 100, &p, ": ", 2);
        switch (var->value->type) {
            case TYPE_REAL: {
                string2buf(buf, 100, &p, "Real", 4);
                break;
            }
            case TYPE_COMPLEX: {
                string2buf(buf, 100, &p, "Cpx", 3);
                break;
            }
            case TYPE_REALMATRIX: {
                string2buf(buf, 100, &p, "Real(", 5);
                vartype_realmatrix *rm = (vartype_realmatrix *) var->value;
                p += int2string(rm->rows, buf + p, 100 - p);
                char2buf(buf, 100, &p, '\1');
                p += int2string(rm->columns, buf + p, 100 - p);
                char2buf(buf, 100, &p, ')');
                break;
            }
            case TYPE_COMPLEXMATRIX: {
                string2buf(buf, 100, &p, "Cpx(", 4);
                vartype_complexmatrix *cm = (vartype_complexmatrix *) var->value;
                p += int2string(cm->rows, buf + p, 100 - p);
                char2buf(buf, 100, &p, '\1');
                p += int2string(cm->columns, buf + p, 100 - p);
                char2buf(buf, 100, &p, ')');
                break;
            }
            case TYPE_STRING: {
                string2buf(buf, 100, &p, "Str", 3);
                break;
            }
            case TYPE_LIST: {
                string2buf(buf, 100, &p, "List(", 5);
                vartype_list *list = (vartype_list *) var->value;
                p += int2string(list->size, buf + p, 100 - p);
                char2buf(buf, 100, &p, ')');
                break;
            }
            case TYPE_EQUATION: {
                string2buf(buf, 100, &p, "Eqn", 3);
                break;
            }
            case TYPE_UNIT: {
                string2buf(buf, 100, &p, "Unit", 4);
                break;
            }
            case TYPE_DIR_REF:
            case TYPE_PGM_REF:
            case TYPE_VAR_REF: {
                string2buf(buf, 100, &p, "Ref", 3);
                break;
            }
        }
    } else {
        if (prall_dir->parent == NULL) {
            done:
            set_annunciators(-1, -1, 0, -1, -1, -1);
            return ERR_NONE;
        } else {
            for (int i = 0; i < prall_dir->parent->children_count; i++)
                if (prall_dir->parent->children[i].dir == prall_dir) {
                    prall_dir = prall_dir->parent;
                    prall_index = i + 1;
                    return ERR_INTERRUPTIBLE;
                }
            goto done;
        }
    }
    print_lines(buf, p, true);
    prall_index++;
    return ERR_INTERRUPTIBLE;
}

int docmd_width(arg_struct *arg) {
    vartype *v = new_real(disp_w);
    if (v == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    return recall_result(v);
}

int docmd_height(arg_struct *arg) {
    vartype *v = new_real(disp_h);
    if (v == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    return recall_result(v);
}

int docmd_header(arg_struct *arg) {
    mode_header = !mode_header;
    return ERR_NONE;
}

static int row_change(int offset) {
    requested_disp_r += offset;
    if (requested_disp_r < 2) {
        draw_message(0, "2 Rows Is Minimum", 17);
        requested_disp_r = 2;
    } else if (requested_disp_r > 99) {
        draw_message(0, "99 Rows Is Maximum", 18);
        requested_disp_r = 99;
    } else {
        char buf[22];
        int n = int2string(requested_disp_r, buf, 22);
        string2buf(buf, 22, &n, " Rows Requested", 15);
        draw_message(0, buf, n);
        shell_request_display_size(requested_disp_r, requested_disp_c);
    }
    return ERR_NONE;
}

int docmd_row_plus(arg_struct *arg) {
    return row_change(1);
}

int docmd_row_minus(arg_struct *arg) {
    return row_change(-1);
}

static int col_change(int offset) {
    requested_disp_c += offset;
    if (requested_disp_c < 22) {
        draw_message(0, "22 Columns Is Minimum", 21);
        requested_disp_c = 22;
    } else if (requested_disp_c > 999) {
        draw_message(0, "999 Columns Is Maximum", 22);
        requested_disp_c = 999;
    } else {
        char buf[22];
        int n = int2string(requested_disp_c, buf, 22);
        string2buf(buf, 22, &n, " Columns Requested", 18);
        draw_message(0, buf, n);
        shell_request_display_size(requested_disp_r, requested_disp_c);
    }
    return ERR_NONE;
}

int docmd_col_plus(arg_struct *arg) {
    return col_change(1);
}

int docmd_col_minus(arg_struct *arg) {
    return col_change(-1);
}

int docmd_getds(arg_struct *arg) {
    vartype *r = new_real(disp_r);
    vartype *c = new_real(disp_c);
    if (r == NULL || c == NULL) {
        free_vartype(r);
        free_vartype(c);
        return ERR_INSUFFICIENT_MEMORY;
    }
    return recall_two_results(c, r);
}

int docmd_setds(arg_struct *arg) {
    phloat rr = ((vartype_real *) stack[sp - 1])->x;
    phloat cc = ((vartype_real *) stack[sp])->x;
    int r = to_int(rr);
    int c = to_int(cc);
    if (rr != r || cc != c || r < 2 || r > 99 || c < 22 || c > 999)
        return ERR_INVALID_DATA;
    requested_disp_r = r;
    requested_disp_c = c;
    if (!program_running()) {
        char buf[22];
        int n = int2string(requested_disp_r, buf, 22);
        string2buf(buf, 22, &n, " Rows ", 6);
        n += int2string(requested_disp_c, buf + n, 22 - n);
        string2buf(buf, 22, &n, " Cols Req", 9);
        draw_message(0, buf, n);
    }
    shell_request_display_size(requested_disp_r, requested_disp_c);
    return ERR_NONE;
}

int docmd_1line(arg_struct *arg) {
    mode_multi_line = false;
    return ERR_NONE;
}

int docmd_nline(arg_struct *arg) {
    mode_multi_line = true;
    return ERR_NONE;
}

int docmd_ltop(arg_struct *arg) {
    mode_lastx_top = !mode_lastx_top;
    return ERR_NONE;
}

int docmd_atop(arg_struct *arg) {
    mode_alpha_top = !mode_alpha_top;
    return ERR_NONE;
}

int docmd_hflags(arg_struct *arg) {
    mode_header_flags = !mode_header_flags;
    return ERR_NONE;
}

int docmd_hpolar(arg_struct *arg) {
    mode_header_polar = !mode_header_polar;
    return ERR_NONE;
}

int docmd_stk(arg_struct *arg) {
    mode_matedit_stk = !mode_matedit_stk;
    return ERR_NONE;
}

int docmd_dirs(arg_struct *arg) {
    set_menu(MENULEVEL_AUX, MENU_CATALOG);
    set_cat_section_no_top(CATSECT_DIRS);
    set_cat_row(0);
    return ERR_NONE;
}

int docmd_dir_fcn(arg_struct *arg) {
    set_plainmenu(MENU_DIR_FCN1, NULL, 0);
    return ERR_NONE;
}

int docmd_units(arg_struct *arg) {
    set_menu(MENULEVEL_AUX, MENU_CATALOG);
    set_cat_section_no_top(CATSECT_UNITS_1);
    set_cat_row(0);
    return ERR_NONE;
}

int docmd_unit_fcn(arg_struct *arg) {
    set_plainmenu(MENU_UNIT_FCN1, NULL, 0);
    return ERR_NONE;
}

static int spv(bool present) {
    phloat i = ((vartype_real *) stack[sp - 1])->x / 100;
    if (i <= -1)
        return ERR_INVALID_DATA;
    phloat n = ((vartype_real *) stack[sp])->x;
    if (present)
        n = -n;
    phloat r = exp(n * log1p(i));
    int inf;
    if ((inf = p_isinf(r)) != 0) {
        if (flags.f.range_error_ignore)
            r = inf < 0 ? NEG_HUGE_PHLOAT : POS_HUGE_PHLOAT;
        else
            return ERR_OUT_OF_RANGE;
    }
    vartype *v = new_real(r);
    if (v == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    else
        return binary_result(v);
}

int docmd_sppv(arg_struct *arg) {
    return spv(true);
}

int docmd_spfv(arg_struct *arg) {
    return spv(false);
}

static int usv(bool present) {
    phloat i = ((vartype_real *) stack[sp - 1])->x / 100;
    if (i <= -1)
        return ERR_INVALID_DATA;
    phloat n = ((vartype_real *) stack[sp])->x;
    phloat r;
    if (i == 0) {
        r = n;
    } else {
        if (present)
            n = -n;
        r = expm1(n * log1p(i)) / i;
        if (present)
            r = -r;
    }
    int inf;
    if ((inf = p_isinf(r)) != 0) {
        if (flags.f.range_error_ignore)
            r = inf < 0 ? NEG_HUGE_PHLOAT : POS_HUGE_PHLOAT;
        else
            return ERR_OUT_OF_RANGE;
    }
    vartype *v = new_real(r);
    if (v == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    else
        return binary_result(v);
}

int docmd_uspv(arg_struct *arg) {
    return usv(true);
}

int docmd_usfv(arg_struct *arg) {
    return usv(false);
}

static int tvm_arg_check(phloat p_yr, phloat mode, phloat *i, phloat *pmt) {
    if (p_yr == 0)
        return ERR_INVALID_DATA;
    if (mode != 0 && mode != 1)
        return ERR_INVALID_DATA;
    if (i != NULL)
        *i /= p_yr * 100;
    if (pmt != NULL && mode == 1)
        *pmt *= 1 + *i;
    return ERR_NONE;
}

static int tvm_result(phloat x) {
    // We know that levels 1-6 all contain vartype_real, and make use
    // of that to avoid memory allocations here...
    free_vartype(lastx);
    lastx = stack[sp];
    free_vartype(stack[sp - 1]);
    free_vartype(stack[sp - 2]);
    free_vartype(stack[sp - 3]);
    free_vartype(stack[sp - 4]);
    ((vartype_real *) stack[sp - 5])->x = x;
    sp -= 5;
    return ERR_NONE;
}

// pv + pmt * (-expm1(-n * log1p(i)) / i) + fv * exp(-n * log1p(i)) = 0
// pv + pmt * n + fv = 0 (i = 0)

static int do_n(phloat i, phloat pv, phloat pmt, phloat fv, phloat p_yr, phloat mode, phloat *res) {
    int err = tvm_arg_check(p_yr, mode, &i, &pmt);
    if (err != ERR_NONE)
        return err;
    phloat n;
    if (i == 0) {
        if (pmt == 0)
            return ERR_INVALID_DATA;
        n = -(fv + pv) / pmt;
    } else {
        n = -log1p(-(pv + fv) / (fv - (pmt / i))) / log1p(i);
    }
    if (p_isnan(n))
        return ERR_INVALID_DATA;
    int inf;
    if ((inf = p_isinf(n)) != 0) {
        if (flags.f.range_error_ignore)
            n = inf < 0 ? NEG_HUGE_PHLOAT : POS_HUGE_PHLOAT;
        else
            return ERR_OUT_OF_RANGE;
    }
    *res = n;
    return ERR_NONE;
}

static int do_i_pct_yr(phloat n, phloat pv, phloat pmt, phloat fv, phloat p_yr, phloat mode, phloat *res) {
    int err = tvm_arg_check(p_yr, mode, NULL, NULL);
    if (err != ERR_NONE)
        return err;
    if (n == 0)
        return ERR_INVALID_DATA;
    phloat i;
    if (pmt == 0) {
        if (pv == 0 || fv == 0
                || (pv > 0) == (fv > 0))
            return ERR_INVALID_DATA;
        i = expm1(log(-fv / pv) / n);
    } else {
        if (mode == 1) {
            pv += pmt;
            fv -= pmt;
        }
        if (pv == 0)
            if (fv == 0)
                return ERR_INVALID_DATA;
            else
                i = pmt / fv;
        else
            if (fv == 0)
                i = -pmt / pv;
            else {
                phloat a = pmt / fv;
                phloat b = -pmt / pv;
                i = fabs(b) > fabs(a) && a > -1 ? a : b;
            }
        if (p_isinf(i) || p_isnan(i) || i <= -1)
            i = 0;
        int c = 2;
        phloat f = 0;
        while (true) {
            phloat eps, f0 = f;
            if (1 + n * i * i == 1) {
                f = (pv + fv + n * pmt) / n;        // f(0)
                phloat a = f - pmt;                 // f(0) - pmt
                phloat b = (n * n - 1) * a / 6 * i; // f''(0)*i    
                phloat fp = (pv - fv + a) / 2 + b;  // f'(0) + f''(0)*i
                f = f + (fp - b / 2) * i;           // f(0) + f'(0)*i + f''(0)/2*i^2
                i += (eps = -f / fp);
            } else {
                phloat x = i / expm1(n * log1p(i));
                phloat k = (pv + fv) * x;
                phloat y = n * x - 1;
                f = k + pv * i + pmt;
                phloat num = y + (n - 1) * i;
                phloat den = i + i * i;
                x = f * den / (k * num - pv * den);
                i = i + x; // Newton's method
                eps = x;
            }
            if (c > 0) {
                c--;
                continue;
            }
            if (p_isnan(f))
                return ERR_NO_SOLUTION_FOUND;
            if (f == 0 || (f > 0) != (f0 > 0))
                break;
            if (fabs(f) >= fabs(f0)) {
                if (i + eps * 1e-6 == i) {
                    i -= eps / 2;
                    break;
                } else
                    return ERR_NO_SOLUTION_FOUND;
            }
        }
    }
    i *= p_yr * 100;
    int inf;
    if ((inf = p_isinf(i)) != 0) {
        if (flags.f.range_error_ignore)
            i = inf < 0 ? NEG_HUGE_PHLOAT : POS_HUGE_PHLOAT;
        else
            return ERR_OUT_OF_RANGE;
    }
    *res = i;
    return ERR_NONE;
}

static int do_pv(phloat n, phloat i, phloat pmt, phloat fv, phloat p_yr, phloat mode, phloat *res) {
    int err = tvm_arg_check(p_yr, mode, &i, &pmt);
    if (err != ERR_NONE)
        return err;
    phloat pv;
    if (i == 0) {
        pv = -(pmt * n + fv);
    } else {
        pv = -(pmt * (-expm1(-n * log1p(i)) / i) + fv * exp(-n * log1p(i)));
        int inf;
        if ((inf = p_isinf(pv)) != 0) {
            if (flags.f.range_error_ignore)
                pv = inf < 0 ? NEG_HUGE_PHLOAT : POS_HUGE_PHLOAT;
            else
                return ERR_OUT_OF_RANGE;
        }
    }
    *res = pv;
    return ERR_NONE;
}

static int do_pmt(phloat n, phloat i, phloat pv, phloat fv, phloat p_yr, phloat mode, phloat *res) {
    int err = tvm_arg_check(p_yr, mode, &i, NULL);
    if (err != ERR_NONE)
        return err;
    if (n == 0)
        return ERR_INVALID_DATA;
    phloat pmt;
    if (i == 0) {
        pmt = -(pv + fv) / n;
    } else {
        pmt = -((pv + fv) / expm1(n * log1p(i)) + pv) * i;
        int inf;
        if ((inf = p_isinf(pmt)) != 0) {
            if (flags.f.range_error_ignore)
                pmt = inf < 0 ? NEG_HUGE_PHLOAT : POS_HUGE_PHLOAT;
            else
                return ERR_OUT_OF_RANGE;
        }
        if (mode == 1)
            pmt /= 1 + i;
    }
    *res = pmt;
    return ERR_NONE;
}

static int do_fv(phloat n, phloat i, phloat pv, phloat pmt, phloat p_yr, phloat mode, phloat *res) {
    int err = tvm_arg_check(p_yr, mode, &i, &pmt);
    if (err != ERR_NONE)
        return err;
    phloat fv;
    if (i == 0) {
        fv = -pv - pmt * n;
    } else {
        fv = -(pv + pmt * (-expm1(-n * log1p(i)) / i)) / exp(-n * log1p(i)) ;
        int inf;
        if ((inf = p_isinf(fv)) != 0) {
            if (flags.f.range_error_ignore)
                fv = inf < 0 ? NEG_HUGE_PHLOAT : POS_HUGE_PHLOAT;
            else
                return ERR_OUT_OF_RANGE;
        }
    }
    *res = fv;
    return ERR_NONE;
}

int docmd_gen_n(arg_struct *arg) {
    phloat i = ((vartype_real *) stack[sp - 5])->x;
    phloat pv = ((vartype_real *) stack[sp - 4])->x;
    phloat pmt = ((vartype_real *) stack[sp - 3])->x;
    phloat fv = ((vartype_real *) stack[sp - 2])->x;
    phloat p_yr = ((vartype_real *) stack[sp - 1])->x;
    phloat mode = ((vartype_real *) stack[sp])->x;
    phloat n;
    int err = do_n(i, pv, pmt, fv, p_yr, mode, &n);
    if (err != ERR_NONE)
        return err;
    else
        return tvm_result(n);
}

int docmd_gen_i(arg_struct *arg) {
    phloat n = ((vartype_real *) stack[sp - 5])->x;
    phloat pv = ((vartype_real *) stack[sp - 4])->x;
    phloat pmt = ((vartype_real *) stack[sp - 3])->x;
    phloat fv = ((vartype_real *) stack[sp - 2])->x;
    phloat p_yr = ((vartype_real *) stack[sp - 1])->x;
    phloat mode = ((vartype_real *) stack[sp])->x;
    phloat i;
    int err = do_i_pct_yr(n, pv, pmt, fv, p_yr, mode, &i);
    if (err != ERR_NONE)
        return err;
    else
        return tvm_result(i);
}

int docmd_gen_pv(arg_struct *arg) {
    phloat n = ((vartype_real *) stack[sp - 5])->x;
    phloat i = ((vartype_real *) stack[sp - 4])->x;
    phloat pmt = ((vartype_real *) stack[sp - 3])->x;
    phloat fv = ((vartype_real *) stack[sp - 2])->x;
    phloat p_yr = ((vartype_real *) stack[sp - 1])->x;
    phloat mode = ((vartype_real *) stack[sp])->x;
    phloat pv;
    int err = do_pv(n, i, pmt, fv, p_yr, mode, &pv);
    if (err != ERR_NONE)
        return err;
    else
        return tvm_result(pv);
}

int docmd_gen_pmt(arg_struct *arg) {
    phloat n = ((vartype_real *) stack[sp - 5])->x;
    phloat i = ((vartype_real *) stack[sp - 4])->x;
    phloat pv = ((vartype_real *) stack[sp - 3])->x;
    phloat fv = ((vartype_real *) stack[sp - 2])->x;
    phloat p_yr = ((vartype_real *) stack[sp - 1])->x;
    phloat mode = ((vartype_real *) stack[sp])->x;
    phloat pmt;
    int err = do_pmt(n, i, pv, fv, p_yr, mode, &pmt);
    if (err != ERR_NONE)
        return err;
    else
        return tvm_result(pmt);
}

int docmd_gen_fv(arg_struct *arg) {
    phloat n = ((vartype_real *) stack[sp - 5])->x;
    phloat i = ((vartype_real *) stack[sp - 4])->x;
    phloat pv = ((vartype_real *) stack[sp - 3])->x;
    phloat pmt = ((vartype_real *) stack[sp - 2])->x;
    phloat p_yr = ((vartype_real *) stack[sp - 1])->x;
    phloat mode = ((vartype_real *) stack[sp])->x;
    phloat fv;
    int err = do_fv(n, i, pv, pmt, p_yr, mode, &fv);
    if (err != ERR_NONE)
        return err;
    else
        return tvm_result(fv);
}

const char *tvm_name[] = { "N", "I%YR", "PV", "PMT", "FV", "P/YR", "BEGIN" };
const unsigned char tvm_length[] = { 1, 4, 2, 3, 2, 4, 5 };

static void show_tvm_message() {
    if (!mode_header || disp_r < 4) {
        char buf[50];
        int pos = tvm_message(buf, 50);
        draw_message(0, buf, pos);
    }
}

int docmd_tvm(arg_struct *arg) {
    if (flags.f.prgm_mode) {
        set_plainmenu(MENU_TVM_PRGM1, NULL, 0);
        return ERR_NONE;
    }

    int alloc = 0;
    for (int i = 0; i < 7; i++) {
        if (recall_var(tvm_name[i], tvm_length[i]) == NULL) {
            vartype *v = new_real(i == 5 ? 12 : 0);
            if (v == NULL)
                goto fail;
            int err = store_var(tvm_name[i], tvm_length[i], v);
            if (err != ERR_NONE) {
                free_vartype(v);
                goto fail;
            }
            alloc |= 1 << i;
        }
    }
    if (false) {
        fail:
        for (int i = 0; i < 7; i++)
            if ((alloc & (1 << i)) != 0)
                purge_var(tvm_name[i], tvm_length[i]);
        return ERR_INSUFFICIENT_MEMORY;
    }

    show_tvm_message();

    set_menu(MENULEVEL_APP, MENU_TVM_APP1);
    return ERR_NONE;
}

int docmd_eqn(arg_struct *arg) {
    return eqn_start(CATSECT_TOP);
}

int docmd_eqn_fcn(arg_struct *arg) {
    set_plainmenu(MENU_EQN_FCN1, NULL, 0);
    return ERR_NONE;
}

static int check_tvm_params(vartype *a, vartype *b, vartype *c, vartype *d, vartype *e, vartype *f) {
    if (a == NULL
            || b == NULL
            || c == NULL
            || d == NULL
            || e == NULL
            || f == NULL)
        return ERR_NONEXISTENT;
    if (a->type != TYPE_REAL
            || b->type != TYPE_REAL
            || c->type != TYPE_REAL
            || d->type != TYPE_REAL
            || e->type != TYPE_REAL
            || f->type != TYPE_REAL)
        return ERR_INVALID_TYPE;
    return ERR_NONE;
}

static int tvm_rpn_result(const char *name, int length, phloat r) {
    vartype *v = new_real(r);
    if (v == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    int err = store_var(name, length, v);
    if (err != ERR_NONE) {
        free_vartype(v);
        return err;
    }
    v = new_real(r);
    if (v == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    err = recall_result_silently(v);
    if (err == ERR_NONE) {
        if (program_running()) {
            print_trace();
        } else {
            arg_struct arg;
            arg.type = ARGTYPE_STR;
            string_copy(arg.val.text, &arg.length, name, length);
            view_helper(&arg, flags.f.trace_print && flags.f.printer_exists);
        }
    }
    return err;
}

int docmd_n(arg_struct *arg) {
    mode_varmenu = true;
    vartype *i = recall_var("I%YR", 4);
    vartype *pv = recall_var("PV", 2);
    vartype *pmt = recall_var("PMT", 3);
    vartype *fv = recall_var("FV", 2);
    vartype *p_yr = recall_var("P/YR", 4);
    vartype *mode = recall_var("BEGIN", 5);
    int err = check_tvm_params(i, pv, pmt, fv, p_yr, mode);
    if (err != ERR_NONE)
        return err;
    phloat n;
    err = do_n(((vartype_real *) i)->x,
               ((vartype_real *) pv)->x,
               ((vartype_real *) pmt)->x,
               ((vartype_real *) fv)->x,
               ((vartype_real *) p_yr)->x,
               ((vartype_real *) mode)->x,
               &n);
    if (err != ERR_NONE)
        return err;
    return tvm_rpn_result("N", 1, n);
}

int docmd_i_pct_yr(arg_struct *arg) {
    mode_varmenu = true;
    vartype *n = recall_var("N", 1);
    vartype *pv = recall_var("PV", 2);
    vartype *pmt = recall_var("PMT", 3);
    vartype *fv = recall_var("FV", 2);
    vartype *p_yr = recall_var("P/YR", 4);
    vartype *mode = recall_var("BEGIN", 5);
    int err = check_tvm_params(n, pv, pmt, fv, p_yr, mode);
    if (err != ERR_NONE)
        return err;
    phloat i;
    err = do_i_pct_yr(((vartype_real *) n)->x,
                      ((vartype_real *) pv)->x,
                      ((vartype_real *) pmt)->x,
                      ((vartype_real *) fv)->x,
                      ((vartype_real *) p_yr)->x,
                      ((vartype_real *) mode)->x,
                      &i);
    if (err != ERR_NONE)
        return err;
    return tvm_rpn_result("I%YR", 4, i);
}

int docmd_pv(arg_struct *arg) {
    mode_varmenu = true;
    vartype *n = recall_var("N", 1);
    vartype *i = recall_var("I%YR", 4);
    vartype *pmt = recall_var("PMT", 3);
    vartype *fv = recall_var("FV", 2);
    vartype *p_yr = recall_var("P/YR", 4);
    vartype *mode = recall_var("BEGIN", 5);
    int err = check_tvm_params(n, i, pmt, fv, p_yr, mode);
    if (err != ERR_NONE)
        return err;
    phloat pv;
    err = do_pv(((vartype_real *) n)->x,
                ((vartype_real *) i)->x,
                ((vartype_real *) pmt)->x,
                ((vartype_real *) fv)->x,
                ((vartype_real *) p_yr)->x,
                ((vartype_real *) mode)->x,
                &pv);
    if (err != ERR_NONE)
        return err;
    return tvm_rpn_result("PV", 2, pv);
}

int docmd_pmt(arg_struct *arg) {
    mode_varmenu = true;
    vartype *n = recall_var("N", 1);
    vartype *i = recall_var("I%YR", 4);
    vartype *pv = recall_var("PV", 2);
    vartype *fv = recall_var("FV", 2);
    vartype *p_yr = recall_var("P/YR", 4);
    vartype *mode = recall_var("BEGIN", 5);
    int err = check_tvm_params(n, i, pv, fv, p_yr, mode);
    if (err != ERR_NONE)
        return err;
    phloat pmt;
    err = do_pmt(((vartype_real *) n)->x,
                 ((vartype_real *) i)->x,
                 ((vartype_real *) pv)->x,
                 ((vartype_real *) fv)->x,
                 ((vartype_real *) p_yr)->x,
                 ((vartype_real *) mode)->x,
                 &pmt);
    if (err != ERR_NONE)
        return err;
    return tvm_rpn_result("PMT", 3, pmt);
}

int docmd_fv(arg_struct *arg) {
    mode_varmenu = true;
    vartype *n = recall_var("N", 1);
    vartype *i = recall_var("I%YR", 4);
    vartype *pv = recall_var("PV", 2);
    vartype *pmt = recall_var("PMT", 3);
    vartype *p_yr = recall_var("P/YR", 4);
    vartype *mode = recall_var("BEGIN", 5);
    int err = check_tvm_params(n, i, pv, pmt, p_yr, mode);
    if (err != ERR_NONE)
        return err;
    phloat fv;
    err = do_fv(((vartype_real *) n)->x,
                ((vartype_real *) i)->x,
                ((vartype_real *) pv)->x,
                ((vartype_real *) pmt)->x,
                ((vartype_real *) p_yr)->x,
                ((vartype_real *) mode)->x,
                &fv);
    if (err != ERR_NONE)
        return err;
    return tvm_rpn_result("FV", 2, fv);
}

int docmd_p_per_yr(arg_struct *arg) {
    /* P/YR must be integer 1..999 */
    phloat x = ((vartype_real *) stack[sp])->x;
    int p = to_int(x);
    if (x != p || p < 1 || p > 999)
        return ERR_INVALID_DATA;
    vartype *v = new_real(x);
    if (v == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    int err = store_var("P/YR", 4, v);
    if (err == ERR_NONE)
        show_tvm_message();
    else
        free_vartype(v);
    return err;
}

static int begin_end_helper(int mode) {
    vartype *v = new_real(mode);
    if (v == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    int err = store_var("BEGIN", 5, v);
    if (err == ERR_NONE)
        show_tvm_message();
    else
        free_vartype(v);
    return err;
}

int docmd_tbegin(arg_struct *arg) {
    return begin_end_helper(1);
}

int docmd_tend(arg_struct *arg) {
    return begin_end_helper(0);
}

int docmd_tclear(arg_struct *arg) {
    for (int i = 0; i < 5; i++) {
        vartype *v = new_real(0);
        if (v == NULL)
            return ERR_INSUFFICIENT_MEMORY;
        int err = store_var(tvm_name[i], tvm_length[i], v);
        if (err != ERR_NONE) {
            free_vartype(v);
            return err;
        }
    }
    return ERR_NONE;
}

int docmd_treset(arg_struct *arg) {
    for (int i = 5; i < 7; i++) {
        vartype *v = new_real(i == 5 ? 12 : 0);
        if (v == NULL)
            return ERR_INSUFFICIENT_MEMORY;
        int err = store_var(tvm_name[i], tvm_length[i], v);
        if (err != ERR_NONE) {
            free_vartype(v);
            return err;
        }
    }
    show_tvm_message();
    return ERR_NONE;
}

#define AMORT_I 0
#define AMORT_BEGIN 1
#define AMORT_PMT 2
#define AMORT_NP 3
#define AMORT_FROM 4
#define AMORT_TO 5
#define AMORT_INT 6
#define AMORT_PRIN 7
#define AMORT_BAL 8
#define AMORT_TABLE_FIRST 9
#define AMORT_TABLE_LAST 10
#define AMORT_TABLE_INCR 11
#define AMORT_HEADER_I_YR 12
#define AMORT_HEADER_P_YR 13
#define AMORT_HEADER_PV 14
#define AMORT_SIZE 15

int appmenu_exitcallback_6(int menuid, bool exitall) {
    if (menuid == MENU_TVM_AMORT || menuid == MENU_TVM_TABLE)
        set_appmenu_exitcallback(6);
    else
        purge_var("AMORT", 5);
    mode_appmenu = menuid;
    return ERR_NONE;
}

static int init_amort() {
    vartype *i_yr = recall_var("I%YR", 4);
    vartype *p_yr = recall_var("P/YR", 4);
    vartype *beg = recall_var("BEGIN", 5);
    vartype *pv = recall_var("PV", 2);
    vartype *pmt = recall_var("PMT", 3);
    if (i_yr == NULL || p_yr == NULL || beg == NULL || pv == NULL || pmt == NULL)
        return ERR_NONEXISTENT;
    if (i_yr->type != TYPE_REAL
            || p_yr->type != TYPE_REAL
            || beg->type != TYPE_REAL
            || pv->type != TYPE_REAL
            || pmt->type != TYPE_REAL)
        return ERR_INVALID_TYPE;
    phloat i_yr_2, p_yr_2, pv_2, pmt_2;
    int err = round_easy(((vartype_real *) i_yr)->x, &i_yr_2);
    if (err != ERR_NONE)
        return err;
    err = round_easy(((vartype_real *) p_yr)->x, &p_yr_2);
    if (err != ERR_NONE)
        return err;
    err = round_easy(((vartype_real *) pv)->x, &pv_2);
    if (err != ERR_NONE)
        return err;
    err = round_easy(((vartype_real *) pmt)->x, &pmt_2);
    if (err != ERR_NONE)
        return err;
    if (p_yr_2 == 0)
        return ERR_INVALID_DATA;
    phloat i = i_yr_2 / p_yr_2;
    int inf = p_isinf(i);
    if (inf != 0) {
        if (flags.f.range_error_ignore)
            i = inf == 1 ? POS_HUGE_PHLOAT : NEG_HUGE_PHLOAT;
        else
            return ERR_OUT_OF_RANGE;
    }
    i /= 100;
    phloat beg_2 = ((vartype_real *) beg)->x;
    if (beg_2 != 0 && beg_2 != 1)
        return ERR_INVALID_DATA;

    vartype *v = new_realmatrix(AMORT_SIZE, 1);
    if (v == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    err = store_var("AMORT", 5, v);
    if (err != ERR_NONE) {
        free_vartype(v);
        return ERR_INSUFFICIENT_MEMORY;
    }
    vartype_realmatrix *rm = (vartype_realmatrix *) v;

    rm->array->data[AMORT_I] = i;         // interest per period
    rm->array->data[AMORT_BEGIN] = beg_2; // BEGIN mode
    rm->array->data[AMORT_PMT] = pmt_2;   // PMT (cached)
    rm->array->data[AMORT_NP] = 1;        // # payments per iteration
    rm->array->data[AMORT_FROM] = 0;      // last payment #
    rm->array->data[AMORT_TO] = 0;        // last payment #
    rm->array->data[AMORT_INT] = 0;       // INT
    rm->array->data[AMORT_PRIN] = 0;      // PRIN
    rm->array->data[AMORT_BAL] = pv_2;    // BAL
    rm->array->data[AMORT_TABLE_FIRST] = 1;
    rm->array->data[AMORT_TABLE_LAST] = 1;
    rm->array->data[AMORT_TABLE_INCR] = 1;
    rm->array->data[AMORT_HEADER_I_YR] = i_yr_2;
    rm->array->data[AMORT_HEADER_P_YR] = p_yr_2;
    rm->array->data[AMORT_HEADER_PV] = pv_2;

    return ERR_NONE;
}

static int get_amort(vartype_realmatrix **amrt, bool disent = false) {
    vartype *v = recall_var("AMORT", 5);
    if (v == NULL)
        return ERR_NONEXISTENT;
    if (v->type != TYPE_REALMATRIX)
        return ERR_INVALID_TYPE;
    if (disent && !disentangle(v))
        return ERR_INSUFFICIENT_MEMORY;
    vartype_realmatrix *rm = (vartype_realmatrix *) v;
    if (rm->rows * rm->columns < AMORT_SIZE)
        return ERR_INVALID_DATA;
    for (int i = 0; i < AMORT_SIZE; i++)
        if (rm->array->is_string[i])
            return ERR_ALPHA_DATA_IS_INVALID;
    int np = to_int(rm->array->data[AMORT_NP]);
    if (rm->array->data[AMORT_NP] != np || np < 1 || np > 1200)
        return ERR_INVALID_DATA;
    *amrt = rm;
    return ERR_NONE;
}

int docmd_amort(arg_struct *arg) {
    int err = init_amort();
    if (err != ERR_NONE) {
        nope:
        purge_var("AMORT", 5);
        return err;
    }
    err = set_menu_return_err(MENULEVEL_APP, MENU_TVM_AMORT, false);
    if (err != ERR_NONE)
        goto nope;
    set_appmenu_exitcallback(6);
    display_amort_status(0);
    return ERR_NONE;
}

int docmd_tnum_p(arg_struct *arg) {
    vartype_realmatrix *rm;
    int err = get_amort(&rm);
    if (err != ERR_NONE)
        return err;
    phloat x = ((vartype_real *) stack[sp])->x;
    int np = to_int(x);
    if (x != np || np < 1 || np > 1200)
        return ERR_INVALID_DATA;
    rm->array->data[AMORT_NP] = np;
    return docmd_tnext(arg);
}

struct amort_spec {
    const char *name;
    int length;
    int index;
};

const amort_spec amort_specs[] = {
    { "Interest=", 8, AMORT_INT },
    { "Principal=", 9, AMORT_PRIN },
    { "Balance=", 7, AMORT_BAL }
};

void display_amort_status(int key) {
    vartype_realmatrix *rm;
    int err = get_amort(&rm);
    if (err != ERR_NONE)
        return;

    int rows = disp_r - 1;
    char buf[50];
    int pos;
    int row = 0;

    if (rows > 1 || key == 0) {
        int np = to_int(rm->array->data[AMORT_NP]);
        pos = 0;
        string2buf(buf, 22, &pos, "#P=", 3);
        pos += int2string(np, buf + pos, 22 - pos);
        string2buf(buf, 22, &pos, " PMTS: ", 7);
        pos += int2string(to_int(rm->array->data[AMORT_FROM]), buf + pos, 22 - pos);
        char2buf(buf, 22, &pos, '-');
        pos += int2string(to_int(rm->array->data[AMORT_TO]), buf + pos, 22 - pos);
        draw_message(row++, buf, pos);
        rows--;
        if (rows == 0)
            return;
    }

    int seq = 0;
    if (key == 0) {
        if (rows == 1)
            seq = 3;
        else if (rows == 2)
            seq = 31;
        else
            seq = 321;
    } else {
        int s = mode_amort_seq;
        if (s == 0)
            s = 321;
        int p = 1;
        while (s != 0) {
            int d = s % 10;
            s /= 10;
            if (d != key) {
                seq += d * p;
                p *= 10;
            }
        }
        seq += key * p;
        mode_amort_seq = seq;
        if (rows == 1)
            seq /= 100;
        else if (rows == 2)
            seq /= 10;
    }

    while (seq != 0) {
        int key = seq % 10;
        seq /= 10;
        const amort_spec *as = amort_specs + key - 1;
        phloat val = rm->array->data[as->index];
        pos = easy_phloat2string(val, buf, 50, 0);
        char *mbuf = (char *) malloc(disp_c);
        if (mbuf == NULL)
            return;
        int n = 0;
        int length = as->length;
        if (pos + length + 1 > disp_c) {
            length = disp_c - pos - 1;
            if (length < 3)
                length = 3;
        }
        string2buf(mbuf, disp_c, &n, as->name, length);
        char2buf(mbuf, disp_c, &n, '=');
        string2buf(mbuf, disp_c, &n, buf, pos);
        draw_message(row++, mbuf, n);
        free(mbuf);
    }
}

static int amort_helper(int key) {
    vartype_realmatrix *rm;
    int err = get_amort(&rm);
    if (err != ERR_NONE)
        return err;
    int idx = amort_specs[key - 1].index;
    phloat val = rm->array->data[idx];
    vartype *v = new_real(val);
    if (v == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    err = recall_result(v);
    if (err == ERR_NONE)
        display_amort_status(key);
    return err;
}

int docmd_tint(arg_struct *arg) {
    return amort_helper(1);
}

int docmd_tprin(arg_struct *arg) {
    return amort_helper(2);
}

int docmd_tbal(arg_struct *arg) {
    return amort_helper(3);
}

static int amort_next(vartype_realmatrix *rm) {
    int np = to_int(rm->array->data[AMORT_NP]);
    bool no_int_first = rm->array->data[AMORT_TO] == 0
                     && rm->array->data[AMORT_BEGIN] == 1;
    phloat total_intr = 0;
    phloat total_prin = 0;
    phloat from = rm->array->data[AMORT_TO] + 1;
    phloat to = rm->array->data[AMORT_TO];
    for (int i = 0; i < np; i++) {
        phloat intr;
        if (no_int_first) {
            intr = 0;
            no_int_first = false;
        } else {
            intr = rm->array->data[AMORT_BAL] * rm->array->data[AMORT_I];
            int err = round_easy(intr, &intr);
            if (err != ERR_NONE)
                return err;
        }
        phloat prin = rm->array->data[AMORT_PMT] + intr;
        int inf = p_isinf(prin);
        if (inf != 0) {
            if (flags.f.range_error_ignore)
                prin = inf < 0 ? NEG_HUGE_PHLOAT : POS_HUGE_PHLOAT;
            else
                return ERR_OUT_OF_RANGE;
        }
        phloat new_bal = rm->array->data[AMORT_BAL] + prin;
        inf = p_isinf(new_bal);
        if (inf != 0) {
            if (flags.f.range_error_ignore)
                new_bal = inf < 0 ? NEG_HUGE_PHLOAT : POS_HUGE_PHLOAT;
            else
                return ERR_OUT_OF_RANGE;
        }
        total_intr -= intr;
        inf = p_isinf(total_intr);
        if (inf != 0) {
            if (flags.f.range_error_ignore)
                total_intr = inf < 0 ? NEG_HUGE_PHLOAT : POS_HUGE_PHLOAT;
            else
                return ERR_OUT_OF_RANGE;
        }
        total_prin += prin;
        inf = p_isinf(total_prin);
        if (inf != 0) {
            if (flags.f.range_error_ignore)
                total_prin = inf < 0 ? NEG_HUGE_PHLOAT : POS_HUGE_PHLOAT;
            else
                return ERR_OUT_OF_RANGE;
        }
        rm->array->data[AMORT_BAL] = new_bal;
        rm->array->data[AMORT_INT] = total_intr;
        rm->array->data[AMORT_PRIN] = total_prin;
        rm->array->data[AMORT_FROM] = from;
        rm->array->data[AMORT_TO] = ++to;
    }
    return ERR_NONE;
}

int docmd_tnext(arg_struct *arg) {
    vartype_realmatrix *rm;
    int err = get_amort(&rm);
    if (err != ERR_NONE)
        return err;
    err = amort_next(rm);
    vartype *v = new_real(rm->array->data[AMORT_BAL]);
    if (v == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    err = recall_result(v);
    if (err == ERR_NONE)
        display_amort_status(0);
    return err;
}

const amort_spec amort_table_specs[] = {
    { "First", 5, AMORT_TABLE_FIRST },
    { "Last", 4, AMORT_TABLE_LAST },
    { "Increment", 9, AMORT_TABLE_INCR }
};

void display_amort_table_param(int key) {
    vartype_realmatrix *rm;
    int err = get_amort(&rm);
    if (err != ERR_NONE)
        return;
    const amort_spec *as = amort_table_specs + key;
    phloat val = rm->array->data[as->index];
    char buf[50];
    int pos = easy_phloat2string(val, buf, 50, 0);
    char *mbuf = (char *) malloc(disp_c);
    if (mbuf == NULL)
        return;
    int n = 0;
    int length = as->length;
    if (pos + length + 1 > disp_c) {
        length = disp_c - pos - 1;
        if (length < 3)
            length = 3;
    }
    string2buf(mbuf, disp_c, &n, as->name, length);
    char2buf(mbuf, disp_c, &n, '=');
    string2buf(mbuf, disp_c, &n, buf, pos);
    draw_message(0, mbuf, n);
    free(mbuf);
}

static int amort_table_helper(int key) {
    vartype_realmatrix *rm;
    int err = get_amort(&rm);
    if (err != ERR_NONE)
        return err;
    int idx = amort_table_specs[key].index;
    phloat x = ((vartype_real *) stack[sp])->x;
    int i = to_int(x);
    if (x != i || i < 1)
        return ERR_INVALID_DATA;
    rm->array->data[idx] = x;
    display_amort_table_param(key);
    return ERR_NONE;
}

int docmd_tfirst(arg_struct *arg) {
    return amort_table_helper(0);
}

int docmd_tlast(arg_struct *arg) {
    return amort_table_helper(1);
}

int docmd_tincr(arg_struct *arg) {
    return amort_table_helper(2);
}

const amort_spec amort_table_header_specs[] = {
    { "I%YR=", 5, AMORT_HEADER_I_YR },
    { "PV=", 3, AMORT_HEADER_PV },
    { "PMT=", 4, AMORT_PMT },
    { "P/YR=", 5, AMORT_HEADER_P_YR }
};

static vartype_realmatrix *tgo_rm;

static int tgo_worker(bool interrupted) {
    int err = ERR_STOP;
    if (interrupted) {
        done:
        set_annunciators(-1, -1, 0, -1, -1, -1);
        free_vartype((vartype *) tgo_rm);
        return err;
    }

    int last = to_int(tgo_rm->array->data[AMORT_TABLE_LAST]);
    if (tgo_rm->array->data[AMORT_TO] >= last) {
        err = ERR_NONE;
        goto done;
    }

    int incr = to_int(tgo_rm->array->data[AMORT_TABLE_INCR]);
    int np = last - to_int(tgo_rm->array->data[AMORT_TO]);
    if (np > incr)
        np = incr;
    tgo_rm->array->data[AMORT_NP] = np;
    err = amort_next(tgo_rm);
    if (err != ERR_NONE)
        goto done;

    char buf[50];
    int pos = 0;
    string2buf(buf, 50, &pos, "PMTS:", 5);
    pos += int2string(to_int(tgo_rm->array->data[AMORT_FROM]), buf + pos, 50 - pos);
    char2buf(buf, 50, &pos, '-');
    pos += int2string(to_int(tgo_rm->array->data[AMORT_TO]), buf + pos, 50 - pos);
    print_text(NULL, 0, true);
    print_text(buf, pos, true);
    for (int i = 0; i < 3; i++) {
        const amort_spec *as = amort_specs + i;
        pos = easy_phloat2string(tgo_rm->array->data[as->index], buf, 50, 0);
        print_wide(as->name, as->length + 1, buf, pos);
    }

    return ERR_INTERRUPTIBLE;
}

int docmd_tgo(arg_struct *arg) {
    vartype_realmatrix *rm;
    int err = get_amort(&rm);
    if (err != ERR_NONE)
        return err;
    if (!flags.f.printer_enable && program_running())
        return ERR_NONE;
    if (!flags.f.printer_exists)
        return ERR_PRINTING_IS_DISABLED;

    set_annunciators(-1, -1, 1, -1, -1, -1);

    print_text(NULL, 0, true);
    char buf[50];
    for (int i = 0; i < 4; i++) {
        const amort_spec *as = amort_table_header_specs + i;
        phloat val = rm->array->data[as->index];
        int pos = easy_phloat2string(val, buf, 50, 0);
        print_wide(as->name, as->length, buf, pos);
    }
    if (rm->array->data[AMORT_BEGIN] == 1)
        print_text("Begin Mode", 10, true);
    else
        print_text("End Mode", 8, true);

    rm = (vartype_realmatrix *) dup_vartype((vartype *) rm);
    if (rm == NULL) {
        nomem:
        err = ERR_INSUFFICIENT_MEMORY;
        done:
        set_annunciators(-1, -1, 0, -1, -1, -1);
        free_vartype((vartype *) rm);
        return err;
    }
    if (!disentangle((vartype *) rm))
        goto nomem;

    rm->array->data[AMORT_BAL] = rm->array->data[AMORT_HEADER_PV];
    rm->array->data[AMORT_INT] = 0;
    rm->array->data[AMORT_PRIN] = 0;
    rm->array->data[AMORT_FROM] = 0;
    rm->array->data[AMORT_TO] = 0;

    int first = to_int(rm->array->data[AMORT_TABLE_FIRST]);
    int last = to_int(rm->array->data[AMORT_TABLE_LAST]);
    if (last < first) {
        err = ERR_NONE;
        goto done;
    }

    if (first > 1) {
        rm->array->data[AMORT_NP] = first - 1;
        err = amort_next(rm);
        if (err != ERR_NONE)
            goto done;
    }

    tgo_rm = rm;
    mode_interruptible = tgo_worker;
    mode_stoppable = true;
    return ERR_INTERRUPTIBLE;
}
