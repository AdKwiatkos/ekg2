/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __EKG_WINDOWS_H
#define __EKG_WINDOWS_H

#include "ekg2-config.h"

#include <ekg/commands.h>
#include <ekg/sessions.h>
#include <ekg/themes.h>

typedef struct {
	int id;			/* numer okna */
	char *target;		/* nick query albo inna nazwa albo NULL */
	session_t *session;	/* kt�rej sesji dotyczy okno */

	int left, top;		/* pozycja (x, y) wzgl�dem pocz�tku ekranu */
	int width, height;	/* wymiary okna */

	int act;		/* czy co� si� zmieni�o? */
	int more;		/* pojawi�o si� co� poza ekranem */

	int floating;		/* czy p�ywaj�ce? */
	int doodle;		/* czy do gryzmolenia? */
	int frames;		/* informacje o ramkach */
	int edge;		/* okienko brzegowe */
	int last_update;	/* czas ostatniego uaktualnienia */
	int nowrap;		/* nie zawijamy linii */
	int hide;		/* ukrywamy, bo jest zbyt du�e */
	int lock;		/* blokowanie zmian w obr�bie komendy */

	void *private;		/* prywatne informacje ui */
} window_t;

list_t windows;
window_t *window_current;

window_t *window_find(const char *target);
window_t *window_new(const char *target, session_t *session, int new_id);
void window_kill(window_t *w, int quiet);
void window_switch(int id);
void window_print(const char *target, session_t *session, int separate, fstring_t *line);

int window_session_cycle(window_t *w);
#define window_session_cycle_n(a) window_session_cycle(window_find(a))
#define window_session_cycle_id(a) window_session_cycle_id(window_find_id(a))
int window_session_set(window_t *w, session_t *s);
#define window_session_set_n(a,b) window_session_set(window_find(a),b)
#define window_session_set_id(a,b) window_session_set(window_find_id(a),b)
session_t *window_session_get(window_t *w);
#define window_session_get_n(a) window_session_get(window_find(a))
#define window_session_get_id(a) window_session_get(window_find_id(a))

int window_lock_set(window_t *w, int lock);
int window_lock_get(window_t *w);
int window_lock_inc(window_t *w);
#define window_lock_inc_n(a) window_lock_inc(window_find(a))
int window_lock_dec(window_t *w);
#define window_lock_dec_n(a) window_lock_dec(window_find(a))

COMMAND(cmd_window);

#endif /* __EKG_WINDOW_H */
