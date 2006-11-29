/* gcc -Wall -g -o libsylph-template libsylph-template.c -lsylph `pkg-config glib-2.0 --cflags --libs` */

#include <sylph/sylmain.h>
#include <sylph/prefs_common.h>
#include <sylph/account.h>

int main(int argc, char *argv[])
{
	syl_init();

	prefs_common_read_config();
	account_read_config_all();

	/* do something here */

	syl_cleanup();

	return 0;
}
