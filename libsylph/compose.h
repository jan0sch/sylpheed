
#ifndef __COMPOSE_H__
#define __COMPOSE_H__

#include <glib.h>

typedef struct _ComposeInfo		ComposeInfo;
typedef struct _ComposeAttachInfo	ComposeAttachInfo;

#include "procmsg.h"
#include "procmime.h"
#include "folder.h"
#include "prefs_account.h"
#include "codeconv.h"

typedef enum
{
	COMPOSE_REPLY             = 1,
	COMPOSE_REPLY_TO_SENDER   = 2,
	COMPOSE_REPLY_TO_ALL      = 3,
	COMPOSE_REPLY_TO_LIST     = 4,
	COMPOSE_FORWARD           = 5,
	COMPOSE_FORWARD_AS_ATTACH = 6,
	COMPOSE_NEW               = 7,
	COMPOSE_REDIRECT          = 8,
	COMPOSE_REEDIT            = 9,

	COMPOSE_WITH_QUOTE        = 1 << 16,

	COMPOSE_MODE_MASK         = 0xffff
} ComposeMode;

#define COMPOSE_MODE(mode)		((mode) & COMPOSE_MODE_MASK)
#define COMPOSE_QUOTE_MODE(mode)	((mode) & COMPOSE_WITH_QUOTE)

typedef enum
{
	COMPOSE_OUT_MIME_PROLOG        = 1 << 0,
	COMPOSE_OUT_DISPOSITION_INLINE = 1 << 1,
	COMPOSE_PROTECT_TRAILING_SPACE = 1 << 2,
	COMPOSE_DRAFT_MODE             = 1 << 3
} ComposeFlags;

typedef gint (*ComposePreFunc)	(ComposeInfo	*compose,
				 gpointer	 user_data);
typedef gint (*ComposePostFunc)	(ComposeInfo	*compose,
				 const gchar	*file,
				 gpointer	 user_data);

struct _ComposeInfo
{
	ComposeMode mode;
	ComposeFlags flags;

	/* reference of redirect or reedit */
	MsgInfo *targetinfo;
	/* reference of reply */
	MsgInfo *replyinfo;

	/* extra information */
	gchar	*ref_replyto;
	gchar	*ref_cc;
	gchar	*ref_bcc;
	gchar	*ref_newsgroups;
	gchar	*ref_followup_to;
	gchar	*ref_ml_post;

	/* composing message information */
	gchar	*to;
	gchar	*cc;
	gchar	*bcc;
	gchar	*replyto;
	gchar	*newsgroups;
	gchar	*followup_to;
	gchar	*subject;

	gchar	*inreplyto;
	gchar	*references;
	gchar	*msgid;

	gchar	*boundary;

	gchar	*header_encoding;
	gchar	*body_encoding;

	EncodingType ctencoding;

	gchar	*body_text;

	GList	*attach_list;

	/* actually sent address list */
	GSList	*to_list;
	GSList	*newsgroup_list;

	PrefsAccount *account;

	/* callback functions for pre/post-process */
	ComposePreFunc preprocess_func;
	ComposePostFunc postprocess_func;

	/* user data */
	gpointer data;
};

struct _ComposeAttachInfo
{
	gchar *file;
	gchar *content_type;
	EncodingType encoding;
	gchar *name;
	gsize size;
};

ComposeInfo *compose_info_new	(PrefsAccount	*account,
				 ComposeMode	 mode);
void compose_info_free		(ComposeInfo	*compose);

ComposeAttachInfo *compose_attach_info_new	(const gchar	*file,
						 const gchar	*content_type,
						 EncodingType	 encoding,
						 const gchar	*name,
						 gsize		 size);

void compose_attach_info_free	(ComposeAttachInfo	*ainfo);

gint compose_parse_header	(ComposeInfo	*compose,
				 MsgInfo	*msginfo);

gint compose_set_headers	(ComposeInfo	*compose,
				 const gchar	*to,
				 const gchar	*cc,
				 const gchar	*bcc,
				 const gchar	*replyto,
				 const gchar	*subject);
gint compose_set_news_headers	(ComposeInfo	*compose,
				 const gchar	*newsgroups,
				 const gchar	*followup_to);
gint compose_set_body		(ComposeInfo	*compose,
				 const gchar	*body);
gint compose_set_encoding	(ComposeInfo	*compose,
				 const gchar	*header_encoding,
				 const gchar	*body_encoding);
gint compose_set_attachments	(ComposeInfo	*compose,
				 GList		*attach_list);

gint compose_write_to_file	(ComposeInfo	*compose,
				 const gchar	*out_file,
				 GError		*error);

gint compose_redirect_write_to_file	(ComposeInfo	*compose,
					 const gchar	*out_file,
					 GError		*error);

gint compose_remove_reedit_target	(ComposeInfo	*compose);

gint compose_queue			(ComposeInfo	*compose,
					 const gchar	*file);

void compose_generate_msgid		(ComposeInfo	*compose,
					 gchar		*buf,
					 gint		 len);

void compose_set_preprocess_func	(ComposeInfo		*compose,
					 ComposePreFunc		 func);
void compose_set_postprocess_func	(ComposeInfo		*compose,
					 ComposePostFunc	 func);

#endif /* __COMPOSE_H__ */
