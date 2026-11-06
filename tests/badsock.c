/*
 * Copyright (C) 2015 onway ag
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
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>

static int badsock_iocb(struct davici_conn *c, int fd, int ops, void *user)
{
	assert(0);
}

static void badsock(void)
{
	struct davici_conn *c;
	struct sockaddr_un addr;
	char longpath[sizeof(addr.sun_path) + 1];
	int ret;

	ret = davici_connect_unix("/does/not/exist", badsock_iocb, NULL, &c);
	assert(ret == -ENOENT);
	memset(longpath, 'l', sizeof(longpath));
	longpath[sizeof(longpath) - 1] = '\0';
	ret = davici_connect_unix(longpath, badsock_iocb, NULL, &c);
	assert(ret == -ENAMETOOLONG);
}

static int partial_iocb(struct davici_conn *c, int fd, int ops, void *user)
{
	return 0;
}

static void partial(int eof)
{
	struct davici_conn *c;
	uint32_t len = htonl(64);
	int s[2];

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, s) == 0);
	assert(davici_connect_socket(s[0], partial_iocb, NULL, &c) == 0);
	assert(write(s[1], &len, sizeof(len)) == sizeof(len));
	assert(davici_read(c) == 0);
	if (eof)
	{
		assert(shutdown(s[1], SHUT_WR) == 0);
		assert(davici_read(c) == -ECONNRESET);
	}
	davici_disconnect(c);
	close(s[1]);
}

int main(int argc, char *argv[])
{
	badsock();
	partial(0);
	partial(1);
	return 0;
}
