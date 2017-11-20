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
#endif
#include <string.h>
#include <stdint.h>

static unsigned int stream_count = 8;
static unsigned int seen = 0;

static void echocb(struct tester *t, davici_fd fd)
{
	char ev[] = {0x03,0x05,'c','o','u','n','t',0x00,0x01,0x00};
	static int state = 0;
	char buf[256];
	uint32_t len, i;

	switch (state++)
	{
		case 0:
			tester_read_eventreg(fd, "listevent");
			tester_write_eventconfirm(fd);
			break;
		case 1:
			len = tester_read_cmdreq(fd, "streamreq");
			assert(len < sizeof(buf));
			assert(recv(fd, buf, len, 0) == len);
			for (i = 0; i < stream_count; i++)
			{
				ev[9] = '0' + i;
				tester_write_event(fd, "listevent", ev, sizeof(ev));
			}
			tester_write_cmdres(fd, buf, len);
			break;
		case 2:
			tester_read_eventunreg(fd, "listevent");
			tester_write_eventconfirm(fd);
			break;
		default:
			assert(0);
			break;
	}
}

static void reqcb(struct davici_conn *c, int err, const char *name,
				  struct davici_response *res, void *user)
{
	char buf[10];
	int i, ret;

	assert(err >= 0);
	for (i = 0;; i++)
	{
		ret = davici_parse(res);
		switch (i)
		{
			case 0:
				assert(ret == DAVICI_KEY_VALUE);
				assert(strcmp(davici_get_name(res), "count") == 0);
				assert(davici_get_value_str(res, buf, sizeof(buf)) == 1);
				assert(buf[0] - '0' == stream_count);
				continue;
			case 1:
				assert(ret == DAVICI_END);
				break;
			default:
				assert(0);
		}
		break;
	}
}

static void streamcb(struct davici_conn *c, int err, const char *name,
					 struct davici_response *res, void *user)
{
	struct tester *t = user;
	char buf[10];
	int i, ret;

	assert(err >= 0);
	if (res)
	{
		for (i = 0;; i++)
		{
			ret = davici_parse(res);
			switch (i)
			{
				case 0:
					assert(ret == DAVICI_KEY_VALUE);
					assert(strcmp(davici_get_name(res), "count") == 0);
					assert(davici_get_value_str(res, buf, sizeof(buf)) == 1);
					assert(buf[0] - '0' == seen++);
					continue;
				case 1:
					assert(ret == DAVICI_END);
					break;
				default:
					assert(0);
			}
			break;
		}
	}
	else if (seen == stream_count)
	{
		tester_complete(t);
	}
}

int main(int argc, char *argv[])
{
	struct tester *t;
	struct davici_conn *c;
	struct davici_request *r;

#ifdef _WIN32
	WSADATA wsd;
	WSAStartup( MAKEWORD( 2, 2 ), &wsd );
	t = tester_create( echocb );
	assert(davici_connect_tcp(tester_getport(t),
							  tester_davici_iocb, t, &c) >= 0);
#else
	t = tester_create( echocb );
	assert(davici_connect_unix(tester_getpath(t),
							   tester_davici_iocb, t, &c) >= 0);
#endif
	assert(davici_new_cmd("streamreq", &r) >= 0);
	davici_kvf(r, "count", "%d", stream_count);
	assert(davici_queue_streamed(c, r, reqcb, "listevent", streamcb, t) >= 0);

	tester_runio(t, c);
	assert(seen == stream_count);
	davici_disconnect(c);
	tester_cleanup(t);
	return 0;
}
