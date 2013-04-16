#ifndef __BUGMESSAGE_H__
#define __BUGMESSAGE_H__

#include "common.h"
#include "mymemory.h"

struct bug_message {
	bug_message(const char *str) {
		const char *fmt = "  [BUG] %s\n";
		msg = (char *)snapshot_malloc(strlen(fmt) + strlen(str));
		sprintf(msg, fmt, str);
	}
	~bug_message() { if (msg) snapshot_free(msg); }

	char *msg;
	void print() { model_print("%s", msg); }

	SNAPSHOTALLOC
};

#endif /* __BUGMESSAGE_H__ */
