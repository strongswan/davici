/*
 * Copyright (C) 2026 onway ag
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
#include <stdint.h>
#include <netinet/in.h>

static void echocb(struct tester *t, int fd)
{
	char buf[256];
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

	assert(err >= 0);
	return tester_complete(t);
}

int main(int argc, char *argv[])
{
	struct tester *t;
	struct davici_conn *c;
	struct davici_request *r;
	struct sockaddr_in in = {
		.sin_family = AF_INET,
		.sin_addr.s_addr = htonl(INADDR_LOOPBACK),
	};

	t = tester_create_tcp(echocb);
	in.sin_port = htons(tester_get_tcpport(t));
	assert(davici_connect_tcp((struct sockaddr*)&in,
							   tester_davici_iocb, t, &c) >= 0);
	assert(davici_new_cmd("echoreq", &r) >= 0);
	assert(davici_queue(c, r, reqcb, t) >= 0);

	tester_runio(t, c);
	davici_disconnect(c);
	tester_cleanup(t);
	return 0;
}
