/* gcc -Wall -g -o libsylph-compose libsylph-compose.c -lsylph `pkg-config glib-2.0 --cflags --libs` */

#include <sylph/sylmain.h>
#include <sylph/prefs_common.h>
#include <sylph/account.h>
#include <sylph/folder.h>
#include <sylph/compose.h>

void do_compose(const gchar *to, const gchar *subject, const gchar *body)
{
	ComposeInfo *compose;
	PrefsAccount *ac;

	ac = account_get_current_account();
	compose = compose_info_new(ac, COMPOSE_NEW);

	compose_set_headers(compose, to, NULL, NULL, NULL, subject);
	compose_set_body(compose, body);
	compose_set_encoding(compose, CS_UTF_8, NULL);

	compose_write_to_file(compose, "mail.txt", NULL);

	compose_info_free(compose);
}

int main(int argc, char *argv[])
{
	GList *alist;
	gchar *to, *subject, *body;

	syl_init();

	if (argc < 4) {
		g_print("Usage: %s to subject body\n", argv[0]);
		return 1;
	}
	to = argv[1];
	subject = argv[2];
	body = argv[3];

	set_debug_mode(TRUE);

	prefs_common_read_config();
	account_read_config_all();

	if (folder_read_list() < 0) {
		g_warning("folder_read_list: error");
		return 1;
	}

	alist = account_get_list();
	if (!alist) {
		g_warning("no account found");
		return 1;
	}

	g_print("\n");

	do_compose(to, subject, body);

	syl_cleanup();

	return 0;
}
