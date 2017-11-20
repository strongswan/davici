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

static void echocb(struct tester *t, davici_fd fd)
{
	char buf[2048];
	uint32_t len;

	len = tester_read_cmdreq(fd, "echoreq");
	assert(len < sizeof(buf));
	assert(read(fd, buf, len) == len);
	tester_write_cmdres(fd, buf, len);
}

static void verify(FILE *f, const char *exp)
{
	char buf[512];

	rewind(f);
	assert(fread(buf, strlen(exp), 1, f) == 1);
	assert(strncmp(buf, exp, strlen(exp)) == 0);
	assert(fread(buf, 1, 1, f) == 0);
	assert(feof(f));
}

static void reqcb(struct davici_conn *c, int err, const char *name,
				  struct davici_response *res, void *user)
{
	struct tester *t = user;
	FILE *f;

	assert(err >= 0);

	f = tmpfile();
	assert(f);
	assert(davici_dump(res, name, " ", 0, 0, f));
	verify(f, "echoreq { section { key = value list [ item ] } }");
	fclose(f);

	f = tmpfile();
	assert(f);
	assert(davici_dump(res, name, "\n", 1, 2, f));
	verify(f,
		"  echoreq {\n"
		"    section {\n"
		"      key = value\n"
		"      list [\n"
		"        item\n"
		"      ]\n"
		"    }\n"
		"  }");
	fclose(f);

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
