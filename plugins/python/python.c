/* $Id$ */

/*
 *  (C) Copyright 2004 Leszek Krupi�ski <leafnode@pld-linux.org>
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

#include "ekg2-config.h"

#include "python.h"
#include "python-ekg.h"
#include "python-config.h"

#include <sys/types.h>

#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <Python.h>

#include <ekg/commands.h>
#include <ekg/dynstuff.h>
#include <ekg/plugins.h>
#include <ekg/protocol.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/userlist.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>

int python_plugin_destroy();
int python_theme_init();
int python_exec(const char *);
int python_run(const char *filename);

/**
 * python_plugin
 *
 * struktura opisuj�ca plugin
 */

plugin_t python_plugin = {
        name: "python",
        pclass: PLUGIN_GENERIC,
        destroy: python_plugin_destroy,
        theme_init: python_theme_init,
};

// * ***************************************************************************
// *
// * Polecenia EKG
// *
// * ***************************************************************************

/**
 * python_command_eval()
 *
 * wykonanie kodu pythona
 *
 */

COMMAND(python_command_eval)
{
        if (!params[0]) {
                print("not_enough_params", name);
                return -1;
        }
        python_exec(params[0]);
        return 0;
}

/**
 * python_command_run()
 *
 * wykonanie skryptu pythonowego
 *
 */

COMMAND(python_command_run)
{
        if (!params[0]) {
                print("not_enough_params", name);
                return -1;
        }
        python_run(params[0]);
        return 0;
}

/**
 * python_command_load()
 *
 * za�adowanie modu�u
 *
 */

COMMAND(python_command_load)
{
        if (!params[0]) {
                print("not_enough_params", name);
                return -1;
        }
        python_load(params[0], quiet);
        return 0;
}

/**
 * python_command_unload()
 *
 * usuni�cie modu�u
 *
 */

COMMAND(python_command_unload)
{
        if (!params[0]) {
                print("not_enough_params", name);
                return -1;
        }
        python_unload(params[0], quiet);
        return 0;
}

/**
 * python_command_list()
 *
 * wy�wietla list� za�adowanych skrypt�w.
 *
 */

COMMAND(python_command_list)
{
	list_t l;

	if (!python_modules)
		printq("python_list_empty");

	for (l = python_modules; l; l = l->next) {
		struct python_module *m = l->data;

		printq("python_list", m->name);

	}

	return 0;
}

// * ***************************************************************************
// *
// * Hooki
// *
// * ***************************************************************************

/**
 * python_protocol_status()
 *
 * przetwarzanie status�w
 *
 */

int python_protocol_status(void *data, va_list ap)
{
        debug("[python] handling status\n");
	char **__session = va_arg(ap, char**), *session = *__session;
	char **__uid = va_arg(ap, char**), *uid = *__uid;
	char **__status = va_arg(ap, char**), *status = *__status;
	char **__descr = va_arg(ap, char**), *descr = *__descr;
        int python_handle_result;

        debug("[python] running python scripts\n");
        PYTHON_HANDLE_HEADER(status, "(ssss)", session, uid, status, descr)
        ;
        PYTHON_HANDLE_FOOTER()

	return 0;
}

/**
 * python_protocol_message()
 *
 * przetwarzanie wiadomo�ci
 *
 */

int python_protocol_message(void *data, va_list ap)
{
        debug("[python] handling protocol message\n");
        char **__session = va_arg(ap, char**), *session = *__session;
        char **__uid = va_arg(ap, char**), *uid = *__uid;
        char ***__rcpts = va_arg(ap, char***), **rcpts = *__rcpts;
        char **__text = va_arg(ap, char**), *text = *__text;
        uint32_t **__format = va_arg(ap, uint32_t**), *format = *__format;
        time_t *__sent = va_arg(ap, time_t*), sent = *__sent;
        int *__class = va_arg(ap, int*), class = *__class;
        char * target;
        userlist_t *u;
        session_t *s;
        int python_handle_result;

        if (!(s = session_find(session)))
                return 0;

        u = userlist_find(s, uid);

        int level = ignored_check(s, uid);

        if ((level == IGNORE_ALL) || (level & IGNORE_MSG))
                return 0;

        debug("[python] running python scripts\n");
        if (class == EKG_MSGCLASS_SENT || class == EKG_MSGCLASS_SENT_CHAT) {
                target = (rcpts) ? rcpts[0] : NULL;
                PYTHON_HANDLE_HEADER(msg_own, "(sss)", session, target, text)
                ;
                PYTHON_HANDLE_FOOTER()
        } else {
                PYTHON_HANDLE_HEADER(msg, "(ssisii)", session, uid, class, text, (int) sent, level)
                ;
                PYTHON_HANDLE_FOOTER()
        }

        return 0;
}

/**
 * python_timer()
 *
 * timer
 *
 */

void python_timer()
{
}


// ********************************************************************************
// *
// * Funkcje pomocnicze
// *
// ********************************************************************************

/**
 * python_exec()
 *
 * wykonuje polecenie pythona.
 *
 *  - command - polecenie.
 *
 */

int python_exec(const char *command)
{
        debug("[python] Running command: %s\n", command);
        char *tmp;

        if (!command)
                return 0;

        tmp = saprintf("import ekg\n%s\n", command);

        if (PyRun_SimpleString(tmp) == -1) {
                print("python_eval_error");
                debug("[python] script evaluation failed\n");
        }
        xfree(tmp);

        return 0;
}

/**
 * python_run()
 *
 * uruchamia jednorazowo skrypt pythona.
 *
 */

int python_run(const char *filename)
{
        FILE *f = fopen(filename, "r");

        if (!f) {
                print("python_not_found", filename);
                return -1;
        }

        PyRun_SimpleFile(f, (char*) filename);
        fclose(f);

        return 0;
}

/*
 * python_get_func()
 *
 * zwraca dan� funkcj� modu�u.
 */

PyObject *python_get_func(PyObject *module, const char *name)
{
	PyObject *result = PyObject_GetAttrString(module, (char*) name);

	if (result && !PyCallable_Check(result)) {
		Py_XDECREF(result);
		result = NULL;
	}

	return result;
}

/*
 * python_load()
 *
 * �aduje skrypt pythona o podanej nazwie z ~/(etc/).ekg/scripts
 *
 *  - name - nazwa skryptu,
 *  - quiet.
 *
 * 0/-1
 */
int python_load(const char *name, int quiet)
{
	PyObject *module, *init;
	struct python_module m;
	char *name2;

	if (!name) {
		printq("python_need_name");
		return -1;
	}

	if (strchr(name, '/')) {
		printq("python_wrong_location", prepare_path("scripts", 0));
		return -1;
	}

	name2 = xstrdup(name);

	if (strlen(name2) > 3 && !strcasecmp(name2 + strlen(name2) - 3, ".py"))
		name2[strlen(name2) - 3] = 0;

	module = PyImport_ImportModule(name2);

	if (!module) {
		printq("python_not_found", name2);
		PyErr_Print();
		xfree(name2);
		return -1;
	}

	if ((init = PyObject_GetAttrString(module, "init"))) {
		if (PyCallable_Check(init)) {
			PyObject *result = PyObject_CallFunction(init, "()");

			if (result) {
				int resulti = PyInt_AsLong(result);

				if (!resulti) {

				}

				Py_XDECREF(result);
			}
		}

		Py_XDECREF(init);
	}

	memset(&m, 0, sizeof(m));

	m.name                    = xstrdup(name2);
	m.module                  = module;
	m.deinit                  = python_get_func(module, "deinit");
	m.handle_msg              = python_get_func(module, "handle_msg");
	m.handle_msg_own          = python_get_func(module, "handle_msg_own");
	m.handle_status           = python_get_func(module, "handle_status");
	m.handle_status_own       = python_get_func(module, "handle_status_own");
	m.handle_connect          = python_get_func(module, "handle_connect");
	m.handle_disconnect       = python_get_func(module, "handle_disconnect");
//	m.handle_keypress         = python_get_func(module, "handle_keypress"); // XXX do dodania p�niej

	PyErr_Clear();

	list_add(&python_modules, &m, sizeof(m));

	xfree(name2);

        printq("python_loaded");

	return 0;
}

/*
 * python_unload()
 *
 * usuwa z pami�ci podany skrypt.
 *
 *  - name - nazwa skryptu,
 *  - quiet.
 *
 * 0/-1
 */
int python_unload(const char *name, int quiet)
{
	list_t l;

	if (!name) {
		printq("python_need_name");
		return -1;
	}

	for (l = python_modules; l; l = l->next) {
		struct python_module *m = l->data;

		if (strcmp(m->name, name))
			continue;

		debug("m->deinit = %p, hmm?\n", m->deinit);
		if (m->deinit) {
			PyObject *res = PyObject_CallFunction(m->deinit, "()");
			Py_XDECREF(res);
			Py_XDECREF(m->deinit);
		}

		Py_XDECREF(m->handle_msg);
		Py_XDECREF(m->handle_msg_own);
		Py_XDECREF(m->handle_connect);
		Py_XDECREF(m->handle_disconnect);
		Py_XDECREF(m->handle_status);
		Py_XDECREF(m->handle_status_own);
//		Py_XDECREF(m->handle_keypress); // XXX do dodania p�niej
		Py_XDECREF(m->module);

		list_remove(&python_modules, m, 1);

		printq("python_removed");

		return 0;
	}

	printq("python_not_found", name);

	return -1;
}

/*
 * python_autorun()
 *
 * �aduje skrypty z katalogu $CONFIG/scripts/autorun
 *
 */
int python_autorun()
{
	const char *path = prepare_path("scripts/autorun", 0);
	struct dirent *d;
	struct stat st;
	char *tmp;
	DIR *dir;

	if (!(dir = opendir(path)))
		return 0;

	/* nale�y utworzy� plik ~/(ekg/).ekg/scripts/autorun/__init__.py, inaczej
	 * python nie b�dzie mo�na �adowa� skrypt�w przez ,,autorun.nazwa'' */

	tmp = saprintf("%s/__init__.py", path);

	if (stat(tmp, &st)) {
		FILE *f = fopen(tmp, "w");
		if (f)
			fclose(f);
	}

	xfree(tmp);

	while ((d = readdir(dir))) {
		tmp = saprintf("%s/%s", path, d->d_name);

		if (stat(tmp, &st) || S_ISDIR(st.st_mode)) {
			xfree(tmp);
			continue;
		}

		xfree(tmp);

		if (!strcmp(d->d_name, "__init__.py"))
			continue;

		if (strlen(d->d_name) < 3 || strcmp(d->d_name + strlen(d->d_name) - 3, ".py"))
			continue;

		tmp = saprintf("autorun.%s", d->d_name);
		tmp[strlen(tmp) - 3] = 0;

		python_load(tmp, 0);

		xfree(tmp);
	}

	closedir(dir);
        return 1;
}

// ********************************************************************************
// *
// * Obs�uga interpretera
// *
// ********************************************************************************

/**
 * python_initialize()
 *
 * inicjalizacja interpretera
 *
 */

int python_initialize()
{
	PyObject *ekg, *ekg_config;

	/* PyImport_ImportModule spodziewa si� nazwy modu�u, kt�ry znajduje
	 * si� w $PYTHONPATH, wi�c dodajemy tam katalog ~/.gg/scripts. mo�na
	 * to zrobi� w bardziej elegancki spos�b, ale po co komplikowa� sobie
	 * �ycie? */

	if (getenv("PYTHONPATH")) {
		char *tmp = saprintf("%s:%s", getenv("PYTHONPATH"), prepare_path("scripts", 0));
#ifdef HAVE_SETENV
		setenv("PYTHONPATH", tmp, 1);
#else
		{
			char *s = saprintf("PYTHONPATH=%s", tmp);
			putenv(s);
			xfree(s);
		}
#endif
		xfree(tmp);
	} else {
#ifdef HAVE_SETENV
		setenv("PYTHONPATH", prepare_path("scripts", 0), 1);
#else
		{
			char *s = saprintf("PYTHONPATH=%s", prepare_path("scripts", 0));
			putenv(s);
			xfree(s);
		}
#endif
	}

	Py_Initialize();

	PyImport_AddModule("ekg");
	if (!(ekg = Py_InitModule("ekg", ekg_methods)))
		return -1;
	ekg_config = PyObject_NEW(PyObject, &ekg_config_type);

	PyModule_AddObject(ekg, "config", ekg_config);

	// Sta�e - og�lne

	PyModule_AddStringConstant(ekg, "VERSION", VERSION);

	// Sta�e - typy wiadomo�ci

	PyModule_AddIntConstant(ekg, "MSGCLASS_MESSAGE",   EKG_MSGCLASS_MESSAGE);
	PyModule_AddIntConstant(ekg, "MSGCLASS_CHAT",      EKG_MSGCLASS_CHAT);
	PyModule_AddIntConstant(ekg, "MSGCLASS_SENT",      EKG_MSGCLASS_SENT);
	PyModule_AddIntConstant(ekg, "MSGCLASS_SENT_CHAT", EKG_MSGCLASS_SENT_CHAT);
	PyModule_AddIntConstant(ekg, "MSGCLASS_SYSTEM",    EKG_MSGCLASS_SYSTEM);

	// Sta�e - typy status�w

	PyModule_AddStringConstant(ekg, "STATUS_NA",            EKG_STATUS_NA);
	PyModule_AddStringConstant(ekg, "STATUS_AVAIL",         EKG_STATUS_AVAIL);
	PyModule_AddStringConstant(ekg, "STATUS_AWAY",          EKG_STATUS_AWAY);
	PyModule_AddStringConstant(ekg, "STATUS_AUTOAWAY",      EKG_STATUS_AUTOAWAY);
	PyModule_AddStringConstant(ekg, "STATUS_INVISIBLE",     EKG_STATUS_INVISIBLE);
	PyModule_AddStringConstant(ekg, "STATUS_XA",            EKG_STATUS_XA);
	PyModule_AddStringConstant(ekg, "STATUS_DND",           EKG_STATUS_DND);
	PyModule_AddStringConstant(ekg, "STATUS_FREE_FOR_CHAT", EKG_STATUS_FREE_FOR_CHAT);
	PyModule_AddStringConstant(ekg, "STATUS_BLOCKED",       EKG_STATUS_BLOCKED);
	PyModule_AddStringConstant(ekg, "STATUS_UNKNOWN",       EKG_STATUS_UNKNOWN);
	PyModule_AddStringConstant(ekg, "STATUS_ERROR",         EKG_STATUS_ERROR);

	// Sta�� - ignorowanie

	PyModule_AddIntConstant(ekg, "IGNORE_STATUS",       IGNORE_STATUS);
	PyModule_AddIntConstant(ekg, "IGNORE_STATUS_DESCR", IGNORE_STATUS_DESCR);
	PyModule_AddIntConstant(ekg, "IGNORE_MSG",          IGNORE_MSG);
	PyModule_AddIntConstant(ekg, "IGNORE_DCC",          IGNORE_DCC);
	PyModule_AddIntConstant(ekg, "IGNORE_EVENTS",       IGNORE_EVENTS);
	PyModule_AddIntConstant(ekg, "IGNORE_NOTIFY",       IGNORE_NOTIFY);
	PyModule_AddIntConstant(ekg, "IGNORE_ALL",          IGNORE_ALL);

	return 0;
}

/**
 * python_finalize()
 *
 * oczyszczenie interpretera, usuni�cie modu��w, skrypt�w
 *
 */

int python_finalize()
{
	list_t l;

	for (l = python_modules; l; l = l->next) {
		struct python_module *m = l->data;

		xfree(m->name);

		if (m->deinit) {
			PyObject *res = PyObject_CallFunction(m->deinit, "()");
			Py_XDECREF(res);
			Py_XDECREF(m->deinit);
		}
	}

	list_destroy(python_modules, 1);
	python_modules = NULL;
        Py_Finalize();
        return 0;
}

// ********************************************************************************
// *
// * Obs�uga pluginu
// *
// ********************************************************************************

/**
 * python_theme_init()
 *
 * inicjalizacja formatek
 *
 */

int python_theme_init() {
	format_add("python_eval_error", "%! B��d wykonywania kodu\n", 1);
	format_add("python_list", "%> %1\n", 1);
	format_add("python_list_empty", "%! Brak za�adowanych skrypt�w\n", 1);
	format_add("python_loaded", "%) Skrypt zosta� za�adowany\n", 1);
	format_add("python_removed", "%) Skrypt zosta� usuni�ty\n", 1);
	format_add("python_need_name", "%! Nie podano nazwy skryptu\n", 1);
	format_add("python_error", "%! Error %T%1%n\n", 1);
	format_add("python_not_found", "%! Nie znaleziono skryptu %T%1%n\n", 1);
	format_add("python_wrong_location", "%! Skrypt nale�y umie�ci� w katalogu %T%1%n\n", 1);
        return 0;
}

/**
 * python_plugin_destroy()
 *
 * wyczyszczenie pluginu
 *
 */

int python_plugin_destroy()
{
        python_finalize();
        plugin_unregister(&python_plugin);
        return 0;
}

/**
 * python_plugin_init()
 *
 * inicjalizacja pluginu
 *
 */

int python_plugin_init()
{

        plugin_register(&python_plugin);
        python_theme_init();

        command_add(&python_plugin, "python:eval",   "?",  python_command_eval,   0, NULL);
        command_add(&python_plugin, "python:run",    "?",  python_command_run,    0, NULL);
        command_add(&python_plugin, "python:load",   "?",  python_command_load,   0, NULL);
        command_add(&python_plugin, "python:unload", "?",  python_command_unload, 0, NULL);
        command_add(&python_plugin, "python:list",    "",  python_command_list,   0, NULL);

        query_connect(&python_plugin, "protocol-message", python_protocol_message, NULL);
        query_connect(&python_plugin, "protocol-status",  python_protocol_status,  NULL);

        timer_add(&python_plugin, "python:timer_hook", 1, 1, python_timer, NULL);

        python_initialize();
	python_autorun();
        return 0;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 * vim: sts=8 sw=8
 */
