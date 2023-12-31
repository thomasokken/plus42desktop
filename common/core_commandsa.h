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

#ifndef CORE_COMMANDSA_H
#define CORE_COMMANDSA_H 1

#include "free42.h"
#include "core_globals.h"

int docmd_plot_m(arg_struct *arg);
int docmd_pgmplot(arg_struct *arg);
int docmd_eqnplot(arg_struct *arg);
int docmd_param(arg_struct *arg);
int docmd_xaxis(arg_struct *arg);
int docmd_yaxis(arg_struct *arg);
int docmd_const(arg_struct *arg);
int docmd_view_p(arg_struct *arg);
int docmd_xmin(arg_struct *arg);
int docmd_xmax(arg_struct *arg);
int docmd_ymin(arg_struct *arg);
int docmd_ymax(arg_struct *arg);
int docmd_scan(arg_struct *arg);
int start_graph_varmenu(int role, int exit_cb);
void display_view_param(int key);
void display_plot_params(int key);
int return_to_plot(bool failure, bool stop);
bool plot_keydown(int key, int *repeat);
int plot_repeat();
int docmd_plot(arg_struct *arg);
int docmd_line(arg_struct *arg);

#endif
