/* gcc -Wall -g -o libsylph-pop libsylph-pop.c -lsylph `pkg-config glib-2.0 --cflags --libs` */

#include <sylph/sylmain.h>
#include <sylph/prefs_common.h>
#include <sylph/account.h>
#include <sylph/folder.h>
#include <sylph/pop.h>

#define POP3_PORT	110

static gint drop_message(Pop3Session *session, const gchar *file)
{
	FolderItem *inbox;

	inbox = folder_get_default_inbox();
	g_return_val_if_fail(inbox != NULL, DROP_ERROR);

	/* add received message to inbox */
	if (folder_item_add_msg(inbox, file, NULL, FALSE) < 0) {
		return DROP_ERROR;
	}

	return DROP_OK;
}

void pop_message(void)
{
	PrefsAccount *ac;
	Session *session;

	ac = account_get_current_account();
	g_return_if_fail(ac != NULL);
	g_return_if_fail(ac->recv_server != NULL);
	g_return_if_fail(ac->userid != NULL);
	g_return_if_fail(ac->passwd != NULL);

	g_print("Receiving from %s\n", ac->recv_server);

	/* create session */
	session = pop3_session_new(ac);
	POP3_SESSION(session)->drop_message = drop_message;
	POP3_SESSION(session)->user = g_strdup(ac->userid);
	POP3_SESSION(session)->pass = g_strdup(ac->passwd);

	/* connect */
	if (session_connect(session, ac->recv_server, POP3_PORT) < 0) {
		session_destroy(session);
		return;
	}

	/* start session */
	g_print("POP3 session start\n");

	while (session_is_connected(session))
		g_main_iteration(TRUE);

	if (session->state == SESSION_ERROR ||
	    session->state == SESSION_EOF ||
	    session->state == SESSION_TIMEOUT ||
	    POP3_SESSION(session)->state == POP3_ERROR ||
	    POP3_SESSION(session)->error_val != PS_SUCCESS) {
		if (POP3_SESSION(session)->error_msg)
			g_warning("error occurred: %s\n",
				  POP3_SESSION(session)->error_msg);
		else
			g_warning("error occurred\n");
	}

	session_destroy(session);
}

int main(int argc, char *argv[])
{
	syl_init();

	set_debug_mode(TRUE);

	prefs_common_read_config();
	account_read_config_all();
	folder_read_list();

	pop_message();

	syl_cleanup();

	return 0;
}
