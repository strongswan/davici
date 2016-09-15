/*
 * Copyright (C) 2016 Tobias Brunner
 * HSR Hochschule fuer Technik Rapperswil
 *
 * Copyright (C) 2015 CloudGuard Software AG
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */

#include "config.h"
#include "tester.h"

#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#if !defined(HAVE_FMEMOPEN) && defined(HAVE_FUNOPEN)

static size_t min(size_t a, size_t b)
{
	return a < b ? a : b;
}

typedef struct {
	char *buf;
	size_t size;
} cookie_t;

static int fmemwrite(cookie_t *cookie, const char *buf, int size)
{
	int len;

	len = min(size, cookie->size);
	memcpy(cookie->buf, buf, len);
	cookie->buf += len;
	cookie->size -= len;
	return len;
}

static int fmemclose(cookie_t *cookie)
{
	if (cookie->size)
	{
		*cookie->buf = '\0';
	}
	free(cookie);
	return 0;
}

static FILE *fmemopen(void *buf, size_t size, const char *mode)
{
	cookie_t *cookie;

	cookie = calloc(1, sizeof(*cookie));
	cookie->buf = buf;
	cookie->size = size;
	return funopen(cookie, NULL, (void*)fmemwrite, NULL, (void*)fmemclose);
}
#endif /* !HAVE_FMEMOPEN && HAVE_FUNOPEN */

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
