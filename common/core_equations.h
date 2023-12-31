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

#ifndef CORE_EQUATIONS_H
#define CORE_EQUATIONS_H 1

#include "free42.h"


bool unpersist_eqn(int4 ver);
bool persist_eqn();
void reset_eqn();

int eqn_start(int whence);
void eqn_end();

bool eqn_active();
bool eqn_alt_keys();
bool eqn_editing();
char *eqn_copy();
void eqn_paste(const char *buf);

void eqn_set_selected_row(int row);
bool eqn_draw();
int eqn_keydown(int key, int *repeat);
int eqn_repeat();
bool eqn_timeout();
void eqn_restart_cursor();

int return_to_eqn_edit();
void eqn_save_error_pos(int eqn_id, int pos);


#endif
