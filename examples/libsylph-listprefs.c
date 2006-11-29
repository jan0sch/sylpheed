/* gcc -Wall -g -o libsylph-listprefs libsylph-listprefs.c -lsylph `pkg-config glib-2.0 --cflags --libs` */

#include <sylph/sylmain.h>
#include <sylph/prefs_common.h>
#include <sylph/account.h>

void list_commonprefs(void)
{
	PrefParam *param;
	gint i;
	gchar *val;

	param = prefs_common_get_params();
	for (i = 0; param[i].name != NULL; ++i) {
		switch (param[i].type) {
		case P_STRING:
			val = *(gchar **)param[i].data;
			if (!val)
				val = "(NULL)";
			break;
		case P_INT:
		case P_ENUM:
			val = itos(*(gint *)param[i].data);
			break;
		case P_BOOL:
			val = itos(*(gboolean *)param[i].data);
			break;
		case P_USHORT:
			val = itos(*(gushort *)param[i].data);
			break;
		default:
			break;
		}
		g_print("%s = %s (default: %s)\n",
			param[i].name, val,
			param[i].defval ? param[i].defval : "NULL");
	}
}

int main(int argc, char *argv[])
{
	syl_init();

	prefs_common_read_config();
	account_read_config_all();

	list_commonprefs();

	syl_cleanup();

	return 0;
}
