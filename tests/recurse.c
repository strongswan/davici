/*
 * Copyright (C) 2017 onway ag
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
#include <errno.h>

static void echocb(struct tester *t, int fd)
{
	char buf[2048];
	uint32_t len;

	len = tester_read_cmdreq(fd, "echoreq");
	assert(len < sizeof(buf));
	assert(read(fd, buf, len) == len);
	tester_write_cmdres(fd, buf, len);
}

static int kv_cb(struct davici_response *res, void *user)
{
	int *i = user;
	char buf[64];
	int err, v;

	err = davici_get_value_str(res, buf, sizeof(buf));
	if (err < 0)
	{
		return err;
	}
	switch ((*i)++)
	{
		case 1:
			assert(davici_name_strcmp(res, "key") == 0);
			assert(strcmp(buf, "value") == 0);
			break;
		case 3:
		case 4:
		case 5:
			assert(davici_name_strcmp(res, "list") == 0);
			assert(davici_value_escanf(res, "item%d", &v) >= 0);
			assert(davici_value_escanf(res, "item%d%d", &v, &v) == -EBADMSG);
			assert(davici_value_escanf(res, "%4c%d", &buf, &v) >= 0);
			assert(davici_value_escanf(res, "%4c", &buf) == -EBADMSG);
			assert(v == (*i) - 3);
			break;
		case 7:
			assert(davici_name_strcmp(res, "v") == 0);
			assert(strcmp(buf, "w") == 0);
			break;
		case 8:
			assert(davici_name_strcmp(res, "x") == 0);
			assert(strcmp(buf, "y") == 0);
			break;
		default:
			assert(0);
			break;
	}
	return 0;
}

static int section_cb(struct davici_response *res, void *user)
{
	int *i = user;
	int err;

	switch ((*i)++)
	{
		case 0:
			assert(davici_name_strcmp(res, "section") == 0);
			break;
		case 2:
			assert(davici_name_strcmp(res, "subsection") == 0);
			break;
		case 6:
			assert(davici_name_strcmp(res, "subsub") == 0);
			break;
		default:
			assert(0);
			break;
	}
	err = davici_recurse(res, section_cb, kv_cb, kv_cb, i);
	if (err < 0)
	{
		return err;
	}
	return 0;
}

static void reqcb(struct davici_conn *c, int err, const char *name,
				  struct davici_response *res, void *user)
{
	struct tester *t = user;
	int i = 0;

	assert(err >= 0);
	assert(davici_recurse(res, section_cb, kv_cb, kv_cb, &i) >= 0);

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
		davici_section_start(r, "subsection");
			davici_list_start(r, "list");
				davici_list_itemf(r, "%s", "item1");
				davici_list_itemf(r, "%s", "item2");
				davici_list_itemf(r, "%s", "item3");
			davici_list_end(r);
			davici_section_start(r, "subsub");
				davici_kvf(r, "v", "w");
				davici_kvf(r, "x", "y");
			davici_section_end(r);
		davici_section_end(r);
	davici_section_end(r);

	assert(davici_queue(c, r, reqcb, t) >= 0);

	tester_runio(t, c);
	davici_disconnect(c);
	tester_cleanup(t);
	return 0;
}
