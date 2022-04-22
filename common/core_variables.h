/*****************************************************************************
 * Plus42 -- an enhanced HP-42S calculator simulator
 * Copyright (C) 2004-2022  Thomas Okken
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

#ifndef CORE_VARIABLES_H
#define CORE_VARIABLES_H 1

#include <stdio.h>
#include <vector>
#include <string>
#include "core_phloat.h"

/***********************/
/* Variable data types */
/***********************/

class Evaluator;
class CodeMap;

class equation_data {
    public:
    int refcount;
    equation_data() : refcount(0), length(0), text(NULL), ev(NULL), map(NULL) {}
    ~equation_data();
    int4 length;
    char *text;
    Evaluator *ev;
    CodeMap *map;
    bool compatMode;
    bool compatModeEmbedded;
    int eqn_index;
};

struct pgm_index {
    int4 dir;
    int4 idx;
    pgm_index() {}
    pgm_index(int4 dir, int4 idx) : dir(dir), idx(idx) {}
    void set(int4 dir, int4 idx) {
        this->dir = dir;
        this->idx = idx;
    }
    bool operator==(const pgm_index &that) {
        return dir == that.dir && idx == that.idx;
    }
    bool operator!=(const pgm_index &that) {
        return dir != that.dir || idx != that.idx;
    }
    bool is_editable();
};

#define TYPE_NULL 0
#define TYPE_REAL 1
#define TYPE_COMPLEX 2
#define TYPE_REALMATRIX 3
#define TYPE_COMPLEXMATRIX 4
#define TYPE_STRING 5
#define TYPE_LIST 6
#define TYPE_EQUATION 7
#define TYPE_UNIT 8
#define TYPE_DIR_REF 9
#define TYPE_PGM_REF 10
#define TYPE_VAR_REF 11
#define TYPE_SENTINEL 12

struct vartype {
    int type;
};


struct vartype_real {
    int type;
    phloat x;
};


struct vartype_complex {
    int type;
    phloat re, im;
};


struct realmatrix_data {
    int refcount;
    phloat *data;
    char *is_string;
};

struct vartype_realmatrix {
    int type;
    int4 rows;
    int4 columns;
    realmatrix_data *array;
};


struct complexmatrix_data {
    int refcount;
    phloat *data;
};

struct vartype_complexmatrix {
    int type;
    int4 rows;
    int4 columns;
    complexmatrix_data *array;
};


/* Maximum short string length in a stand-alone variable */
#define SSLENV ((int) (sizeof(char *) < 8 ? 8 : sizeof(char *)))
/* Maximum short string length in a matrix element */
#define SSLENM ((int) sizeof(phloat) - 1)

struct vartype_string {
    int type;
    int4 length;
    /* When length <= SSLENV, use buf; otherwise, use ptr */
    union {
        char buf[SSLENV];
        char *ptr;
    } t;
    char *txt() {
        return length > SSLENV ? t.ptr : t.buf;
    }
    const char *txt() const {
        return length > SSLENV ? t.ptr : t.buf;
    }
    void trim1();
};


struct list_data {
    int refcount;
    vartype **data;
};

struct vartype_list {
    int type;
    int4 size;
    list_data *array;
};


struct vartype_equation {
    int type;
    equation_data *data;
};


struct vartype_unit {
    int type;
    phloat x;
    char *text;
    int4 length;
};


struct vartype_dir_ref {
    int type;
    int4 dir;
};

struct vartype_pgm_ref {
    int type;
    int4 dir;
    int4 pgm;
};

struct vartype_var_ref {
    int type;
    int4 dir;
    unsigned char length;
    char name[7];
};

struct vloc {
    int dir;
    int idx;
    vloc() : dir(1), idx(-1) {}
    vloc(int dir, int idx) : dir(dir), idx(idx) {}
    bool not_found() { return idx == -1; }
    vartype *value();
    void set_value(vartype *value);
    int level() { return dir <= 0 ? -dir : -1; }
    bool writable();
};


vartype *new_real(phloat value);
vartype *new_complex(phloat re, phloat im);
vartype *new_string(const char *s, int slen);
vartype *new_realmatrix(int4 rows, int4 columns);
vartype *new_complexmatrix(int4 rows, int4 columns);
vartype *new_list(int4 size);
vartype *new_equation(const char *text, int4 length, bool compat_mode, int *errpos);
vartype *new_equation(equation_data *eqd);
vartype *new_unit(phloat value, const char *text, int4 length);
vartype *new_dir_ref(int4 dir);
vartype *new_pgm_ref(int4 dir, int4 pgm);
vartype *new_var_ref(int4 dir, const char *name, int length);
void free_vartype(vartype *v);
void clean_vartype_pools();
void free_long_strings(char *is_string, phloat *data, int4 n);
void get_matrix_string(vartype_realmatrix *rm, int4 i, char **text, int4 *length);
void get_matrix_string(const vartype_realmatrix *rm, int4 i, const char **text, int4 *length);
bool put_matrix_string(vartype_realmatrix *rm, int4 i, const char *text, int4 length);
void put_matrix_phloat(vartype_realmatrix *rm, int4 i, phloat value);
vartype *dup_vartype(const vartype *v);
bool disentangle(vartype *v);
vloc lookup_var(const char *name, int namelength, bool no_locals = false, bool no_ancestors = false);
vartype *recall_var(const char *name, int namelength, bool *writable = NULL);
vartype *recall_global_var(const char *name, int namelength, bool *writable = NULL);
equation_data *find_equation_data(const char *name, int namelength);
std::vector<std::string> get_equation_names();
std::vector<int> get_equation_indexes();
int store_params();
bool ensure_var_space(int n);
int store_var(const char *name, int namelength, vartype *value, bool local = false, bool global = false);
int store_root_var(const char *name, int namelength, vartype *value);
bool purge_var(const char *name, int namelength, bool force = false, bool global = true, bool local = true);
bool vars_exist(int section);
bool named_eqns_exist();
bool contains_strings(const vartype_realmatrix *rm);
int matrix_copy(vartype *dst, const vartype *src);
vartype *recall_private_var(const char *name, int namelength);
vartype *recall_and_purge_private_var(const char *name, int namelength);
int store_private_var(const char *name, int namelength, vartype *value);
int store_stack_reference(vartype *value);

#endif
