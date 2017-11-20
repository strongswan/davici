/*
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

#include "tester.h"

#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

static const unsigned int request_count = 512;
static unsigned int seen = 0;

static void echocb(struct tester *t, davici_fd fd)
{
	char buf[32768];
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
	int ret;

	assert(err >= 0);

	while (1)
	{
		ret = davici_parse(res);
		assert(ret >= 0);
		if (ret == 0)
		{
			break;
		}
	}
	if (++seen == request_count)
	{
		tester_complete(t);
	}
	assert(seen <= request_count);
}

int main(int argc, char *argv[])
{
	struct tester *t;
	struct davici_conn *c;
	struct davici_request *r;
	int i, j, k;

	t = tester_create(echocb);
	assert(davici_connect_unix(tester_getpath(t),
							   tester_davici_iocb, t, &c) >= 0);

	assert(davici_queue_len(c) == 0);
	for (i = 0; i < request_count; i++)
	{
		assert(davici_new_cmd("echoreq", &r) >= 0);
		for (j = 0; j < 8; j++)
		{
			davici_section_start(r, "section");
			for (k = 0; k < 16; k++)
			{
				davici_kvf(r, "key", "value-%d", k);
			}
			davici_list_start(r, "list");
			for (k = 0; k < 32; k++)
			{
				davici_list_itemf(r, "item-%d", k);
			}
			davici_list_end(r);
			davici_section_end(r);
		}
		assert(davici_queue(c, r, reqcb, t) >= 0);
		assert(davici_queue_len(c) == i + 1);
	}

	tester_runio(t, c);
	assert(seen == request_count);
	assert(davici_queue_len(c) == 0);
	davici_disconnect(c);
	tester_cleanup(t);
	return 0;
}
