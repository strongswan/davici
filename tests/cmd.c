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
#ifdef _WIN32
#include <WinSock2.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#endif
#include <string.h>
#include <stdint.h>

static char huge[4096];

static void echocb(struct tester *t, davici_fd fd)
{
	char buf[sizeof(huge) * 2];
	uint32_t len;

	len = tester_read_cmdreq(fd, "echoreq");
	assert(len < sizeof(buf));
	assert(recv(fd, buf, len, 0) == len);
	tester_write_cmdres(fd, buf, len);
}

static void reqcb(struct davici_conn *c, int err, const char *name,
				  struct davici_response *res, void *user)
{
	struct tester *t = user;
	char buf[64];
	const char *h;
	unsigned int len, j;
	const void *v;
	int ret, i;

	assert(err >= 0);
	assert(davici_get_level(res) == 0);

	for (i = 0;; i++)
	{
		ret = davici_parse(res);
		switch (i)
		{
			case 0:
				assert(ret == DAVICI_SECTION_START);
				assert(strcmp(davici_get_name(res), "section") == 0);
				assert(davici_get_level(res) == 1);
				break;
			case 1:
				assert(ret == DAVICI_KEY_VALUE);
				assert(strcmp(davici_get_name(res), "key") == 0);
				v = davici_get_value(res, &len);
				assert(v);
				assert(len == strlen("value"));
				assert(memcmp(v, "value", len) == 0);
				assert(davici_get_level(res) == 1);
				break;
			case 2:
				assert(ret == DAVICI_LIST_START);
				assert(strcmp(davici_get_name(res), "list") == 0);
				assert(davici_name_strcmp(res, "list") == 0);
				assert(davici_name_strcmp(res, "a") > 0);
				assert(davici_name_strcmp(res, "x") < 0);
				assert(davici_get_level(res) == 2);
				break;
			case 3:
				assert(ret == DAVICI_LIST_ITEM);
				assert(davici_get_value_str(res, buf,
											sizeof(buf)) == strlen("item"));
				assert(strcmp(buf, "item") == 0);
				assert(davici_value_strcmp(res, "item") == 0);
				assert(davici_value_strcmp(res, "a") > 0);
				assert(davici_value_strcmp(res, "x") < 0);
				assert(davici_value_strcmp(res, "itemxx") < 0);
				assert(davici_get_level(res) == 2);
				break;
			case 4:
				assert(ret == DAVICI_LIST_END);
				assert(davici_get_level(res) == 1);
				break;
			case 5:
				assert(ret == DAVICI_KEY_VALUE);
				assert(davici_get_level(res) == 1);
				assert(davici_name_strcmp(res, "huge") == 0);
				h = davici_get_value(res, &len);
				assert(len == sizeof(huge));
				for (j = 0; j < len; j++)
				{
					assert(h[i] == 'h');
				}
				break;
			case 6:
				assert(ret == DAVICI_SECTION_END);
				assert(davici_get_level(res) == 0);
				break;
			case 7:
				assert(ret == DAVICI_END);
				assert(davici_get_level(res) == 0);
				tester_complete(t);
				return;
			default:
				assert(0);
				break;
		}
	}
}

int main(int argc, char *argv[])
{
	struct tester *t;
	struct davici_conn *c;
	struct davici_request *r;

#ifdef _WIN32
	WSADATA wsd;
	WSAStartup(MAKEWORD(2,2), &wsd);
	t = tester_create( echocb );
	assert(davici_connect_tcp(tester_getport(t),
							  tester_davici_iocb, t, &c) >= 0);
#else
	t = tester_create( echocb );
	assert(davici_connect_unix(tester_getpath(t),
							   tester_davici_iocb, t, &c) >= 0);
#endif
	assert(davici_new_cmd("tocancel", &r) >= 0);
	davici_cancel(r);
	assert(davici_new_cmd("one", &r) >= 0);
	assert(davici_queue(c, r, reqcb, t) >= 0);
	assert(davici_new_cmd("two", &r) >= 0);
	assert(davici_queue(c, r, reqcb, t) >= 0);
	assert(davici_new_cmd("three", &r) >= 0);
	assert(davici_queue(c, r, reqcb, t) >= 0);
	davici_disconnect(c);

#ifdef _WIN32
	assert(davici_connect_tcp(tester_getport(t),
							  tester_davici_iocb, t, &c) >= 0);
#else
	assert(davici_connect_unix(tester_getpath(t),
							   tester_davici_iocb, t, &c) >= 0);
#endif
	assert(davici_new_cmd("echoreq", &r) >= 0);
	davici_section_start(r, "section");
	davici_kvf(r, "key", "%s", "value");
	davici_list_start(r, "list");
	davici_list_itemf(r, "%s", "item");
	davici_list_end(r);
	memset(huge, 'h', sizeof(huge));
	davici_kv(r, "huge", huge, sizeof(huge));
	davici_section_end(r);

	assert(davici_queue(c, r, reqcb, t) >= 0);

	tester_runio(t, c);
	davici_disconnect(c);
	tester_cleanup(t);
	return 0;
}
