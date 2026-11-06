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

#include "davici.h"

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

static int iocb(struct davici_conn *c, int fd, int ops, void *user)
{
	return 0;
}

static void partial(unsigned int body, int eof)
{
	struct davici_conn *c;
	uint32_t len = htonl(64);
	unsigned char type = 1;
	int s[2];

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, s) == 0);
	assert(davici_connect_socket(s[0], iocb, NULL, &c) == 0);
	assert(write(s[1], &len, sizeof(len)) == sizeof(len));
	if (body)
	{
		assert(write(s[1], &type, sizeof(type)) == sizeof(type));
	}
	/* Allocate the receive buffer, then stop at EWOULDBLOCK. */
	assert(davici_read(c) == 0);
	if (eof)
	{
		assert(shutdown(s[1], SHUT_WR) == 0);
		assert(davici_read(c) == -ECONNRESET);
	}
	/* Run under a leak checker to verify partial buffers are released. */
	davici_disconnect(c);
	close(s[1]);
}

int main(int argc, char *argv[])
{
	partial(0, 0);
	partial(1, 0);
	partial(0, 1);
	partial(1, 1);
	return 0;
}
