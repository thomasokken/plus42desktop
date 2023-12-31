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

#ifndef CORE_DISPLAY_H
#define CORE_DISPLAY_H 1

#include "free42.h"
#include "core_phloat.h"
#include "core_globals.h"

extern int disp_r, disp_c, disp_w, disp_h;
extern int requested_disp_r, requested_disp_c;

bool display_alloc(int rows, int cols);
bool display_exists();
bool persist_display();
bool unpersist_display(int ver);
void clear_display();
void flush_display();
void repaint_display();
void draw_pixel(int x, int y);
void draw_line(int x1, int y1, int x2, int y2);
void draw_pattern(phloat dx, phloat dy, const char *pattern, int pattern_width);
void move_crosshairs(int x, int y, bool show = true);
void hide_crosshairs();
bool get_crosshairs(int *x, int *y);
void fly_goose();
void move_prgm_highlight(int offset);

void squeak();
void tone(int n);
void draw_char(int x, int y, char c);
void draw_block(int x, int y);
const char *get_char(char c);
void draw_string(int x, int y, const char *s, int length);
void draw_message(int y, const char *s, int length, bool flush = true);
void clear_message();
void fill_rect(int x, int y, int width, int height, int color);
int draw_small_string(int x, int y, const char *s, int length, int max_width,
                            bool right_align = false, bool left_trunc = false,
                            bool reverse = false);
int small_string_width(const char *s, int length);
void draw_key(int n, int highlight, int hide_meta,
                            const char *s, int length, bool reverse = false);
bool should_highlight(int cmd);
int special_menu_key(int which);
void clear_row(int row);

void display_error(int error, bool print);
int start_varmenu_lbl(const char *name, int len, int role);
int start_varmenu_eqn(vartype *eq, int role);
void config_varmenu_lbl(const char *name, int len);
void config_varmenu_eqn(vartype *eq);
void config_varmenu_none();
void draw_varmenu();
bool show_nonlocal_vars(int catsect);
int draw_eqn_catalog(int section, int row, int *item);
void get_units_cat_row(int catsect, const char *text[6], int length[6], int *row, int *rows);
void display_mem();
void show();
void redisplay(int mode = 0);
bool display_header();
int tvm_message(char *buf, int buflen);

int print_display();
int print_program(pgm_index prgm, int4 pc, int4 lines, bool normal);
void print_program_line(pgm_index prgm, int4 pc);
int command2buf(char *buf, int len, int cmd, const arg_struct *arg);

struct textbuf {
    char *buf;
    size_t size;
    size_t capacity;
    bool fail;
};

void tb_write(textbuf *tb, const char *data, size_t size);
void tb_write_null(textbuf *tb);
void tb_indent(textbuf *tb, int indent);
void tb_print_current_program(textbuf *tb);

#define MENULEVEL_COMMAND   0
#define MENULEVEL_ALPHA     1
#define MENULEVEL_TRANSIENT 2
#define MENULEVEL_PLAIN     3
#define MENULEVEL_AUX       4
#define MENULEVEL_APP       5

void set_menu(int level, int menuid);
int set_menu_return_err(int level, int menuid, bool exitall);
void set_appmenu_exitcallback(int callback_id);
void set_plainmenu(int menuid);
void set_catalog_menu(int direction);
int get_front_menu();
void set_cat_section(int section);
void set_cat_section_no_top(int section);
int get_cat_section();
void move_cat_row(int direction);
void set_cat_row(int row);
int get_cat_row();
bool get_cat_item(int menukey, int4 *dir, int *item);
void update_catalog();

void clear_custom_menu();
void assign_custom_key(int keynum, const char *name, int length);
void get_custom_key(int keynum, char *name, int *length);

void clear_prgm_menu();
void assign_prgm_key(int keynum, bool is_gto, const arg_struct *arg);
void do_prgm_menu_key(int keynum);

#endif
