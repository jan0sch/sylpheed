/* gcc -Wall -g -o libsylph-createmailbox libsylph-createmailbox.c -lsylph `pkg-config glib-2.0 --cflags --libs` */

#include <sylph/sylmain.h>
#include <sylph/prefs_common.h>
#include <sylph/folder.h>

Folder *create_mailbox(void)
{
	Folder *folder;

	/* create new MH Folder object */
	folder = folder_new(F_MH, "TestMailbox", "TestMailbox");
	/* create physical directories */
	folder_create_tree(folder);
	/* add to Folder list */
	folder_add(folder);
	/* scan directory tree */
	folder_scan_tree(folder);

	return folder;
}

int main(int argc, char *argv[])
{
	Folder *folder;
	FolderItem *item;

	syl_init();

	prefs_common_read_config();

	folder = create_mailbox();

	/* search created folder */
	item = folder_find_item_from_identifier("#mh/TestMailbox/inbox");
	if (item) {
		gchar *path = folder_item_get_path(item);
		g_print("%s found\n", path);
		g_free(path);
	}

	/* destroy folder tree */
	folder_destroy(folder);

	syl_cleanup();

	return 0;
}
