/* gcc -Wall -g -o libsylph-listfolder libsylph-listfolder.c -lsylph `pkg-config glib-2.0 --cflags --libs` */

#include <sylph/sylmain.h>
#include <sylph/prefs_common.h>
#include <sylph/account.h>
#include <sylph/folder.h>

static gboolean traverse_func(GNode *node, gpointer data)
{
	FolderItem *item;
	gchar *id;

	item = FOLDER_ITEM(node->data);
	id = folder_item_get_identifier(item);
	if (id) {
		g_print("%s (%d, %d, %d)\n",
			id, item->new, item->unread, item->total);
		g_free(id);
	}

	return FALSE;
}

void list_folders(void)
{
	GList *list;
	Folder *folder;

	for (list = folder_get_list(); list != NULL; list = list->next) {
		folder = FOLDER(list->data);
		if (folder->node)
			g_node_traverse(folder->node, G_PRE_ORDER,
					G_TRAVERSE_ALL, -1,
					traverse_func, NULL);
	}
}

int main(int argc, char *argv[])
{
	syl_init();

	prefs_common_read_config();
	account_read_config_all();

	if (folder_read_list() < 0) {
		g_warning("folder_read_list: error");
		return 1;
	}

	list_folders();

	syl_cleanup();

	return 0;
}
