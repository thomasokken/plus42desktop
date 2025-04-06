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

#include <stdlib.h>
#include <string.h>

#include "core_globals.h"
#include "core_helpers.h"
#include "core_display.h"
#include "core_parser.h"
#include "core_variables.h"


equation_data::~equation_data() {
    free(text);
    delete ev;
    delete map;
}

bool pgm_index::is_editable() {
    return dir == cwd->id;
}

bool pgm_index::is_locked() {
    return dir_list[dir]->prgms[idx].locked;
}

// We cache vartype_real, vartype_complex, and vartype_string instances, to
// cut down on the malloc/free overhead.

#define POOLSIZE 10
static vartype_real *realpool[POOLSIZE];
static vartype_complex *complexpool[POOLSIZE];
static vartype_string *stringpool[POOLSIZE];
static int realpool_size = 0;
static int complexpool_size = 0;
static int stringpool_size = 0;

vartype *new_real(phloat value) {
    vartype_real *r;
    if (realpool_size > 0) {
        r = realpool[--realpool_size];
    } else {
        r = (vartype_real *) malloc(sizeof(vartype_real));
        if (r == NULL)
            return NULL;
        r->type = TYPE_REAL;
    }
    r->x = value;
    return (vartype *) r;
}

vartype *new_complex(phloat re, phloat im) {
    vartype_complex *c;
    if (complexpool_size > 0) {
        c = complexpool[--complexpool_size];
    } else {
        c = (vartype_complex *) malloc(sizeof(vartype_complex));
        if (c == NULL)
            return NULL;
        c->type = TYPE_COMPLEX;
    }
    c->re = re;
    c->im = im;
    return (vartype *) c;
}

vartype *new_string(const char *text, int length) {
    char *dbuf;
    if (length > SSLENV) {
        dbuf = (char *) malloc(length);
        if (dbuf == NULL)
            return NULL;
    }
    vartype_string *s;
    if (stringpool_size > 0) {
        s = stringpool[--stringpool_size];
    } else {
        s = (vartype_string *) malloc(sizeof(vartype_string));
        if (s == NULL) {
            if (length > SSLENV)
                free(dbuf);
            return NULL;
        }
        s->type = TYPE_STRING;
    }
    s->length = length;
    if (length > SSLENV)
        s->t.ptr = dbuf;
    if (text != NULL)
        memcpy(length > SSLENV ? s->t.ptr : s->t.buf, text, length);
    return (vartype *) s;
}

vartype *new_realmatrix(int4 rows, int4 columns) {
    double d_bytes = ((double) rows) * ((double) columns) * sizeof(phloat);
    if (((double) (int4) d_bytes) != d_bytes)
        return NULL;

    vartype_realmatrix *rm = (vartype_realmatrix *)
                                        malloc(sizeof(vartype_realmatrix));
    if (rm == NULL)
        return NULL;
    int4 i, sz;
    rm->type = TYPE_REALMATRIX;
    rm->rows = rows;
    rm->columns = columns;
    sz = rows * columns;
    rm->array = (realmatrix_data *) malloc(sizeof(realmatrix_data));
    if (rm->array == NULL) {
        free(rm);
        return NULL;
    }
    rm->array->data = (phloat *) malloc(sz * sizeof(phloat));
    if (rm->array->data == NULL) {
        free(rm->array);
        free(rm);
        return NULL;
    }
    rm->array->is_string = (char *) malloc(sz);
    if (rm->array->is_string == NULL) {
        free(rm->array->data);
        free(rm->array);
        free(rm);
        return NULL;
    }
    for (i = 0; i < sz; i++)
        rm->array->data[i] = 0;
    memset(rm->array->is_string, 0, sz);
    rm->array->refcount = 1;
    return (vartype *) rm;
}

vartype *new_complexmatrix(int4 rows, int4 columns) {
    double d_bytes = ((double) rows) * ((double) columns) * sizeof(phloat) * 2;
    if (((double) (int4) d_bytes) != d_bytes)
        return NULL;

    vartype_complexmatrix *cm = (vartype_complexmatrix *)
                                        malloc(sizeof(vartype_complexmatrix));
    if (cm == NULL)
        return NULL;
    int4 i, sz;
    cm->type = TYPE_COMPLEXMATRIX;
    cm->rows = rows;
    cm->columns = columns;
    sz = rows * columns * 2;
    cm->array = (complexmatrix_data *) malloc(sizeof(complexmatrix_data));
    if (cm->array == NULL) {
        free(cm);
        return NULL;
    }
    cm->array->data = (phloat *) malloc(sz * sizeof(phloat));
    if (cm->array->data == NULL) {
        free(cm->array);
        free(cm);
        return NULL;
    }
    for (i = 0; i < sz; i++)
        cm->array->data[i] = 0;
    cm->array->refcount = 1;
    return (vartype *) cm;
}

vartype *new_list(int4 size) {
    vartype_list *list = (vartype_list *) malloc(sizeof(vartype_list));
    if (list == NULL)
        return NULL;
    list->type = TYPE_LIST;
    list->size = size;
    list->array = (list_data *) malloc(sizeof(list_data));
    if (list->array == NULL) {
        free(list);
        return NULL;
    }
    list->array->data = (vartype **) malloc(size * sizeof(vartype *));
    if (list->array->data == NULL && size != 0) {
        free(list->array);
        free(list);
        return NULL;
    }
    memset(list->array->data, 0, size * sizeof(vartype *));
    list->array->refcount = 1;
    return (vartype *) list;
}

equation_data *new_equation_data(const char *text, int4 length, bool compat_mode, int *errpos, int eqn_index) {
    *errpos = -1;
    if (eqn_index == -1) {
        for (int i = 0; i < eq_dir->prgms_count; i++) {
            equation_data *eqd = eq_dir->prgms[i].eq_data;
            if (eqd == NULL)
                continue;
            if (!eqd->compatModeEmbedded && eqd->compatMode != compat_mode)
                continue;
            if (!string_equals(eqd->text, eqd->length, text, length))
                continue;
            return eqd;
        }
        eqn_index = new_eqn_idx();
        if (eqn_index == -1)
            return NULL;
    }
    equation_data *eqd = new (std::nothrow) equation_data;
    if (eqd == NULL)
        return NULL;
    eqd->length = length;
    eqd->text = (char *) malloc(length);
    if (eqd->text == NULL && length != 0) {
        delete eqd;
        return NULL;
    }
    memcpy(eqd->text, text, length);
    eqd->ev = Parser::parse(std::string(text, length), &compat_mode, &eqd->compatModeEmbedded, errpos);
    if (eqd->ev == NULL) {
        delete eqd;
        return NULL;
    }
    eqd->eqn_index = eqn_index;
    eqd->compatMode = compat_mode;

    CodeMap *map = new (std::nothrow) CodeMap;
    eq_dir->prgms[eqn_index].eq_data = eqd;
    Parser::generateCode(eqd->ev, eq_dir->prgms + eqn_index, map);
    if (map != NULL && map->getSize() == -1) {
        delete map;
        map = NULL;
    }
    eqd->map = map;
    if (eq_dir->prgms[eqn_index].text == NULL) {
        // Code generator failure
        eq_dir->prgms[eqn_index].eq_data = NULL;
        delete eqd;
        return NULL;
    }

    return eqd;
}

vartype *new_equation(const char *text, int4 len, bool compat_mode, int *errpos) {
    *errpos = -1;
    vartype_equation *eq = (vartype_equation *) malloc(sizeof(vartype_equation));
    if (eq == NULL)
        return NULL;
    equation_data *eqd = new_equation_data(text, len, compat_mode, errpos, -1);
    if (eqd == NULL) {
        free(eq);
        return NULL;
    } else {
        eq->type = TYPE_EQUATION;
        eq->data = eqd;
        eqd->refcount++;
        return (vartype *) eq;
    }
}

vartype *new_equation(equation_data *eqd) {
    vartype_equation *eq = (vartype_equation *) malloc(sizeof(vartype_equation));
    if (eq == NULL)
        return NULL;
    eq->type = TYPE_EQUATION;
    eq->data = eqd;
    eqd->refcount++;
    return (vartype *) eq;
}

vartype *new_unit(phloat value, const char *text, int4 length) {
    if (length == 0)
        return new_real(value);
    vartype_unit *u = (vartype_unit *) malloc(sizeof(vartype_unit));
    if (u == NULL)
        return NULL;
    u->text = (char *) malloc(length);
    if (u->text == NULL && length != 0) {
        free(u);
        return NULL;
    }
    u->type = TYPE_UNIT;
    u->x = value;
    memcpy(u->text, text, length);
    for (int i = 0; i < length; i++) {
        char c = text[i];
        if (c == '\0')
            c = '/';
        else if (c == '\1')
            c = '*';
        else if (c == '^')
            c = '\36';
        u->text[i] = c;
    }
    u->length = length;
    return (vartype *) u;
}

vartype *new_dir_ref(int4 dir) {
    vartype_dir_ref *r = (vartype_dir_ref *) malloc(sizeof(vartype_dir_ref));
    if (r == NULL)
        return NULL;
    r->type = TYPE_DIR_REF;
    r->dir = dir;
    return (vartype *) r;
}

vartype *new_pgm_ref(int4 dir, int4 pgm) {
    vartype_pgm_ref *r = (vartype_pgm_ref *) malloc(sizeof(vartype_pgm_ref));
    if (r == NULL)
        return NULL;
    r->type = TYPE_PGM_REF;
    r->dir = dir;
    r->pgm = pgm;
    return (vartype *) r;
}

vartype *new_var_ref(int4 dir, const char *name, int length) {
    if (length < 1 || length > 7)
        return NULL;
    vartype_var_ref *r = (vartype_var_ref *) malloc(sizeof(vartype_var_ref));
    if (r == NULL)
        return NULL;
    r->type = TYPE_VAR_REF;
    r->dir = dir;
    for (int i = 0; i < length; i++)
        r->name[i] = name[i];
    r->length = length;
    return (vartype *) r;
}

void free_vartype(vartype *v) {
    if (v == NULL)
        return;
    switch (v->type) {
        case TYPE_REAL: {
            if (realpool_size < POOLSIZE)
                realpool[realpool_size++] = (vartype_real *) v;
            else
                free(v);
            break;
        }
        case TYPE_COMPLEX: {
            if (complexpool_size < POOLSIZE)
                complexpool[complexpool_size++] = (vartype_complex *) v;
            else
                free(v);
            break;
        }
        case TYPE_STRING: {
            vartype_string *s = (vartype_string *) v;
            if (s->length > SSLENV)
                free(s->t.ptr);
            if (stringpool_size < POOLSIZE)
                stringpool[stringpool_size++] = s;
            else
                free(s);
            break;
        }
        case TYPE_REALMATRIX: {
            vartype_realmatrix *rm = (vartype_realmatrix *) v;
            if (--(rm->array->refcount) == 0) {
                int4 sz = rm->rows * rm->columns;
                free_long_strings(rm->array->is_string, rm->array->data, sz);
                free(rm->array->data);
                free(rm->array->is_string);
                free(rm->array);
            }
            free(rm);
            break;
        }
        case TYPE_COMPLEXMATRIX: {
            vartype_complexmatrix *cm = (vartype_complexmatrix *) v;
            if (--(cm->array->refcount) == 0) {
                free(cm->array->data);
                free(cm->array);
            }
            free(cm);
            break;
        }
        case TYPE_LIST: {
            vartype_list *list = (vartype_list *) v;
            if (--(list->array->refcount) == 0) {
                for (int4 i = 0; i < list->size; i++)
                    free_vartype(list->array->data[i]);
                free(list->array->data);
                free(list->array);
            }
            free(list);
            break;
        }
        case TYPE_EQUATION: {
            vartype_equation *eq = (vartype_equation *) v;
            remove_equation_reference(eq->data->eqn_index);
            free(eq);
            break;
        }
        case TYPE_UNIT: {
            vartype_unit *u = (vartype_unit *) v;
            free(u->text);
            free(u);
            break;
        }
        case TYPE_DIR_REF:
        case TYPE_PGM_REF:
        case TYPE_VAR_REF: {
            free(v);
            break;
        }
    }
}

void clean_vartype_pools() {
    while (realpool_size > 0)
        free(realpool[--realpool_size]);
    while (complexpool_size > 0)
        free(complexpool[--complexpool_size]);
    while (stringpool_size > 0)
        free(stringpool[--stringpool_size]);
}

void free_long_strings(char *is_string, phloat *data, int4 n) {
    for (int4 i = 0; i < n; i++)
        if (is_string[i] == 2)
            free(*(void **) &data[i]);
}

void get_matrix_string(vartype_realmatrix *rm, int i, char **text, int4 *length) {
    if (rm->array->is_string[i] == 1) {
        char *t = (char *) &rm->array->data[i];
        *text = t + 1;
        *length = *t;
    } else {
        int4 *p = *(int4 **) &rm->array->data[i];
        *text = (char *) (p + 1);
        *length = *p;
    }
}

void get_matrix_string(const vartype_realmatrix *rm, int i, const char **text, int4 *length) {
    get_matrix_string((vartype_realmatrix *) rm, i, (char **) text, length);
}

bool put_matrix_string(vartype_realmatrix *rm, int i, const char *text, int4 length) {
    char *ptext;
    int4 plength;
    if (rm->array->is_string[i] != 0) {
        get_matrix_string(rm, i, &ptext, &plength);
        if (plength == length) {
            memcpy(ptext, text, length);
            return true;
        }
    }
    if (length > SSLENM) {
        int4 *p = (int4 *) malloc(length + 4);
        if (p == NULL)
            return false;
        *p = length;
        memcpy(p + 1, text, length);
        if (rm->array->is_string[i] == 2)
            free(*(void **) &rm->array->data[i]);
        *(int4 **) &rm->array->data[i] = p;
        rm->array->is_string[i] = 2;
    } else {
        void *oldptr = rm->array->is_string[i] == 2 ? *(void **) &rm->array->data[i] : NULL;
        char *t = (char *) &rm->array->data[i];
        t[0] = length;
        memmove(t + 1, text, length);
        rm->array->is_string[i] = 1;
        if (oldptr != NULL)
            free(oldptr);
    }
    return true;
}

void put_matrix_phloat(vartype_realmatrix *rm, int i, phloat value) {
    if (rm->array->is_string[i] == 2)
        free(*(void **) &rm->array->data[i]);
    rm->array->is_string[i] = 0;
    rm->array->data[i] = value;
}

vartype *dup_vartype(const vartype *v) {
    if (v == NULL)
        return NULL;
    switch (v->type) {
        case TYPE_REAL: {
            vartype_real *r = (vartype_real *) v;
            return new_real(r->x);
        }
        case TYPE_COMPLEX: {
            vartype_complex *c = (vartype_complex *) v;
            return new_complex(c->re, c->im);
        }
        case TYPE_REALMATRIX: {
            vartype_realmatrix *rm = (vartype_realmatrix *) v;
            vartype_realmatrix *rm2 = (vartype_realmatrix *)
                                        malloc(sizeof(vartype_realmatrix));
            if (rm2 == NULL)
                return NULL;
            *rm2 = *rm;
            rm->array->refcount++;
            return (vartype *) rm2;
        }
        case TYPE_COMPLEXMATRIX: {
            vartype_complexmatrix *cm = (vartype_complexmatrix *) v;
            vartype_complexmatrix *cm2 = (vartype_complexmatrix *)
                                        malloc(sizeof(vartype_complexmatrix));
            if (cm2 == NULL)
                return NULL;
            *cm2 = *cm;
            cm->array->refcount++;
            return (vartype *) cm2;
        }
        case TYPE_STRING: {
            vartype_string *s = (vartype_string *) v;
            return new_string(s->txt(), s->length);
        }
        case TYPE_LIST: {
            vartype_list *list = (vartype_list *) v;
            vartype_list *list2 = (vartype_list *) malloc(sizeof(vartype_list));
            if (list2 == NULL)
                return NULL;
            *list2 = *list;
            list->array->refcount++;
            return (vartype *) list2;
        }
        case TYPE_EQUATION: {
            vartype_equation *eq = (vartype_equation *) v;
            vartype_equation *eq2 = (vartype_equation *) malloc(sizeof(vartype_equation));
            if (eq2 == NULL)
                return NULL;
            *eq2 = *eq;
            eq->data->refcount++;
            return (vartype *) eq2;
        }
        case TYPE_UNIT: {
            vartype_unit *u = (vartype_unit *) v;
            vartype_unit *u2 = (vartype_unit *) malloc(sizeof(vartype_unit));
            if (u2 == NULL)
                return NULL;
            *u2 = *u;
            u2->text = (char *) malloc(u->length);
            if (u2->text == NULL && u->length != 0) {
                free(u2);
                return NULL;
            }
            memcpy(u2->text, u->text, u->length);
            return (vartype *) u2;
        }
        case TYPE_DIR_REF: {
            vartype_dir_ref *r = (vartype_dir_ref *) v;
            vartype_dir_ref *r2 = (vartype_dir_ref *) malloc(sizeof(vartype_dir_ref));
            if (r2 == NULL)
                return NULL;
            *r2 = *r;
            return (vartype *) r2;
        }
        case TYPE_PGM_REF: {
            vartype_pgm_ref *r = (vartype_pgm_ref *) v;
            vartype_pgm_ref *r2 = (vartype_pgm_ref *) malloc(sizeof(vartype_pgm_ref));
            if (r2 == NULL)
                return NULL;
            *r2 = *r;
            return (vartype *) r2;
        }
        case TYPE_VAR_REF: {
            vartype_var_ref *r = (vartype_var_ref *) v;
            vartype_var_ref *r2 = (vartype_var_ref *) malloc(sizeof(vartype_var_ref));
            if (r2 == NULL)
                return NULL;
            *r2 = *r;
            return (vartype *) r2;
        }
        default:
            return NULL;
    }
}

bool disentangle(vartype *v) {
    switch (v->type) {
        case TYPE_REALMATRIX: {
            vartype_realmatrix *rm = (vartype_realmatrix *) v;
            if (rm->array->refcount == 1)
                return true;
            else {
                realmatrix_data *md = (realmatrix_data *)
                                        malloc(sizeof(realmatrix_data));
                if (md == NULL)
                    return false;
                int4 sz = rm->rows * rm->columns;
                int4 i;
                md->data = (phloat *) malloc(sz * sizeof(phloat));
                if (md->data == NULL) {
                    free(md);
                    return false;
                }
                md->is_string = (char *) malloc(sz);
                if (md->is_string == NULL) {
                    free(md->data);
                    free(md);
                    return false;
                }
                for (i = 0; i < sz; i++) {
                    md->is_string[i] = rm->array->is_string[i];
                    if (md->is_string[i] == 2) {
                        int4 *sp = *(int4 **) &rm->array->data[i];
                        int4 len = *sp + 4;
                        int4 *dp = (int4 *) malloc(len);
                        if (dp == NULL) {
                            free_long_strings(md->is_string, md->data, i);
                            free(md->is_string);
                            free(md->data);
                            free(md);
                            return false;
                        }
                        memcpy(dp, sp, len);
                        *(int4 **) &md->data[i] = dp;
                    } else {
                        md->data[i] = rm->array->data[i];
                    }
                }
                md->refcount = 1;
                rm->array->refcount--;
                rm->array = md;
                return true;
            }
        }
        case TYPE_COMPLEXMATRIX: {
            vartype_complexmatrix *cm = (vartype_complexmatrix *) v;
            if (cm->array->refcount == 1)
                return true;
            else {
                complexmatrix_data *md = (complexmatrix_data *)
                                            malloc(sizeof(complexmatrix_data));
                if (md == NULL)
                    return false;
                int4 sz = cm->rows * cm->columns * 2;
                int4 i;
                md->data = (phloat *) malloc(sz * sizeof(phloat));
                if (md->data == NULL) {
                    free(md);
                    return false;
                }
                for (i = 0; i < sz; i++)
                    md->data[i] = cm->array->data[i];
                md->refcount = 1;
                cm->array->refcount--;
                cm->array = md;
                return true;
            }
        }
        case TYPE_LIST: {
            vartype_list *list = (vartype_list *) v;
            if (list->array->refcount == 1)
                return true;
            else {
                list_data *ld = (list_data *) malloc(sizeof(list_data));
                if (ld == NULL)
                    return false;
                ld->data = (vartype **) malloc(list->size * sizeof(vartype *));
                if (ld->data == NULL && list->size != 0) {
                    free(ld);
                    return false;
                }
                for (int4 i = 0; i < list->size; i++) {
                    vartype *vv = list->array->data[i];
                    if (vv != NULL) {
                        vv = dup_vartype(vv);
                        if (vv == NULL) {
                            for (int4 j = 0; j < i; j++)
                                free_vartype(ld->data[j]);
                            free(ld->data);
                            free(ld);
                            return false;
                        }
                    }
                    ld->data[i] = vv;
                }
                ld->refcount = 1;
                list->array->refcount--;
                list->array = ld;
                return true;
            }
        }
        case TYPE_REAL:
        case TYPE_COMPLEX:
        case TYPE_STRING:
        case TYPE_EQUATION:
        default:
            return true;
    }
}

vartype *vloc::value() {
    if (idx == -1)
        return NULL;
    else if (dir <= 0)
        return local_vars[idx].value;
    else
        return dir_list[dir]->vars[idx].value;
}

void vloc::set_value(vartype *v) {
    if (dir <= 0)
        local_vars[idx].value = v;
    else
        dir_list[dir]->vars[idx].value = v;
}

bool vloc::writable() {
    return idx != -1 && (dir <= 0 || dir == cwd->id);
}

vloc lookup_var(const char *name, int namelength, bool no_locals, bool no_ancestors) {
    if (!no_locals) {
        for (int i = local_vars_count - 1; i >= 0; i--) {
            if ((local_vars[i].flags & VAR_PRIVATE) != 0)
                continue;
            if (local_vars[i].length == namelength) {
                for (int j = 0; j < namelength; j++)
                    if (local_vars[i].name[j] != name[j])
                        goto nomatch;
                return vloc(-local_vars[i].level, i);
            }
            nomatch:;
        }
    }
    directory *dir = cwd;
    do {
        for (int i = dir->vars_count - 1; i >= 0; i--) {
            if (dir->vars[i].length == namelength) {
                for (int j = 0; j < namelength; j++)
                    if (dir->vars[i].name[j] != name[j])
                        goto nomatch2;
                return vloc(dir->id, i);
            }
            nomatch2:;
        }
        if (no_ancestors)
            return vloc();
        dir = dir->parent;
    } while (dir != NULL);
    vartype_list *path = get_path();
    if (path == NULL)
        return vloc();
    for (int i = 0; i < path->size; i++) {
        vartype *v = path->array->data[i];
        if (v->type != TYPE_DIR_REF)
            continue;
        dir = get_dir(((vartype_dir_ref *) v)->dir);
        if (dir == NULL)
            continue;
        for (int j = dir->vars_count - 1; j >= 0; j--) {
            if (dir->vars[j].length == namelength) {
                for (int k = 0; k < namelength; k++)
                    if (dir->vars[j].name[k] != name[k])
                        goto nomatch3;
                return vloc(dir->id, j);
            }
            nomatch3:;
        }
    }
    return vloc();
}

vartype *recall_var(const char *name, int namelength, bool *writable) {
    vloc idx = lookup_var(name, namelength);
    if (writable != NULL)
        *writable = idx.writable();
    return idx.value();
}

vartype *recall_global_var(const char *name, int namelength, bool *writable) {
    vloc idx = lookup_var(name, namelength, true);
    if (writable != NULL)
        *writable = idx.writable();
    return idx.value();
}

equation_data *find_equation_data(const char *name, int namelength) {
    vartype *v = recall_var("EQNS", 4);
    if (v == NULL)
        return NULL;
    if (v->type != TYPE_LIST)
        return NULL;
    vartype_list *eqns = (vartype_list *) v;
    std::string s(name, namelength);
    for (int i = 0; i < eqns->size; i++) {
        v = eqns->array->data[i];
        if (v->type == TYPE_EQUATION) {
            vartype_equation *eq = (vartype_equation *) v;
            equation_data *eqd = eq->data;
            if (eqd->ev != NULL && eqd->ev->eqnName() == s)
                return eqd;
        }
    }
    return NULL;
}

std::vector<std::string> get_equation_names() {
    std::vector<std::string> res;
    vartype *v = recall_var("EQNS", 4);
    if (v == NULL)
        return res;
    if (v->type != TYPE_LIST)
        return res;
    vartype_list *eqns = (vartype_list *) v;
    for (int i = 0; i < eqns->size; i++) {
        v = eqns->array->data[i];
        if (v->type == TYPE_EQUATION) {
            vartype_equation *eq = (vartype_equation *) v;
            equation_data *eqd = eq->data;
            if (eqd->ev != NULL && eqd->ev->eqnName().length() > 0)
                res.push_back(eqd->ev->eqnName());
        }
    }
    return res;
}

std::vector<int> get_equation_indexes() {
    std::vector<int> res;
    vartype *v = recall_var("EQNS", 4);
    if (v == NULL)
        return res;
    if (v->type != TYPE_LIST)
        return res;
    vartype_list *eqns = (vartype_list *) v;
    for (int i = 0; i < eqns->size; i++) {
        v = eqns->array->data[i];
        if (v->type == TYPE_EQUATION) {
            vartype_equation *eq = (vartype_equation *) v;
            equation_data *eqd = eq->data;
            if (eqd->ev != NULL && eqd->ev->eqnName().length() > 0)
                res.push_back(i);
        }
    }
    return res;
}

static int store_params2(std::vector<std::string> *params, bool lenient) {
    if (stack[sp - 1]->type != TYPE_REAL)
        return ERR_INVALID_TYPE;
    phloat x = ((vartype_real *) stack[sp - 1])->x;
    if (x < 0 || x >= 2147483648.0)
        return ERR_INVALID_DATA;
    int4 n = to_int4(x);
    if (sp < n + 1)
        return ERR_STACK_DEPTH_ERROR;

    int nparams = params == NULL ? 0 : (int) params->size();
    if (n < nparams)
        return ERR_TOO_FEW_ARGUMENTS;
    bool stack_params = false;
    if (n > nparams) {
        if (nparams != 0 || !lenient)
            return ERR_TOO_MANY_ARGUMENTS;
        stack_params = true;
    }
    if (!ensure_var_space(nparams))
        return ERR_INSUFFICIENT_MEMORY;
    int vsp = sp - nparams - 1;

    if (flags.f.trace_print && flags.f.printer_exists) {
        char buf[24];
        int ptr = 0;
        string2buf(buf, 24, &ptr, "\17PAR: ", 6);
        ptr += int2string(n, buf + ptr, 24 - ptr);
        if (n > 0)
            string2buf(buf, 24, &ptr, stack_params ? " STK" : " VAR", 4);
        string2buf(buf, 24, &ptr, " PARAM", 6);
        if (n != 1)
            char2buf(buf, 24, &ptr, 'S');
        print_text(buf, ptr, true);
    }

    for (int i = 0; i < nparams; i++) {
        std::string p = (*params)[i];
        store_var(p.c_str(), (int) p.length(), stack[vsp++], true);
        if (flags.f.trace_print && flags.f.printer_exists)
            print_one_var(p.c_str(), (int) p.length());
    }
    free_vartype(stack[sp - 1]);
    free_vartype(lastx);
    lastx = stack[sp];
    sp -= nparams + 2;
    print_trace();
    return ERR_NONE;
}

int store_params() {
    if (!flags.f.big_stack)
        return ERR_INVALID_CONTEXT;
    if (stack[sp]->type == TYPE_STRING) {
        vartype_string *name = (vartype_string *) stack[sp];
        if (name->length > 7)
            return ERR_NAME_TOO_LONG;
        std::vector<std::string> params = get_mvars(name->txt(), name->length);
        return store_params2(&params, true);
    } else if (stack[sp]->type == TYPE_EQUATION) {
        vartype_equation *eq = (vartype_equation *) stack[sp];
        equation_data *eqd = eq->data;
        std::vector<std::string> *params = eqd->ev->eqnParamNames();
        return store_params2(params, false);
    } else {
        return ERR_INVALID_TYPE;
    }
}

bool ensure_var_space(int n) {
    int nc = cwd->vars_count + n;
    if (nc > cwd->vars_capacity) {
        var_struct *nv = (var_struct *) realloc(cwd->vars, nc * sizeof(var_struct));
        if (nv == NULL)
            return false;
        cwd->vars_capacity = nc;
        cwd->vars = nv;
    }
    return true;
}

int store_var(const char *name, int namelength, vartype *value, bool local /* = false */, bool global /* = false */) {
    vloc varindex = lookup_var(name, namelength, global, true);
    if (varindex.not_found()) {
        if (local)
            goto do_local;
        /* Create new global */
        if (cwd->vars_count == cwd->vars_capacity) {
            int nc = cwd->vars_capacity + 25;
            var_struct *nv = (var_struct *) realloc(cwd->vars, nc * sizeof(var_struct));
            if (nv == NULL)
                return ERR_INSUFFICIENT_MEMORY;
            cwd->vars_capacity = nc;
            cwd->vars = nv;
        }
        int idx = cwd->vars_count++;
        var_struct *gv = cwd->vars + idx;
        string_copy(gv->name, &gv->length, name, namelength);
        gv->value = value;
    } else if (local && varindex.level() < get_rtn_level()) {
        do_local:
        /* Create new local */
        if (local_vars_count == local_vars_capacity) {
            int nc = local_vars_capacity + 25;
            var_struct *nv = (var_struct *) realloc(local_vars, nc * sizeof(var_struct));
            if (nv == NULL)
                return ERR_INSUFFICIENT_MEMORY;
            local_vars_capacity = nc;
            local_vars = nv;
        }
        int idx = local_vars_count++;
        var_struct *lv = local_vars + idx;
        string_copy(lv->name, &lv->length, name, namelength);
        lv->level = get_rtn_level();
        lv->flags = 0;
        lv->value = value;
    } else {
        /* Update existing vaiable */
        if ((matedit_mode == 1 || matedit_mode == 3)
                && matedit_dir == varindex.dir
                && string_equals(name, namelength, matedit_name, matedit_length)) {
            if (matedit_mode == 1)
                matedit_i = matedit_j = 0;
            else
                return ERR_RESTRICTED_OPERATION;
        }
        free_vartype(varindex.value());
        varindex.set_value(value);
    }
    update_catalog();
    return ERR_NONE;
}

/* This function exists for storing REGS and EQNS. Different semantics in two
 * ways: it will update existing instances, even outside the current directory;
 * and if there is no existing instance, it will create a new one in the root
 * directory, not the current directory.
 */
int store_root_var(const char *name, int namelength, vartype *value) {
    vloc varindex = lookup_var(name, namelength);
    if (varindex.not_found()) {
        directory *saved_cwd = cwd;
        cwd = root;
        int err = store_var(name, namelength, value, false, true);
        cwd = saved_cwd;
        if (err == ERR_NONE)
            update_catalog();
        return err;
    } else {
        free_vartype(varindex.value());
        varindex.set_value(value);
        return ERR_NONE;
    }
}

bool purge_var(const char *name, int namelength, bool force, bool global, bool local) {
    vloc varindex = lookup_var(name, namelength, false, !force);
    if (varindex.not_found())
        return true;
    if (varindex.level() == -1) {
        if (!global)
            // Asked to delete a local, but found a global;
            // not an error, but don't delete.
            return true;
        if (varindex.dir != cwd->id && !force)
            // Won't delete global var not in the current dir
            return true;
    } else {
        if (!local)
            // Asked to delete a global, but found a local;
            // that's an error.
            return false;
        if (varindex.level() != get_rtn_level())
            // Found a local at a lower level;
            // not an error, but don't delete.
            return true;
    }
    if (matedit_mode == 3 && matedit_dir == varindex.dir
            && string_equals(matedit_name, matedit_length, name, namelength))
        // Deleting var in use by EDITN is not allowed
        return false;
    free_vartype(varindex.value());
    if (varindex.dir <= 0) {
        for (int i = varindex.idx; i < local_vars_count - 1; i++)
            local_vars[i] = local_vars[i + 1];
        local_vars_count--;
    } else {
        directory *dir = dir_list[varindex.dir];
        for (int i = varindex.idx; i < dir->vars_count - 1; i++)
            dir->vars[i] = dir->vars[i + 1];
        dir->vars_count--;
    }
    update_catalog();
    return true;
}

static bool var_exists(int section, int type) {
    if (section == CATSECT_VARS_ONLY)
        return true;
    switch (type) {
        case TYPE_REAL:
            return section == CATSECT_REAL || section == CATSECT_REAL_ONLY;
        case TYPE_STRING:
            return section == CATSECT_REAL || section == CATSECT_REAL_ONLY || section == CATSECT_LIST_STR_ONLY;
        case TYPE_COMPLEX:
            return section == CATSECT_CPX;
        case TYPE_REALMATRIX:
        case TYPE_COMPLEXMATRIX:
            return section == CATSECT_MAT || section == CATSECT_MAT_ONLY
                || section == CATSECT_MAT_LIST || section == CATSECT_MAT_LIST_ONLY;
        case TYPE_LIST:
            return section == CATSECT_LIST || section == CATSECT_LIST_ONLY
                || section == CATSECT_LIST_STR_ONLY
                || section == CATSECT_MAT_LIST || section == CATSECT_MAT_LIST_ONLY;
        case TYPE_EQUATION:
            return section == CATSECT_EQN || section == CATSECT_EQN_ONLY;
        case TYPE_UNIT:
        case TYPE_DIR_REF:
        case TYPE_PGM_REF:
        case TYPE_VAR_REF:
            return section == CATSECT_OTHER;
    }
    return false;
}

bool vars_exist(int section) {
    bool show_ancestors = show_nonlocal_vars(section);
    for (int i = 0; i < local_vars_count; i++)
        if ((local_vars[i].flags & (VAR_HIDDEN | VAR_PRIVATE)) == 0
                && var_exists(section, local_vars[i].value->type))
            return true;
    directory *dir = cwd;
    do {
        for (int i = 0; i < dir->vars_count; i++)
            if (var_exists(section, dir->vars[i].value->type))
                return true;
        if (!show_ancestors)
            return false;
        dir = dir->parent;
    } while (dir != NULL);
    vartype_list *path = get_path();
    if (path == NULL)
        return false;
    for (int i = 0; i < path->size; i++) {
        vartype *v = path->array->data[i];
        if (v->type != TYPE_DIR_REF)
            continue;
        dir = get_dir(((vartype_dir_ref *) v)->dir);
        if (dir == NULL)
            continue;
        for (int j = 0; j < dir->vars_count; j++)
            if (var_exists(section, dir->vars[i].value->type))
                return true;
    }
    return false;
}

bool named_eqns_exist() {
    vartype *v = recall_var("EQNS", 4);
    if (v == NULL)
        return false;
    if (v->type != TYPE_LIST)
        return false;
    vartype_list *eqns = (vartype_list *) v;
    for (int i = 0; i < eqns->size; i++) {
        v = eqns->array->data[i];
        if (v->type == TYPE_EQUATION) {
            vartype_equation *eq = (vartype_equation *) v;
            equation_data *eqd = eq->data;
            if (eqd->ev != NULL && eqd->ev->eqnName().length() > 0)
                return true;
        }
    }
    return false;
}

bool contains_strings(const vartype_realmatrix *rm) {
    int4 size = rm->rows * rm->columns;
    for (int4 i = 0; i < size; i++)
        if (rm->array->is_string[i] != 0)
            return true;
    return false;
}

/* This is only used by core_linalg1, and does not deal with strings,
 * even when copying a real matrix to a real matrix. It returns an
 * error if any are encountered.
 */
int matrix_copy(vartype *dst, const vartype *src) {
    if (src->type == TYPE_REALMATRIX) {
        vartype_realmatrix *s = (vartype_realmatrix *) src;
        if (dst->type == TYPE_REALMATRIX) {
            vartype_realmatrix *d = (vartype_realmatrix *) dst;
            if (s->rows != d->rows || s->columns != d->columns)
                return ERR_DIMENSION_ERROR;
            if (contains_strings(s))
                return ERR_ALPHA_DATA_IS_INVALID;
            int4 size = s->rows * s->columns;
            free_long_strings(d->array->is_string, d->array->data, size);
            memset(d->array->is_string, 0, size);
            memcpy((void *) d->array->data, (const void *) s->array->data, size * sizeof(phloat));
            return ERR_NONE;
        } else if (dst->type == TYPE_COMPLEXMATRIX) {
            vartype_complexmatrix *d = (vartype_complexmatrix *) dst;
            if (s->rows != d->rows || s->columns != d->columns)
                return ERR_DIMENSION_ERROR;
            if (contains_strings(s))
                return ERR_ALPHA_DATA_IS_INVALID;
            int4 size = s->rows * s->columns;
            for (int4 i = 0; i < size; i++) {
                d->array->data[2 * i] = s->array->data[i];
                d->array->data[2 * i + 1] = 0;
            }
            return ERR_NONE;
        } else
            return ERR_INVALID_TYPE;
    } else if (src->type == TYPE_COMPLEXMATRIX
                    && dst->type == TYPE_COMPLEXMATRIX) {
        vartype_complexmatrix *s = (vartype_complexmatrix *) src;
        vartype_complexmatrix *d = (vartype_complexmatrix *) dst;
        if (s->rows != d->rows || s->columns != d->columns)
            return ERR_DIMENSION_ERROR;
        int4 size = s->rows * s->columns * 2;
        memcpy((void *) d->array->data, (const void *) s->array->data, size * sizeof(phloat));
        return ERR_NONE;
    } else
        return ERR_INVALID_TYPE;
}

static vloc lookup_private_var(const char *name, int namelength, bool allow_calling_frames) {
    int level = get_rtn_level();
    int i, j;
    for (i = local_vars_count - 1; i >= 0; i--) {
        int vlevel = local_vars[i].level;
        if (vlevel == -1)
            continue;
        if (!allow_calling_frames && vlevel < level)
            break;
        if ((local_vars[i].flags & VAR_PRIVATE) == 0)
            continue;
        if (local_vars[i].length == namelength) {
            for (j = 0; j < namelength; j++)
                if (local_vars[i].name[j] != name[j])
                    goto nomatch;
            return vloc(-local_vars[i].level, i);
        }
        nomatch:;
    }
    return vloc();
}

vartype *recall_private_var(const char *name, int namelength, bool allow_calling_frames) {
    return lookup_private_var(name, namelength, allow_calling_frames).value();
}

vartype *recall_and_purge_private_var(const char *name, int namelength) {
    vloc varindex = lookup_private_var(name, namelength, false);
    if (varindex.not_found())
        return NULL;
    vartype *ret = varindex.value();
    for (int i = varindex.idx; i < local_vars_count - 1; i++)
        local_vars[i] = local_vars[i + 1];
    local_vars_count--;
    return ret;
}

int store_private_var(const char *name, int namelength, vartype *value) {
    vloc varindex = lookup_private_var(name, namelength, false);
    int i;
    if (varindex.not_found()) {
        if (local_vars_count == local_vars_capacity) {
            int nc = local_vars_capacity + 25;
            var_struct *nv = (var_struct *) realloc(local_vars, nc * sizeof(var_struct));
            if (nv == NULL)
                return ERR_INSUFFICIENT_MEMORY;
            local_vars_capacity = nc;
            local_vars = nv;
        }
        int idx = local_vars_count++;
        local_vars[idx].length = namelength;
        for (i = 0; i < namelength; i++)
            local_vars[idx].name[i] = name[i];
        local_vars[idx].level = get_rtn_level();
        local_vars[idx].flags = VAR_PRIVATE;
        local_vars[idx].value = value;
    } else {
        free_vartype(varindex.value());
        varindex.set_value(value);
    }
    return ERR_NONE;
}

int store_stack_reference(vartype *v) {
    vartype *v2 = dup_vartype(v);
    if (v2 == NULL)
        return ERR_INSUFFICIENT_MEMORY;
    int err = store_private_var("REF", 3, v2);
    if (err != ERR_NONE)
        free_vartype(v2);
    return err;
}

void reparse_all_equations() {
    // NOTE: We can safely assume that re-parsing all equations will succeed,
    // because we re-parse all equations anyway when the state file is loaded,
    // in order to re-create their parse trees. (The generated code and the
    // code map are loaded from the state file.) This means that parser changes
    // that cause formerly valid equations to become invalid will be caught by
    // unpersist_vartype(), and those equations will be converted to strings,
    // before we even get here. The re-parsing we're doing here is in order to
    // re-generate the code, in cases when code generator bugs have been fixed
    // or the semantics of generated code have changed.
    for (int4 i = 0; i < eq_dir->prgms_capacity; i++) {
        prgm_struct *prgm = eq_dir->prgms + i;
        if (prgm->text == NULL)
            continue;
        prgm_struct old_prgm = *prgm;
        equation_data *old_eqd = old_prgm.eq_data;
        prgm->size = prgm->capacity = 0;
        prgm->text = NULL;
        prgm->eq_data = NULL;
        int errpos = -1;
        equation_data *new_eqd = new_equation_data(old_eqd->text, old_eqd->length, flags.f.eqn_compat, &errpos, i);
        if (new_eqd == NULL) {
            // Should never happen, but now that we're here, let's at least
            // try to handle it gracefully, by keeping the old equation.
            *prgm = old_prgm;
        } else {
            // I can't just replace the old equation_data object, because of
            // all the vartype_equation objects referencing it. Hence, we
            // copy the parse tree and code map from the new equation_data
            // into the old one, and then delete the new one.
            delete old_eqd->ev;
            delete old_eqd->map;
            old_eqd->ev = new_eqd->ev;
            old_eqd->map = new_eqd->map;
            new_eqd->ev = NULL;
            new_eqd->map = NULL;
            delete new_eqd;
            free(old_prgm.text);
            prgm->eq_data = old_prgm.eq_data;
        }
    }
}

void remove_equation_reference(int4 id) {
    equation_data *eqd = eq_dir->prgms[id].eq_data;
    if (--(eqd->refcount) > 0)
        return;
    equation_deleted(id);
    count_embed_references(eq_dir, id, false);
    free(eq_dir->prgms[id].text);
    eq_dir->prgms[id].text = NULL;
    eq_dir->prgms[id].eq_data = NULL;
    delete eqd;
}
