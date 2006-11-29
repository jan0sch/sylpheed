/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>

#ifdef G_OS_UNIX
#  include <signal.h>
#endif

#if HAVE_LOCALE_H
#  include <locale.h>
#endif

#include "sylmain.h"
#include "codeconv.h"
#include "utils.h"

#define PACKAGE	"libsylph"

void syl_init(void)
{
#ifdef G_OS_WIN32
	gchar *newpath;
	const gchar *lang_env;

	/* disable locale variable such as "LANG=1041" */

#define DISABLE_DIGIT_LOCALE(envstr)			\
{							\
	lang_env = g_getenv(envstr);			\
	if (lang_env && g_ascii_isdigit(lang_env[0]))	\
		g_unsetenv(envstr);			\
}

	DISABLE_DIGIT_LOCALE("LC_ALL");
	DISABLE_DIGIT_LOCALE("LANG");
	DISABLE_DIGIT_LOCALE("LC_CTYPE");
	DISABLE_DIGIT_LOCALE("LC_MESSAGES");

#undef DISABLE_DIGIT_LOCALE
#endif

	setlocale(LC_ALL, "");

	set_startup_dir();

#ifdef G_OS_WIN32
	/* include startup directory into %PATH% for GSpawn */
	newpath = g_strconcat(get_startup_dir(), ";", g_getenv("PATH"), NULL);
	g_setenv("PATH", newpath, TRUE);
	g_free(newpath);
#endif

	if (g_path_is_absolute(LOCALEDIR))
		bindtextdomain(PACKAGE, LOCALEDIR);
	else {
		gchar *locale_dir;

		locale_dir = g_strconcat(get_startup_dir(), G_DIR_SEPARATOR_S,
					 LOCALEDIR, NULL);
#ifdef G_OS_WIN32
		{
			gchar *locale_dir_;

			locale_dir_ = g_locale_from_utf8(locale_dir, -1,
							 NULL, NULL, NULL);
			if (locale_dir_) {
				g_free(locale_dir);
				locale_dir = locale_dir_;
			}
		}
#endif
		bindtextdomain(PACKAGE, locale_dir);
		g_free(locale_dir);
	}

	bind_textdomain_codeset(PACKAGE, CS_UTF_8);
	textdomain(PACKAGE);

	sock_init();
#if USE_SSL
	ssl_init();
#endif

#ifdef G_OS_UNIX
	/* ignore SIGPIPE signal for preventing sudden death of program */
	signal(SIGPIPE, SIG_IGN);
#endif
}

#define MAKE_DIR_IF_NOT_EXIST(dir)					\
{									\
	if (!is_dir_exist(dir)) {					\
		if (is_file_exist(dir)) {				\
			g_warning("File '%s' already exists. "		\
				  "Can't create folder.", dir);		\
			return -1;					\
		}							\
	}								\
	if (make_dir(dir) < 0)						\
		return -1;						\
}

gint syl_setup_rc_dir(void)
{
	if (!is_dir_exist(get_rc_dir())) {
		if (make_dir_hier(get_rc_dir()) < 0)
			return -1;
	}

	MAKE_DIR_IF_NOT_EXIST(get_mail_base_dir());

	CHDIR_RETURN_VAL_IF_FAIL(get_rc_dir(), -1);

	MAKE_DIR_IF_NOT_EXIST(get_imap_cache_dir());
	MAKE_DIR_IF_NOT_EXIST(get_news_cache_dir());
	MAKE_DIR_IF_NOT_EXIST(get_mime_tmp_dir());
	MAKE_DIR_IF_NOT_EXIST(get_tmp_dir());
	MAKE_DIR_IF_NOT_EXIST(UIDL_DIR);

	/* remove temporary files */
	remove_all_files(get_tmp_dir());
	remove_all_files(get_mime_tmp_dir());

	return 0;
}

void syl_save_all_state(void)
{
	folder_write_list();
	prefs_common_write_config();
	filter_write_config();
	account_write_config_all();
}

void syl_cleanup(void)
{
	/* remove temporary files */
	remove_all_files(get_tmp_dir());
	remove_all_files(get_mime_tmp_dir());
	g_log_set_default_handler(g_log_default_handler, NULL);
	close_log_file();

#if USE_SSL
	ssl_done();
#endif
	sock_cleanup();
}
