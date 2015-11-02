/*
 * Copyright (c) 2015 CloudGuard Software AG. All rights reserved.
 */

#include "tester.h"

#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

static void echocb(struct tester *t, int fd)
{
	char buf[2048];
	uint32_t len;

	len = tester_read_cmdreq(fd, "echoreq");
	assert(len < sizeof(buf));
	assert(read(fd, buf, len) == len);
	tester_write_cmdres(fd, buf, len);
}

static void reqcb(struct davici_conn *c, int err, const char *name,
				  struct davici_response *res, void *user)
{
	struct tester *t = user;
	char buf[512];
	FILE *f;

	assert(err >= 0);

	f = fmemopen(buf, sizeof(buf), "w");
	assert(f);
	assert(davici_dump(res, name, " ", 0, 0, f));
	fclose(f);

	assert(strcmp(buf,
			"echoreq { section { key = value list [ item ] } }") == 0);

	f = fmemopen(buf, sizeof(buf), "w");
	assert(f);
	assert(davici_dump(res, name, "\n", 1, 2, f));
	fclose(f);

	assert(strcmp(buf,
			"  echoreq {\n"
			"    section {\n"
			"      key = value\n"
			"      list [\n"
			"        item\n"
			"      ]\n"
			"    }\n"
			"  }") == 0);

	tester_complete(t);
}

int main(int argc, char *argv[])
{
	struct tester *t;
	struct davici_conn *c;
	struct davici_request *r;

	t = tester_create(echocb);
	assert(davici_connect_unix(tester_getpath(t),
							   tester_davici_iocb, t, &c) >= 0);
	assert(davici_new_cmd("echoreq", &r) >= 0);

	davici_section_start(r, "section");
	davici_kvf(r, "key", "%s", "value");
	davici_list_start(r, "list");
	davici_list_itemf(r, "%s", "item");
	davici_list_end(r);
	davici_section_end(r);

	assert(davici_queue(c, r, reqcb, t) >= 0);

	tester_runio(t, c);
	davici_disconnect(c);
	tester_cleanup(t);
	return 0;
}
