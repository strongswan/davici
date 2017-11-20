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

#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#ifdef _WIN32
#include <WinSock2.h>
#define unlink _unlink
#define poll   WSAPoll
#else
#include <unistd.h>
#include <poll.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#define closesocket close
#endif

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
#ifdef _WIN32
	int port;
#else
	const char *path;
#endif
	tester_srvcb srvcb;
	int complete;
};

struct tester* tester_create(tester_srvcb srvcb)
{
#ifdef _WIN32
	struct sockaddr_in addr;
	struct tester *t;

	t = calloc(1, sizeof(*t));
	assert(t);
	t->port = 4502;
	t->srvcb = srvcb;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons( t->port );
	addr.sin_addr.s_addr = 0x0100007F;

	t->pfd[FD_LISTEN].fd = socket(AF_INET, SOCK_STREAM, 0);
	assert(t->pfd[FD_LISTEN].fd >= 0);
	t->pfd[FD_LISTEN].events = POLLIN;
	t->pfd[FD_SERVER].events = POLLIN;
	t->pfd[FD_SERVER].fd = -1;

	assert(bind(t->pfd[FD_LISTEN].fd, (struct sockaddr*)&addr, sizeof(addr)) == 0);
	assert(listen(t->pfd[FD_LISTEN].fd, 2) == 0);

	return t;
#else
	struct sockaddr_un addr;
	struct tester *t;
	int len;

	t = calloc(1, sizeof(*t));
	assert(t);
	t->path = "/tmp/test.vici";
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
#endif
}

int tester_davici_iocb(struct davici_conn *c, davici_fd fd, int ops, void *user)
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
		davici_fd fd;

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

#ifdef _WIN32

int tester_getport(struct tester *t)
{
	return t->port;
}

#else

const char *tester_getpath(struct tester *t)
{
	return t->path;
}

#endif

void tester_cleanup(struct tester *t)
{
	closesocket(t->pfd[FD_LISTEN].fd);
	closesocket(t->pfd[FD_SERVER].fd);
	free(t);
}

static unsigned int read_type(davici_fd fd, const char *name, uint8_t expected)
{
	uint8_t type, namelen;
	uint32_t pktlen;
	char buf[257];

	assert(recv(fd, (char *)&pktlen, sizeof(pktlen), 0) == sizeof(pktlen));
	assert(recv(fd, &type, sizeof(type), 0) == sizeof(type));
	assert(type == expected);
	assert(recv(fd, &namelen, sizeof(namelen), 0) == sizeof(namelen));
	assert(recv(fd, buf, namelen, 0) == namelen);
	buf[namelen] = 0;
	assert(strcmp(buf, name) == 0);

	return ntohl(pktlen) - sizeof(type) - sizeof(namelen) - namelen;
}

unsigned int tester_read_cmdreq(davici_fd fd, const char *name)
{
	return read_type(fd, name, CMD_REQUEST);
}

void tester_write_cmdres(davici_fd fd, const char *buf, unsigned int buflen)
{
	uint8_t type = CMD_RESPONSE;
	uint32_t len;

	len = htonl(sizeof(type) + buflen);
	assert(send(fd, (const char *)&len, sizeof(len), 0) == sizeof(len));
	assert(send(fd, &type, sizeof(type), 0) == sizeof(type));
	assert(send(fd, buf, buflen, 0) == buflen);
}

void tester_write_cmdunknown(davici_fd fd)
{
	uint8_t type = CMD_UNKNOWN;
	uint32_t len;

	len = htonl(sizeof(type));
	assert(send(fd, (const char *)&len, sizeof(len), 0) == sizeof(len));
	assert(send(fd, &type, sizeof(type), 0) == sizeof(type));
}

unsigned int tester_read_eventreg(davici_fd fd, const char *name)
{
	return read_type(fd, name, EVENT_REGISTER);
}

unsigned int tester_read_eventunreg(davici_fd fd, const char *name)
{
	return read_type(fd, name, EVENT_UNREGISTER);
}

void tester_write_eventconfirm(davici_fd fd)
{
	uint8_t type = EVENT_CONFIRM;
	uint32_t len;

	len = htonl(sizeof(type));
	assert(send(fd, (const char *)&len, sizeof(len), 0) == sizeof(len));
	assert(send(fd, &type, sizeof(type), 0) == sizeof(type));
}

void tester_write_eventunknown(davici_fd fd)
{
	uint8_t type = EVENT_UNKNOWN;
	uint32_t len;

	len = htonl(sizeof(type));
	assert(send(fd, (const char *)&len, sizeof(len), 0) == sizeof(len));
	assert(send(fd, &type, sizeof(type), 0) == sizeof(type));
}

void tester_write_event(davici_fd fd, const char *name,
						const char *buf, unsigned int buflen)
{
	uint8_t namelen, type = EVENT;
	uint32_t len;

	namelen = (uint8_t)strlen(name);
	len = htonl(sizeof(type) + sizeof(namelen) + namelen + buflen);
	assert(send(fd, (const char *)&len, sizeof(len), 0) == sizeof(len));
	assert(send(fd, &type, sizeof(type), 0) == sizeof(type));
	assert(send(fd, &namelen, sizeof(namelen), 0) == sizeof(namelen));
	assert(send(fd, name, namelen, 0) == namelen);
	assert(send(fd, buf, buflen, 0) == buflen);
}
