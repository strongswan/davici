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

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <assert.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "tester.h"

enum tester_fd {
	FD_CLIENT = 0,
	FD_LISTEN = 1,
	FD_SERVER = 2,
	FD_COUNT
};

enum tester_type {
	CMD_REQUEST = 0,
	CMD_RESPONSE = 1,
	CMD_UNKNOWN = 2,
	EVENT_REGISTER = 3,
	EVENT_UNREGISTER = 4,
	EVENT_CONFIRM = 5,
	EVENT_UNKNOWN = 6,
	EVENT = 7,
};

struct tester {
	struct pollfd pfd[FD_COUNT];
	char path[32];
	unsigned short port;
	tester_srvcb srvcb;
	int complete;
};

struct tester* tester_create(tester_srvcb srvcb)
{
	struct sockaddr_un addr;
	struct tester *t;
	int len;

	t = calloc(1, sizeof(*t));
	assert(t);
	assert(snprintf(t->path, sizeof(t->path),
					"/tmp/test-%d.vici", getpid()) > 0);
	t->srvcb = srvcb;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", t->path);
	len = offsetof(struct sockaddr_un, sun_path) + strlen(addr.sun_path);

	t->pfd[FD_LISTEN].fd = socket(AF_UNIX, SOCK_STREAM, 0);
	assert(t->pfd[FD_LISTEN].fd >= 0);
	t->pfd[FD_LISTEN].events = POLLIN;
	t->pfd[FD_SERVER].events = POLLIN;
	t->pfd[FD_SERVER].fd = -1;

	unlink(t->path);
	assert(bind(t->pfd[FD_LISTEN].fd, (struct sockaddr*)&addr, len) == 0);
	assert(listen(t->pfd[FD_LISTEN].fd, 2) == 0);

	return t;
}

struct tester* tester_create_tcp(tester_srvcb srvcb)
{
	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_addr.s_addr = htonl(INADDR_LOOPBACK),
	};
	struct tester *t;
	socklen_t len;

	t = calloc(1, sizeof(*t));
	assert(t);
	t->srvcb = srvcb;

	t->pfd[FD_LISTEN].fd = socket(AF_INET, SOCK_STREAM, 0);
	assert(t->pfd[FD_LISTEN].fd >= 0);
	t->pfd[FD_LISTEN].events = POLLIN;
	t->pfd[FD_SERVER].events = POLLIN;
	t->pfd[FD_SERVER].fd = -1;

	len = sizeof(addr);
	assert(bind(t->pfd[FD_LISTEN].fd, (struct sockaddr*)&addr, len) == 0);
	assert(listen(t->pfd[FD_LISTEN].fd, 2) == 0);
	assert(getsockname(t->pfd[FD_LISTEN].fd,
					   (struct sockaddr*)&addr, &len) == 0);
	t->port = ntohs(addr.sin_port);
	return t;
}

int tester_davici_iocb(struct davici_conn *c, int fd, int ops, void *user)
{
	struct tester *t = user;

	t->pfd[FD_CLIENT].fd = fd;
	t->pfd[FD_CLIENT].events = 0;
	if (ops & DAVICI_READ)
	{
		t->pfd[FD_CLIENT].events |= POLLIN;
	}
	if (ops & DAVICI_WRITE)
	{
		t->pfd[FD_CLIENT].events |= POLLOUT;
	}
	return 0;
}

void tester_runio(struct tester *t, struct davici_conn *c)
{
	while (!t->complete)
	{
		int fd;

		assert(poll(t->pfd, sizeof(t->pfd) / sizeof(t->pfd[0]), -1) >= 0);
		if (t->pfd[FD_CLIENT].revents & POLLIN)
		{
			assert(davici_read(c) >= 0);
		}
		if (t->pfd[FD_CLIENT].revents & POLLOUT)
		{
			assert(davici_write(c) >= 0);
		}
		if (t->pfd[FD_LISTEN].revents & POLLIN)
		{
			fd = accept(t->pfd[FD_LISTEN].fd, NULL, NULL);
			assert(fd >= 0);
			t->pfd[FD_SERVER].fd = fd;
		}
		if (t->pfd[FD_SERVER].revents & POLLIN)
		{
			t->srvcb(t, t->pfd[FD_SERVER].fd);
		}
	}
}

void tester_complete(struct tester *t)
{
	t->complete = 1;
}

const char *tester_getpath(struct tester *t)
{
	return t->path;
}

unsigned short tester_get_tcpport(struct tester *t)
{
	return t->port;
}

void tester_cleanup(struct tester *t)
{
	close(t->pfd[FD_LISTEN].fd);
	close(t->pfd[FD_SERVER].fd);
	if (t->path[0])
	{
		unlink(t->path);
	}
	free(t);
}

static unsigned int read_type(int fd, const char *name, uint8_t expected)
{
	uint8_t type, namelen;
	uint32_t pktlen;
	char buf[257];

	assert(read(fd, &pktlen, sizeof(pktlen)) == sizeof(pktlen));
	assert(read(fd, &type, sizeof(type)) == sizeof(type));
	assert(type == expected);
	assert(read(fd, &namelen, sizeof(namelen)) == sizeof(namelen));
	assert(read(fd, buf, namelen) == namelen);
	buf[namelen] = 0;
	assert(strcmp(buf, name) == 0);

	return ntohl(pktlen) - sizeof(type) - sizeof(namelen) - namelen;
}

unsigned int tester_read_cmdreq(int fd, const char *name)
{
	return read_type(fd, name, CMD_REQUEST);
}

void tester_write_cmdres(int fd, const char *buf, unsigned int buflen)
{
	uint8_t type = CMD_RESPONSE;
	uint32_t len;

	len = htonl(sizeof(type) + buflen);
	assert(write(fd, &len, sizeof(len)) == sizeof(len));
	assert(write(fd, &type, sizeof(type)) == sizeof(type));
	assert(write(fd, buf, buflen) == buflen);
}

void tester_write_cmdunknown(int fd)
{
	uint8_t type = CMD_UNKNOWN;
	uint32_t len;

	len = htonl(sizeof(type));
	assert(write(fd, &len, sizeof(len)) == sizeof(len));
	assert(write(fd, &type, sizeof(type)) == sizeof(type));
}

unsigned int tester_read_eventreg(int fd, const char *name)
{
	return read_type(fd, name, EVENT_REGISTER);
}

unsigned int tester_read_eventunreg(int fd, const char *name)
{
	return read_type(fd, name, EVENT_UNREGISTER);
}

void tester_write_eventconfirm(int fd)
{
	uint8_t type = EVENT_CONFIRM;
	uint32_t len;

	len = htonl(sizeof(type));
	assert(write(fd, &len, sizeof(len)) == sizeof(len));
	assert(write(fd, &type, sizeof(type)) == sizeof(type));
}

void tester_write_eventunknown(int fd)
{
	uint8_t type = EVENT_UNKNOWN;
	uint32_t len;

	len = htonl(sizeof(type));
	assert(write(fd, &len, sizeof(len)) == sizeof(len));
	assert(write(fd, &type, sizeof(type)) == sizeof(type));
}

void tester_write_event(int fd, const char *name,
						const char *buf, unsigned int buflen)
{
	uint8_t namelen, type = EVENT;
	uint32_t len;

	namelen = strlen(name);
	len = htonl(sizeof(type) + sizeof(namelen) + namelen + buflen);
	assert(write(fd, &len, sizeof(len)) == sizeof(len));
	assert(write(fd, &type, sizeof(type)) == sizeof(type));
	assert(write(fd, &namelen, sizeof(namelen)) == sizeof(namelen));
	assert(write(fd, name, namelen) == namelen);
	assert(write(fd, buf, buflen) == buflen);
}
