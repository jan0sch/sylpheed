
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#if HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#include <signal.h>
#include <errno.h>
#ifdef G_OS_WIN32
#  include <windows.h>
#endif

#include "compose.h"
#include "procmsg.h"
#include "imap.h"
#include "news.h"
#include "customheader.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "account.h"
#include "procheader.h"
#include "procmime.h"
#include "base64.h"
#include "quoted-printable.h"
#include "codeconv.h"
#include "utils.h"
#include "socket.h"
#include "folder.h"
#include "filter.h"

#define B64_LINE_SIZE		57
#define B64_BUFFSIZE		77

#define MAX_REFERENCES_LEN	999

static gchar *compose_parse_references		(const gchar	*ref,
						 const gchar	*msgid);

static void compose_write_attach		(ComposeInfo	*compose,
						 FILE		*fp,
						 const gchar	*charset);
static gint compose_write_headers		(ComposeInfo	*compose,
						 FILE		*fp,
						 const gchar	*charset,
						 const gchar	*body_charset,
						 EncodingType	 encoding);
static gint compose_redirect_write_headers	(ComposeInfo	*compose,
						 FILE		*fp);

static void compose_convert_header		(ComposeInfo	*compose,
						 gchar		*dest,
						 gint		 len,
						 const gchar	*src,
						 gint		 header_len,
						 gboolean	 addr_field,
						 const gchar	*encoding);
static gchar *compose_convert_filename		(ComposeInfo	*compose,
						 const gchar	*src,
						 const gchar	*param_name,
						 const gchar	*encoding);


ComposeInfo *compose_info_new(PrefsAccount *account, ComposeMode mode)
{
	ComposeInfo *compose;

	g_return_val_if_fail(account != NULL, NULL);

	compose = g_new0(ComposeInfo, 1);

	compose->mode = mode;
	compose->flags = 0;

	compose->account = account;

	return compose;
}

void compose_info_free(ComposeInfo *compose)
{
	if (!compose)
		return;

	g_free(compose);
}

ComposeAttachInfo *compose_attach_info_new(const gchar *file,
					   const gchar *content_type,
					   EncodingType encoding,
					   const gchar *name, gsize size)
{
	ComposeAttachInfo *ainfo;

	ainfo = g_new0(ComposeAttachInfo, 1);
	ainfo->file = g_strdup(file);
	ainfo->content_type = g_strdup(content_type);
	ainfo->encoding = encoding;
	ainfo->name = g_strdup(name);
	ainfo->size = size;

	return ainfo;
}

void compose_attach_info_free(ComposeAttachInfo *ainfo)
{
	if (!ainfo)
		return;

	g_free(ainfo->file);
	g_free(ainfo->content_type);
	g_free(ainfo->name);
	g_free(ainfo);
}

gint compose_parse_header(ComposeInfo *compose, MsgInfo *msginfo)
{
	static HeaderEntry hentry[] = {{"Reply-To:",	NULL, TRUE},
				       {"Cc:",		NULL, TRUE},
				       {"References:",	NULL, FALSE},
				       {"Bcc:",		NULL, TRUE},
				       {"Newsgroups:",  NULL, TRUE},
				       {"Followup-To:", NULL, TRUE},
				       {"List-Post:",   NULL, FALSE},
				       {"Content-Type:",NULL, FALSE},
				       {NULL,		NULL, FALSE}};

	enum
	{
		H_REPLY_TO	= 0,
		H_CC		= 1,
		H_REFERENCES	= 2,
		H_BCC		= 3,
		H_NEWSGROUPS    = 4,
		H_FOLLOWUP_TO	= 5,
		H_LIST_POST     = 6,
		H_CONTENT_TYPE  = 7
	};

	FILE *fp;
	gchar *charset = NULL;

	g_return_val_if_fail(msginfo != NULL, -1);

	g_free(compose->inreplyto);
	compose->inreplyto = NULL;
	g_free(compose->references);
	compose->references = NULL;

	if ((fp = procmsg_open_message(msginfo)) == NULL) return -1;
	procheader_get_header_fields(fp, hentry);
	fclose(fp);

	if (hentry[H_CONTENT_TYPE].body != NULL) {
		procmime_scan_content_type_str(hentry[H_CONTENT_TYPE].body,
					       NULL, &charset, NULL, NULL);
		g_free(hentry[H_CONTENT_TYPE].body);
		hentry[H_CONTENT_TYPE].body = NULL;
	}
	if (hentry[H_REPLY_TO].body != NULL) {
		if (hentry[H_REPLY_TO].body[0] != '\0') {
			compose->ref_replyto =
				conv_unmime_header(hentry[H_REPLY_TO].body,
						   charset);
		}
		g_free(hentry[H_REPLY_TO].body);
		hentry[H_REPLY_TO].body = NULL;
	}
	if (hentry[H_CC].body != NULL) {
		compose->ref_cc =
			conv_unmime_header(hentry[H_CC].body, charset);
		g_free(hentry[H_CC].body);
		hentry[H_CC].body = NULL;
	}
	if (hentry[H_REFERENCES].body != NULL) {
		if (compose->mode == COMPOSE_REEDIT)
			compose->references = hentry[H_REFERENCES].body;
		else {
			compose->references = compose_parse_references
				(hentry[H_REFERENCES].body, msginfo->msgid);
			g_free(hentry[H_REFERENCES].body);
		}
		hentry[H_REFERENCES].body = NULL;
	}
	if (hentry[H_BCC].body != NULL) {
		if (compose->mode == COMPOSE_REEDIT)
			compose->ref_bcc =
				conv_unmime_header(hentry[H_BCC].body, charset);
		g_free(hentry[H_BCC].body);
		hentry[H_BCC].body = NULL;
	}
	if (hentry[H_NEWSGROUPS].body != NULL) {
		compose->ref_newsgroups = hentry[H_NEWSGROUPS].body;
		hentry[H_NEWSGROUPS].body = NULL;
	}
	if (hentry[H_FOLLOWUP_TO].body != NULL) {
		if (hentry[H_FOLLOWUP_TO].body[0] != '\0') {
			compose->ref_followup_to =
				conv_unmime_header(hentry[H_FOLLOWUP_TO].body,
						   charset);
		}
		g_free(hentry[H_FOLLOWUP_TO].body);
		hentry[H_FOLLOWUP_TO].body = NULL;
	}
	if (hentry[H_LIST_POST].body != NULL) {
		gchar *to = NULL;

		extract_address(hentry[H_LIST_POST].body);
		if (hentry[H_LIST_POST].body[0] != '\0') {
			scan_mailto_url(hentry[H_LIST_POST].body,
					&to, NULL, NULL, NULL, NULL);
			if (to) {
				g_free(compose->ref_ml_post);
				compose->ref_ml_post = to;
			}
		}
		g_free(hentry[H_LIST_POST].body);
		hentry[H_LIST_POST].body = NULL;
	}

	g_free(charset);

	if (compose->mode == COMPOSE_REEDIT) {
		if (msginfo->inreplyto && *msginfo->inreplyto)
			compose->inreplyto = g_strdup(msginfo->inreplyto);
		return 0;
	}

	if (msginfo->msgid && *msginfo->msgid)
		compose->inreplyto = g_strdup(msginfo->msgid);

	if (!compose->references) {
		if (msginfo->msgid && *msginfo->msgid) {
			if (msginfo->inreplyto && *msginfo->inreplyto)
				compose->references =
					g_strdup_printf("<%s>\n\t<%s>",
							msginfo->inreplyto,
							msginfo->msgid);
			else
				compose->references =
					g_strconcat("<", msginfo->msgid, ">",
						    NULL);
		} else if (msginfo->inreplyto && *msginfo->inreplyto) {
			compose->references =
				g_strconcat("<", msginfo->inreplyto, ">",
					    NULL);
		}
	}

	return 0;
}

static gchar *compose_parse_references(const gchar *ref, const gchar *msgid)
{
	GSList *ref_id_list, *cur;
	GString *new_ref;
	gchar *new_ref_str;

	ref_id_list = references_list_append(NULL, ref);
	if (!ref_id_list) return NULL;
	if (msgid && *msgid)
		ref_id_list = g_slist_append(ref_id_list, g_strdup(msgid));

	for (;;) {
		gint len = 0;

		for (cur = ref_id_list; cur != NULL; cur = cur->next)
			/* "<" + Message-ID + ">" + CR+LF+TAB */
			len += strlen((gchar *)cur->data) + 5;

		if (len > MAX_REFERENCES_LEN) {
			/* remove second message-ID */
			if (ref_id_list && ref_id_list->next &&
			    ref_id_list->next->next) {
				g_free(ref_id_list->next->data);
				ref_id_list = g_slist_remove
					(ref_id_list, ref_id_list->next->data);
			} else {
				slist_free_strings(ref_id_list);
				g_slist_free(ref_id_list);
				return NULL;
			}
		} else
			break;
	}

	new_ref = g_string_new("");
	for (cur = ref_id_list; cur != NULL; cur = cur->next) {
		if (new_ref->len > 0)
			g_string_append(new_ref, "\n\t");
		g_string_sprintfa(new_ref, "<%s>", (gchar *)cur->data);
	}

	slist_free_strings(ref_id_list);
	g_slist_free(ref_id_list);

	new_ref_str = new_ref->str;
	g_string_free(new_ref, FALSE);

	return new_ref_str;
}

gchar *compose_get_signature_str(ComposeInfo *compose)
{
	gchar *sig_path;
	gchar *sig_body = NULL;
	gchar *sig_str = NULL;
	gchar *utf8_sig_str = NULL;

	g_return_val_if_fail(compose->account != NULL, NULL);

	if (!compose->account->sig_path)
		return NULL;
	if (g_path_is_absolute(compose->account->sig_path) ||
	    compose->account->sig_type == SIG_COMMAND)
		sig_path = g_strdup(compose->account->sig_path);
	else {
#ifdef G_OS_WIN32
		sig_path = g_strconcat(get_rc_dir(),
#else
		sig_path = g_strconcat(get_home_dir(),
#endif
				       G_DIR_SEPARATOR_S,
				       compose->account->sig_path, NULL);
	}

	if (compose->account->sig_type == SIG_FILE) {
		if (!is_file_or_fifo_exist(sig_path)) {
			debug_print("can't open signature file: %s\n",
				    sig_path);
			g_free(sig_path);
			return NULL;
		}
	}

	if (compose->account->sig_type == SIG_COMMAND)
		sig_body = get_command_output(sig_path);
	else {
		gchar *tmp;

		tmp = file_read_to_str(sig_path);
		if (!tmp)
			return NULL;
		sig_body = normalize_newlines(tmp);
		g_free(tmp);
	}
	g_free(sig_path);

	if (prefs_common.sig_sep) {
		sig_str = g_strconcat(prefs_common.sig_sep, "\n", sig_body,
				      NULL);
		g_free(sig_body);
	} else
		sig_str = sig_body;

	if (sig_str) {
		gint error = 0;

		utf8_sig_str = conv_codeset_strdup_full
			(sig_str, conv_get_locale_charset_str(),
			 CS_INTERNAL, &error);
		if (!utf8_sig_str || error != 0) {
			if (g_utf8_validate(sig_str, -1, NULL) == TRUE) {
				g_free(utf8_sig_str);
				utf8_sig_str = sig_str;
			} else
				g_free(sig_str);
		} else
			g_free(sig_str);
	}

	return utf8_sig_str;
}

gint compose_set_headers(ComposeInfo *compose,
			 const gchar *to, const gchar *cc, const gchar *bcc,
			 const gchar *replyto, const gchar *subject)
{
	g_free(compose->to);
	compose->to = g_strdup(to);
	g_free(compose->cc);
	compose->cc = g_strdup(cc);
	g_free(compose->bcc);
	compose->bcc = g_strdup(bcc);
	g_free(compose->replyto);
	compose->replyto = g_strdup(replyto);
	g_free(compose->subject);
	compose->subject = g_strdup(subject);

	return 0;
}

gint compose_set_news_headers(ComposeInfo *compose,
			      const gchar *newsgroups, const gchar *followup_to)
{
	g_free(compose->newsgroups);
	compose->newsgroups = g_strdup(newsgroups);
	g_free(compose->followup_to);
	compose->followup_to = g_strdup(followup_to);

	return 0;
}

gint compose_set_body(ComposeInfo *compose, const gchar *body)
{
	g_free(compose->body_text);
	compose->body_text = canonicalize_str(body);
	return 0;
}

gint compose_set_encoding(ComposeInfo *compose, const gchar *header_encoding,
			  const gchar *body_encoding)
{
	g_free(compose->header_encoding);
	compose->header_encoding = g_strdup(header_encoding);
	g_free(compose->body_encoding);
	compose->body_encoding = g_strdup(body_encoding);
	return 0;
}

gint compose_set_attachments(ComposeInfo *compose, GList *attach_list)
{
	compose->attach_list = attach_list;
	return 0;
}

gint compose_write_to_file(ComposeInfo *compose, const gchar *out_file,
			   GError *error)
{
	FILE *fp;
	size_t len;
	const gchar *header_encoding;
	const gchar *body_encoding;
	const gchar *src_encoding = CS_INTERNAL;
	EncodingType encoding;
	gint line;

	g_return_val_if_fail(compose->body_text != NULL, -1);

	/* get outgoing charset */
	header_encoding = compose->header_encoding;
	if (!header_encoding)
		header_encoding = conv_get_outgoing_charset_str();
	if (!g_ascii_strcasecmp(header_encoding, CS_US_ASCII))
		header_encoding = CS_ISO_8859_1;
	body_encoding = compose->body_encoding;
	if (!body_encoding)
		body_encoding = conv_get_outgoing_charset_str();
	if (!g_ascii_strcasecmp(body_encoding, CS_US_ASCII))
		body_encoding = CS_ISO_8859_1;

	if (is_ascii_str(compose->body_text)) {
		body_encoding = CS_US_ASCII;
		encoding = ENC_7BIT;
	} else {
		gint err = 0;
		gchar *buf;

		buf = conv_codeset_strdup_full
			(compose->body_text, src_encoding, body_encoding, &err);
		if (!buf || err != 0) {
			/* FIXME: ask for UTF-8 */
			header_encoding = body_encoding = src_encoding;
			g_free(buf);
		} else {
			g_free(compose->body_text);
			compose->body_text = buf;
		}

		if (prefs_common.encoding_method == CTE_BASE64)
			encoding = ENC_BASE64;
		else if (prefs_common.encoding_method == CTE_QUOTED_PRINTABLE)
			encoding = ENC_QUOTED_PRINTABLE;
		else if (prefs_common.encoding_method == CTE_8BIT)
			encoding = ENC_8BIT;
		else
			encoding = procmime_get_encoding_for_charset
				(body_encoding);
	}

	debug_print("src encoding = %s, header encoding = %s, "
		    "body encoding = %s, transfer encoding = %s\n",
		    src_encoding, header_encoding, body_encoding,
		    procmime_get_encoding_str(encoding));

	/* rewrite buffer or check contents here */
	if (compose->preprocess_func) {
		if (compose->preprocess_func(compose, compose->data) < 0)
			return -1;
	}

	if ((fp = g_fopen(out_file, "wb")) == NULL) {
		FILE_OP_ERROR(out_file, "fopen");
		return -1;
	}

	/* chmod for security */
	if (change_file_mode_rw(fp, out_file) < 0) {
		FILE_OP_ERROR(out_file, "chmod");
		g_warning(_("can't change file mode\n"));
	}

	/* write headers */
	if (compose_write_headers(compose, fp, header_encoding, body_encoding,
				  encoding) < 0) {
		g_warning("can't write headers\n");
		fclose(fp);
		g_unlink(out_file);
		return -1;
	}

	/* write first part headers for multipart MIME */
	if (compose->attach_list) {
		if ((compose->flags & COMPOSE_OUT_MIME_PROLOG) != 0)
			fputs("This is a multi-part message in MIME format.\n",
			      fp);

		fprintf(fp, "\n--%s\n", compose->boundary);
		fprintf(fp, "Content-Type: text/plain; charset=%s\n",
			body_encoding);
		if ((compose->flags & COMPOSE_OUT_DISPOSITION_INLINE) != 0)
			fprintf(fp, "Content-Disposition: inline\n");
		fprintf(fp, "Content-Transfer-Encoding: %s\n",
			procmime_get_encoding_str(encoding));
		fputc('\n', fp);
	}

	/* write body */
	len = strlen(compose->body_text);
	if (encoding == ENC_BASE64) {
		gchar outbuf[B64_BUFFSIZE];
		gint i, l;

		for (i = 0; i < len; i += B64_LINE_SIZE) {
			l = MIN(B64_LINE_SIZE, len - i);
			base64_encode(outbuf, (guchar *)compose->body_text + i,
				      l);
			fputs(outbuf, fp);
			fputc('\n', fp);
		}
	} else if (encoding == ENC_QUOTED_PRINTABLE) {
		gchar *outbuf;
		size_t outlen;

		outbuf = g_malloc(len * 4);
		qp_encode_line(outbuf, (guchar *)compose->body_text);
		outlen = strlen(outbuf);
		if (fwrite(outbuf, sizeof(gchar), outlen, fp) != outlen) {
			FILE_OP_ERROR(out_file, "fwrite");
			fclose(fp);
			g_unlink(out_file);
			g_free(outbuf);
			return -1;
		}
		g_free(outbuf);
	} else if (fwrite(compose->body_text, sizeof(gchar), len, fp) != len) {
		FILE_OP_ERROR(out_file, "fwrite");
		fclose(fp);
		g_unlink(out_file);
		return -1;
	}

	if (compose->attach_list)
		compose_write_attach(compose, fp, header_encoding);

	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(out_file, "fclose");
		g_unlink(out_file);
		return -1;
	}

	/* rewrite file here */
	if (compose->postprocess_func)
		if (compose->postprocess_func
			(compose, out_file, compose->data) < 0)
			return -1;

	uncanonicalize_file_replace(out_file);

	return 0;
}

gint compose_redirect_write_to_file(ComposeInfo *compose, const gchar *file,
				    GError *error)
{
	FILE *fp;
	FILE *fdest;
	size_t len;
	gchar buf[BUFFSIZE];

	g_return_val_if_fail(file != NULL, -1);
	g_return_val_if_fail(compose->account != NULL, -1);
	g_return_val_if_fail(compose->account->address != NULL, -1);
	g_return_val_if_fail(compose->mode == COMPOSE_REDIRECT, -1);
	g_return_val_if_fail(compose->targetinfo != NULL, -1);

	if ((fp = procmsg_open_message(compose->targetinfo)) == NULL)
		return -1;

	if ((fdest = g_fopen(file, "wb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		fclose(fp);
		return -1;
	}

	if (change_file_mode_rw(fdest, file) < 0) {
		FILE_OP_ERROR(file, "chmod");
		g_warning(_("can't change file mode\n"));
	}

	while (procheader_get_one_field(buf, sizeof(buf), fp, NULL) == 0) {
		if (g_ascii_strncasecmp(buf, "Return-Path:",
					strlen("Return-Path:")) == 0 ||
		    g_ascii_strncasecmp(buf, "Delivered-To:",
					strlen("Delivered-To:")) == 0 ||
		    g_ascii_strncasecmp(buf, "Received:",
					strlen("Received:")) == 0 ||
		    g_ascii_strncasecmp(buf, "Subject:",
					strlen("Subject:")) == 0 ||
		    g_ascii_strncasecmp(buf, "X-UIDL:",
					strlen("X-UIDL:")) == 0)
			continue;

		if (fputs(buf, fdest) == EOF)
			goto error;

#if 0
		if (g_ascii_strncasecmp(buf, "From:", strlen("From:")) == 0) {
			fputs("\n (by way of ", fdest);
			if (compose->account->name) {
				compose_convert_header(compose,
						       buf, sizeof(buf),
						       compose->account->name,
						       strlen(" (by way of "),
						       FALSE, NULL);
				fprintf(fdest, "%s <%s>", buf,
					compose->account->address);
			} else
				fputs(compose->account->address, fdest);
			fputs(")", fdest);
		}
#endif

		if (fputs("\n", fdest) == EOF)
			goto error;
	}

	compose_redirect_write_headers(compose, fdest);

	while ((len = fread(buf, sizeof(gchar), sizeof(buf), fp)) > 0) {
		if (fwrite(buf, sizeof(gchar), len, fdest) != len) {
			FILE_OP_ERROR(file, "fwrite");
			goto error;
		}
	}

	fclose(fp);
	if (fclose(fdest) == EOF) {
		FILE_OP_ERROR(file, "fclose");
		g_unlink(file);
		return -1;
	}

	return 0;
error:
	fclose(fp);
	fclose(fdest);
	g_unlink(file);

	return -1;
}

gint compose_remove_reedit_target(ComposeInfo *compose)
{
	FolderItem *item;
	MsgInfo *msginfo = compose->targetinfo;

	g_return_val_if_fail(compose->mode == COMPOSE_REEDIT, -1);
	if (!msginfo) return -1;

	item = msginfo->folder;
	g_return_val_if_fail(item != NULL, -1);

	folder_item_scan(item);
	if (procmsg_msg_exist(msginfo) &&
	    (item->stype == F_DRAFT || item->stype == F_QUEUE)) {
		if (folder_item_remove_msg(item, msginfo) < 0) {
			g_warning(_("can't remove the old message\n"));
			return -1;
		}
	}

	return 0;
}

gint compose_queue(ComposeInfo *compose, const gchar *file)
{
	FolderItem *queue;
	gchar *tmp;
	FILE *fp, *src_fp;
	GSList *cur;
	gchar buf[BUFFSIZE];
	gint num;
	MsgFlags flag = {0, 0};

	debug_print(_("queueing message...\n"));
	g_return_val_if_fail(compose->to_list != NULL ||
			     compose->newsgroup_list != NULL,
			     -1);
	g_return_val_if_fail(compose->account != NULL, -1);

	tmp = g_strdup_printf("%s%cqueue.%p", get_tmp_dir(),
			      G_DIR_SEPARATOR, compose);
	if ((fp = g_fopen(tmp, "wb")) == NULL) {
		FILE_OP_ERROR(tmp, "fopen");
		g_free(tmp);
		return -1;
	}
	if ((src_fp = g_fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		fclose(fp);
		g_unlink(tmp);
		g_free(tmp);
		return -1;
	}
	if (change_file_mode_rw(fp, tmp) < 0) {
		FILE_OP_ERROR(tmp, "chmod");
		g_warning(_("can't change file mode\n"));
	}

	/* queueing variables */
	fprintf(fp, "AF:\n");
	fprintf(fp, "NF:0\n");
	fprintf(fp, "PS:10\n");
	fprintf(fp, "SRH:1\n");
	fprintf(fp, "SFN:\n");
	fprintf(fp, "DSR:\n");
	if (compose->msgid)
		fprintf(fp, "MID:<%s>\n", compose->msgid);
	else
		fprintf(fp, "MID:\n");
	fprintf(fp, "CFG:\n");
	fprintf(fp, "PT:0\n");
	fprintf(fp, "S:%s\n", compose->account->address);
	fprintf(fp, "RQ:\n");
	if (compose->account->smtp_server)
		fprintf(fp, "SSV:%s\n", compose->account->smtp_server);
	else
		fprintf(fp, "SSV:\n");
	if (compose->account->nntp_server)
		fprintf(fp, "NSV:%s\n", compose->account->nntp_server);
	else
		fprintf(fp, "NSV:\n");
	fprintf(fp, "SSH:\n");
	if (compose->to_list) {
		fprintf(fp, "R:<%s>", (gchar *)compose->to_list->data);
		for (cur = compose->to_list->next; cur != NULL;
		     cur = cur->next)
			fprintf(fp, ",<%s>", (gchar *)cur->data);
		fprintf(fp, "\n");
	} else
		fprintf(fp, "R:\n");
	/* Sylpheed account ID */
	fprintf(fp, "AID:%d\n", compose->account->account_id);
	fprintf(fp, "\n");

	while (fgets(buf, sizeof(buf), src_fp) != NULL) {
		if (fputs(buf, fp) == EOF) {
			FILE_OP_ERROR(tmp, "fputs");
			fclose(fp);
			fclose(src_fp);
			g_unlink(tmp);
			g_free(tmp);
			return -1;
		}
	}

	fclose(src_fp);
	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(tmp, "fclose");
		g_unlink(tmp);
		g_free(tmp);
		return -1;
	}

	queue = account_get_special_folder(compose->account, F_QUEUE);
	if (!queue) {
		g_warning(_("can't find queue folder\n"));
		g_unlink(tmp);
		g_free(tmp);
		return -1;
	}
	folder_item_scan(queue);
	if ((num = folder_item_add_msg(queue, tmp, &flag, TRUE)) < 0) {
		g_warning(_("can't queue the message\n"));
		g_unlink(tmp);
		g_free(tmp);
		return -1;
	}
	g_free(tmp);

	return 0;
}

static void compose_write_attach(ComposeInfo *compose, FILE *fp,
				 const gchar *header_encoding)
{
	ComposeAttachInfo *ainfo;
	GList *cur;
	FILE *attach_fp;
	gint len;
	EncodingType encoding;

	for (cur = compose->attach_list; cur != NULL; cur = cur->next) {
		ainfo = (ComposeAttachInfo *)cur->data;

		if ((attach_fp = g_fopen(ainfo->file, "rb")) == NULL) {
			g_warning("Can't open file %s\n", ainfo->file);
			continue;
		}

		fprintf(fp, "\n--%s\n", compose->boundary);

		encoding = ainfo->encoding;

		if (!g_ascii_strncasecmp(ainfo->content_type, "message/", 8)) {
			fprintf(fp, "Content-Type: %s\n", ainfo->content_type);
			fprintf(fp, "Content-Disposition: inline\n");

			/* message/... shouldn't be encoded */
			if (encoding == ENC_QUOTED_PRINTABLE ||
			    encoding == ENC_BASE64)
				encoding = ENC_8BIT;
		} else {
			if (prefs_common.mime_fencoding_method ==
			    FENC_RFC2231) {
				gchar *param;

				param = compose_convert_filename
					(compose, ainfo->name, "name",
					 header_encoding);
				fprintf(fp, "Content-Type: %s;\n"
					    "%s\n",
					ainfo->content_type, param);
				g_free(param);
				param = compose_convert_filename
					(compose, ainfo->name, "filename",
					 header_encoding);
				fprintf(fp, "Content-Disposition: attachment;\n"
					    "%s\n", param);
				g_free(param);
			} else {
				gchar filename[BUFFSIZE];

				compose_convert_header(compose, filename,
						       sizeof(filename),
						       ainfo->name, 12, FALSE,
						       header_encoding);
				fprintf(fp, "Content-Type: %s;\n"
					    " name=\"%s\"\n",
					ainfo->content_type, filename);
				fprintf(fp, "Content-Disposition: attachment;\n"
					    " filename=\"%s\"\n", filename);
			}

			if ((compose->flags & COMPOSE_PROTECT_TRAILING_SPACE)
			    != 0) {
				if (encoding == ENC_7BIT)
					encoding = ENC_QUOTED_PRINTABLE;
				else if (encoding == ENC_8BIT)
					encoding = ENC_BASE64;
			}
		}

		fprintf(fp, "Content-Transfer-Encoding: %s\n\n",
			procmime_get_encoding_str(encoding));

		if (encoding == ENC_BASE64) {
			gchar inbuf[B64_LINE_SIZE], outbuf[B64_BUFFSIZE];
			FILE *tmp_fp = attach_fp;
			gchar *tmp_file = NULL;
			ContentType content_type;

			content_type =
				procmime_scan_mime_type(ainfo->content_type);
			if (content_type == MIME_TEXT ||
			    content_type == MIME_TEXT_HTML ||
			    content_type == MIME_MESSAGE_RFC822) {
				tmp_file = get_tmp_file();
				if (canonicalize_file(ainfo->file, tmp_file) < 0) {
					g_free(tmp_file);
					fclose(attach_fp);
					continue;
				}
				if ((tmp_fp = g_fopen(tmp_file, "rb")) == NULL) {
					FILE_OP_ERROR(tmp_file, "fopen");
					g_unlink(tmp_file);
					g_free(tmp_file);
					fclose(attach_fp);
					continue;
				}
			}

			while ((len = fread(inbuf, sizeof(gchar),
					    B64_LINE_SIZE, tmp_fp))
			       == B64_LINE_SIZE) {
				base64_encode(outbuf, (guchar *)inbuf,
					      B64_LINE_SIZE);
				fputs(outbuf, fp);
				fputc('\n', fp);
			}
			if (len > 0 && feof(tmp_fp)) {
				base64_encode(outbuf, (guchar *)inbuf, len);
				fputs(outbuf, fp);
				fputc('\n', fp);
			}

			if (tmp_file) {
				fclose(tmp_fp);
				g_unlink(tmp_file);
				g_free(tmp_file);
			}
		} else if (encoding == ENC_QUOTED_PRINTABLE) {
			gchar inbuf[BUFFSIZE], outbuf[BUFFSIZE * 4];

			while (fgets(inbuf, sizeof(inbuf), attach_fp) != NULL) {
				qp_encode_line(outbuf, (guchar *)inbuf);
				fputs(outbuf, fp);
			}
		} else {
			gchar buf[BUFFSIZE];

			while (fgets(buf, sizeof(buf), attach_fp) != NULL) {
				strcrchomp(buf);
				fputs(buf, fp);
			}
		}

		fclose(attach_fp);
	}

	fprintf(fp, "\n--%s--\n", compose->boundary);
}

#define QUOTE_IF_REQUIRED(out, str)			\
{							\
	if (*str != '"' && strpbrk(str, ",.[]<>")) {	\
		gchar *__tmp;				\
		gint len;				\
							\
		len = strlen(str) + 3;			\
		Xalloca(__tmp, len, return -1);		\
		g_snprintf(__tmp, len, "\"%s\"", str);	\
		out = __tmp;				\
	} else {					\
		Xstrdup_a(out, str, return -1);		\
	}						\
}

#define PUT_RECIPIENT_HEADER(header, str)				     \
{									     \
	if (*str != '\0') {						     \
		gchar *dest;						     \
									     \
		Xstrdup_a(dest, str, return -1);			     \
		g_strstrip(dest);					     \
		if (*dest != '\0') {					     \
			compose->to_list = address_list_append		     \
				(compose->to_list, dest);		     \
			compose_convert_header				     \
				(compose, buf, sizeof(buf), dest,	     \
				 strlen(header) + 2, TRUE, header_encoding); \
			fprintf(fp, "%s: %s\n", header, buf);		     \
		}							     \
	}								     \
}

#define IS_IN_CUSTOM_HEADER(header) \
	(compose->account->add_customhdr && \
	 custom_header_find(compose->account->customhdr_list, header) != NULL)

static gint compose_write_headers(ComposeInfo *compose, FILE *fp,
				  const gchar *header_encoding,
				  const gchar *body_encoding,
				  EncodingType encoding)
{
	gchar buf[BUFFSIZE];
	const gchar *entry_str;
	gchar *str;
	gchar *name;

	g_return_val_if_fail(fp != NULL, -1);
	g_return_val_if_fail(header_encoding != NULL, -1);
	g_return_val_if_fail(compose->account != NULL, -1);
	g_return_val_if_fail(compose->account->address != NULL, -1);

	/* Date */
	if (compose->account->add_date) {
		get_rfc822_date(buf, sizeof(buf));
		fprintf(fp, "Date: %s\n", buf);
	}

	/* From */
	if (compose->account->name && *compose->account->name) {
		compose_convert_header
			(compose, buf, sizeof(buf), compose->account->name,
			 strlen("From: "), TRUE, header_encoding);
		QUOTE_IF_REQUIRED(name, buf);
		fprintf(fp, "From: %s <%s>\n",
			name, compose->account->address);
	} else
		fprintf(fp, "From: %s\n", compose->account->address);

	slist_free_strings(compose->to_list);
	g_slist_free(compose->to_list);
	compose->to_list = NULL;

	if (compose->to) {
		PUT_RECIPIENT_HEADER("To", compose->to);
	}

	/* Newsgroups */
	slist_free_strings(compose->newsgroup_list);
	g_slist_free(compose->newsgroup_list);
	compose->newsgroup_list = NULL;

	if (compose->newsgroups) {
		Xstrdup_a(str, compose->newsgroups, return -1);
		g_strstrip(str);
		remove_space(str);
		if (*str != '\0') {
			compose->newsgroup_list =
				newsgroup_list_append
					(compose->newsgroup_list, str);
			compose_convert_header(compose, buf, sizeof(buf), str,
					       strlen("Newsgroups: "),
					       FALSE, header_encoding);
			fprintf(fp, "Newsgroups: %s\n", buf);
		}
	}

	if (compose->cc) {
		PUT_RECIPIENT_HEADER("Cc", compose->cc);
	}

	if (compose->bcc) {
		PUT_RECIPIENT_HEADER("Bcc", compose->bcc);
	}

	if (!(compose->flags & COMPOSE_DRAFT_MODE) &&
	    !compose->to_list && !compose->newsgroup_list) {
		g_warning("no recepients\n");
		return -1;
	}

	/* Subject */
	if (compose->subject && !IS_IN_CUSTOM_HEADER("Subject")) {
		Xstrdup_a(str, compose->subject, return -1);
		g_strstrip(str);
		if (*str != '\0') {
			compose_convert_header(compose, buf, sizeof(buf), str,
					       strlen("Subject: "), FALSE,
					       header_encoding);
			fprintf(fp, "Subject: %s\n", buf);
		}
	}

	/* Message-ID */
	if (compose->account->gen_msgid) {
		compose_generate_msgid(compose, buf, sizeof(buf));
		fprintf(fp, "Message-Id: <%s>\n", buf);
		compose->msgid = g_strdup(buf);
	}

	/* In-Reply-To */
	if (compose->inreplyto && compose->to_list)
		fprintf(fp, "In-Reply-To: <%s>\n", compose->inreplyto);

	/* References */
	if (compose->references)
		fprintf(fp, "References: %s\n", compose->references);

	/* Followup-To */
	if (compose->followup_to && !IS_IN_CUSTOM_HEADER("Followup-To")) {
		Xstrdup_a(str, compose->followup_to, return -1);
		g_strstrip(str);
		remove_space(str);
		if (*str != '\0') {
			compose_convert_header(compose, buf, sizeof(buf), str,
					       strlen("Followup-To: "),
					       FALSE, header_encoding);
			fprintf(fp, "Followup-To: %s\n", buf);
		}
	}

	/* Reply-To */
	if (compose->replyto && !IS_IN_CUSTOM_HEADER("Reply-To")) {
		Xstrdup_a(str, compose->replyto, return -1);
		g_strstrip(str);
		if (*str != '\0') {
			compose_convert_header(compose, buf, sizeof(buf), str,
					       strlen("Reply-To: "),
					       TRUE, header_encoding);
			fprintf(fp, "Reply-To: %s\n", buf);
		}
	}

	/* Organization */
	if (compose->account->organization &&
	    !IS_IN_CUSTOM_HEADER("Organization")) {
		compose_convert_header(compose, buf, sizeof(buf),
				       compose->account->organization,
				       strlen("Organization: "), FALSE,
				       header_encoding);
		fprintf(fp, "Organization: %s\n", buf);
	}

#if 0
	/* Program version and system info */
	if (compose->to_list && !IS_IN_CUSTOM_HEADER("X-Mailer")) {
		fprintf(fp, "X-Mailer: %s (GTK+ %d.%d.%d; %s)\n",
			prog_version,
			gtk_major_version, gtk_minor_version, gtk_micro_version,
			TARGET_ALIAS);
	}
	if (compose->newsgroup_list && !IS_IN_CUSTOM_HEADER("X-Newsreader")) {
		fprintf(fp, "X-Newsreader: %s (GTK+ %d.%d.%d; %s)\n",
			prog_version,
			gtk_major_version, gtk_minor_version, gtk_micro_version,
			TARGET_ALIAS);
	}
#endif

	/* custom headers */
	if (compose->account->add_customhdr) {
		GSList *cur;

		for (cur = compose->account->customhdr_list; cur != NULL;
		     cur = cur->next) {
			CustomHeader *chdr = (CustomHeader *)cur->data;

			if (g_ascii_strcasecmp(chdr->name, "Date") != 0 &&
			    g_ascii_strcasecmp(chdr->name, "From") != 0 &&
			    g_ascii_strcasecmp(chdr->name, "To") != 0 &&
			 /* g_ascii_strcasecmp(chdr->name, "Sender") != 0 && */
			    g_ascii_strcasecmp(chdr->name, "Message-Id") != 0 &&
			    g_ascii_strcasecmp(chdr->name, "In-Reply-To") != 0 &&
			    g_ascii_strcasecmp(chdr->name, "References") != 0 &&
			    g_ascii_strcasecmp(chdr->name, "Mime-Version") != 0 &&
			    g_ascii_strcasecmp(chdr->name, "Content-Type") != 0 &&
			    g_ascii_strcasecmp(chdr->name, "Content-Transfer-Encoding") != 0) {
				compose_convert_header
					(compose, buf, sizeof(buf),
					 chdr->value ? chdr->value : "",
					 strlen(chdr->name) + 2, FALSE,
					 header_encoding);
				fprintf(fp, "%s: %s\n", chdr->name, buf);
			}
		}
	}

	/* MIME */
	fprintf(fp, "Mime-Version: 1.0\n");
	if (compose->attach_list) {
		compose->boundary = generate_mime_boundary(NULL);
		fprintf(fp,
			"Content-Type: multipart/mixed;\n"
			" boundary=\"%s\"\n", compose->boundary);
	} else {
		fprintf(fp, "Content-Type: text/plain; charset=%s\n",
			body_encoding);
		if ((compose->flags & COMPOSE_OUT_DISPOSITION_INLINE) != 0)
			fprintf(fp, "Content-Disposition: inline\n");
		fprintf(fp, "Content-Transfer-Encoding: %s\n",
			procmime_get_encoding_str(encoding));
	}

	/* X-Sylpheed header */
	if ((compose->flags & COMPOSE_DRAFT_MODE) != 0)
		fprintf(fp, "X-Sylpheed-Account-Id: %d\n",
			compose->account->account_id);

	/* separator between header and body */
	fputs("\n", fp);

	return 0;
}

static gint compose_redirect_write_headers(ComposeInfo *compose, FILE *fp)
{
	gchar buf[BUFFSIZE];
	gchar *str;
	const gchar *header_encoding = NULL;

	g_return_val_if_fail(fp != NULL, -1);
	g_return_val_if_fail(compose->account != NULL, -1);
	g_return_val_if_fail(compose->account->address != NULL, -1);

	/* Resent-Date */
	get_rfc822_date(buf, sizeof(buf));
	fprintf(fp, "Resent-Date: %s\n", buf);

	/* Resent-From */
	if (compose->account->name) {
		compose_convert_header
			(compose, buf, sizeof(buf), compose->account->name,
			 strlen("Resent-From: "), TRUE, NULL);
		fprintf(fp, "Resent-From: %s <%s>\n",
			buf, compose->account->address);
	} else
		fprintf(fp, "Resent-From: %s\n", compose->account->address);

	slist_free_strings(compose->to_list);
	g_slist_free(compose->to_list);
	compose->to_list = NULL;

	/* Resent-To */
	if (compose->to) {
		PUT_RECIPIENT_HEADER("Resent-To", compose->to);
	}
	if (compose->cc) {
		PUT_RECIPIENT_HEADER("Resent-Cc", compose->cc);
	}
	if (compose->bcc) {
		PUT_RECIPIENT_HEADER("Bcc", compose->bcc);
	}

	slist_free_strings(compose->newsgroup_list);
	g_slist_free(compose->newsgroup_list);
	compose->newsgroup_list = NULL;

	/* Newsgroups */
	if (compose->newsgroups) {
		Xstrdup_a(str, compose->newsgroups, return -1);
		g_strstrip(str);
		remove_space(str);
		if (*str != '\0') {
			compose->newsgroup_list =
				newsgroup_list_append
					(compose->newsgroup_list, str);
			compose_convert_header(compose, buf, sizeof(buf), str,
					       strlen("Newsgroups: "),
					       FALSE, NULL);
			fprintf(fp, "Newsgroups: %s\n", buf);
		}
	}

	if (!compose->to_list && !compose->newsgroup_list)
		return -1;

	/* Subject */
	if (compose->subject) {
		Xstrdup_a(str, compose->subject, return -1);
		g_strstrip(str);
		if (*str != '\0') {
			compose_convert_header(compose, buf, sizeof(buf), str,
					       strlen("Subject: "), FALSE,
					       NULL);
			fprintf(fp, "Subject: %s\n", buf);
		}
	}

	/* Resent-Message-Id */
	if (compose->account->gen_msgid) {
		compose_generate_msgid(compose, buf, sizeof(buf));
		fprintf(fp, "Resent-Message-Id: <%s>\n", buf);
		compose->msgid = g_strdup(buf);
	}

	/* Followup-To */
	if (compose->followup_to) {
		Xstrdup_a(str, compose->followup_to, return -1);
		g_strstrip(str);
		remove_space(str);
		if (*str != '\0') {
			compose_convert_header(compose, buf, sizeof(buf), str,
					       strlen("Followup-To: "),
					       FALSE, NULL);
			fprintf(fp, "Followup-To: %s\n", buf);
		}
	}

	/* Resent-Reply-To */
	if (compose->replyto) {
		Xstrdup_a(str, compose->replyto, return -1);
		g_strstrip(str);
		if (*str != '\0') {
			compose_convert_header
				(compose, buf, sizeof(buf), str,
				 strlen("Resent-Reply-To: "), TRUE,
				 NULL);
			fprintf(fp, "Resent-Reply-To: %s\n", buf);
		}
	}

	fputs("\n", fp);

	return 0;
}

#undef IS_IN_CUSTOM_HEADER

static void compose_convert_header(ComposeInfo *compose, gchar *dest, gint len,
				   const gchar *src, gint header_len,
				   gboolean addr_field, const gchar *encoding)
{
	gchar *src_;

	g_return_if_fail(src != NULL);
	g_return_if_fail(dest != NULL);

	if (len < 1) return;

	if (addr_field)
		src_ = normalize_address_field(src);
	else
		src_ = g_strdup(src);
	g_strchomp(src_);
	if (!encoding)
		encoding = compose->header_encoding;

	conv_encode_header(dest, len, src_, header_len, addr_field, encoding);

	g_free(src_);
}

static gchar *compose_convert_filename(ComposeInfo *compose, const gchar *src,
				       const gchar *param_name,
				       const gchar *encoding)
{
	gchar *str;

	g_return_val_if_fail(src != NULL, NULL);

	if (!encoding)
		encoding = compose->header_encoding;

	str = conv_encode_filename(src, param_name, encoding);

	return str;
}

void compose_generate_msgid(ComposeInfo *compose, gchar *buf, gint len)
{
	struct tm *lt;
	time_t t;
	gchar *addr;

	t = time(NULL);
	lt = localtime(&t);

	if (compose->account && compose->account->address &&
	    *compose->account->address) {
		if (strchr(compose->account->address, '@'))
			addr = g_strdup(compose->account->address);
		else
			addr = g_strconcat(compose->account->address, "@",
					   get_domain_name(), NULL);
	} else
		addr = g_strconcat(g_get_user_name(), "@", get_domain_name(),
				   NULL);

	g_snprintf(buf, len, "%04d%02d%02d%02d%02d%02d.%08x.%s",
		   lt->tm_year + 1900, lt->tm_mon + 1,
		   lt->tm_mday, lt->tm_hour,
		   lt->tm_min, lt->tm_sec,
		   g_random_int(), addr);

	debug_print(_("generated Message-ID: %s\n"), buf);

	g_free(addr);
}
