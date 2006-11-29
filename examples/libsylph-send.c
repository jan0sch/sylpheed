/* gcc -Wall -g -o libsylph-send libsylph-send.c -lsylph `pkg-config glib-2.0 --cflags --libs` */

#include <sylph/sylmain.h>
#include <sylph/prefs_common.h>
#include <sylph/account.h>
#include <sylph/smtp.h>

#define SMTP_PORT	25

void send_message(const gchar *to, const gchar *file)
{
	PrefsAccount *ac;
	Session *session;
	GSList *to_list;
	FILE *fp;

	ac = account_get_current_account();
	g_return_if_fail(ac != NULL);
	g_return_if_fail(ac->address != NULL);

	g_print("from: %s\nto: %s\n", ac->address, to);

	if ((fp = g_fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return;
	}

	/* create session */
	session = smtp_session_new();
	SMTP_SESSION(session)->from = g_strdup(ac->address);
	to_list = g_slist_append(NULL, g_strdup(to));
	SMTP_SESSION(session)->to_list = to_list;
	SMTP_SESSION(session)->cur_to = to_list;
	SMTP_SESSION(session)->send_data_fp = get_outgoing_rfc2822_file(fp);
	SMTP_SESSION(session)->send_data_len =
		get_left_file_size(SMTP_SESSION(session)->send_data_fp);

	fclose(fp);

	/* connect */
	if (session_connect(session, ac->smtp_server, SMTP_PORT) < 0) {
		goto finish;
	}

	/* start session */
	g_print("SMTP session start\n");

	while (session_is_connected(session))
		g_main_iteration(TRUE);

	if (session->state == SESSION_ERROR ||
	    session->state == SESSION_EOF ||
	    session->state == SESSION_TIMEOUT ||
	    SMTP_SESSION(session)->state == SMTP_ERROR ||
	    SMTP_SESSION(session)->error_val != SM_OK) {
		if (SMTP_SESSION(session)->error_msg)
			g_warning("error occurred: %s\n",
				  SMTP_SESSION(session)->error_msg);
		else
			g_warning("error occurred\n");
	}

finish:
	session_destroy(session);
	slist_free_strings(to_list);
	g_slist_free(to_list);
}

int main(int argc, char *argv[])
{
	gchar *to, *file;

	syl_init();

	if (argc < 3) {
		g_print("Usage: %s message-file to-address\n", argv[0]);
		return 1;
	}
	file = argv[1];
	to = argv[2];

	set_debug_mode(TRUE);

	prefs_common_read_config();
	account_read_config_all();

	send_message(to, file);

	syl_cleanup();

	return 0;
}
