/* gcc -Wall -g -o libsylph-listsummary libsylph-listsummary.c -lsylph `pkg-config glib-2.0 --cflags --libs` */

#include <sylph/sylmain.h>
#include <sylph/prefs_common.h>
#include <sylph/account.h>
#include <sylph/folder.h>
#include <sylph/procmsg.h>

void list_messages(const gchar *folder_id)
{
	FolderItem *item;
	GSList *mlist, *cur;

	if (!folder_id)
		folder_id = "inbox";

	item = folder_find_item_from_identifier(folder_id);
	if (!item) {
		g_warning("folder item '%s' not found.\n", folder_id);
		exit(1);
	}
	mlist = folder_item_get_msg_list(item, TRUE);

	for (cur = mlist; cur != NULL; cur = cur->next) {
		MsgInfo *msginfo = (MsgInfo *)cur->data;

		g_print("%u %s %s %s\n", msginfo->msgnum, msginfo->subject, msginfo->from, msginfo->date);
	}

	procmsg_msg_list_free(mlist);
}

int main(int argc, char *argv[])
{
	gchar *folder_id = NULL;

	syl_init();

	if (argc > 1)
		folder_id = argv[1];

	prefs_common_read_config();
	account_read_config_all();

	if (folder_read_list() < 0) {
		g_warning("folder_read_list: error");
		return 1;
	}

	list_messages(folder_id);

	syl_cleanup();

	return 0;
}
