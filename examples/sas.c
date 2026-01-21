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

#include <stdio.h>
#include <poll.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <davici.h>
#include <sys/select.h>
#include <netinet/in.h>

struct dispatch_data {
	int rfd;
	int wfd;
	struct davici_conn *c;
};

static int fdcb(struct davici_conn *conn, int fd, int ops, void *user)
{
	struct dispatch_data *dd = user;

	if (ops & DAVICI_READ)
	{
		dd->rfd = fd;
	}
	else
	{
		dd->rfd = -1;
	}
	if (ops & DAVICI_WRITE)
	{
		dd->wfd = fd;
	}
	else
	{
		dd->wfd = -1;
	}
	return 0;
}

static void die(int err, const char *msg)
{
	fprintf(stderr, "%s: %s\n", msg, strerror(-err));
	exit(-err);
}

static void sas_done_cb(struct davici_conn *conn, int err, const char *name,
						struct davici_response *res, void *user)
{
	if (err < 0)
	{
		die(err, name);
	}
}

static void sas_cb(struct davici_conn *conn, int err, const char *name,
				   struct davici_response *res, void *user)
{
	if (err < 0)
	{
		die(err, name);
	}
	if (res)
	{
		davici_dump(res, name, "\n", 0, 2, stdout);
		printf("\n");
	}
}

static void updown_cb(struct davici_conn *conn, int err, const char *name,
					  struct davici_response *res, void *user)
{
	if (err < 0)
	{
		die(err, name);
	}
	if (res)
	{
		davici_dump(res, name, "\n", 0, 2, stdout);
		printf("\n");
	}
}

static int query_sas(struct dispatch_data *dd)
{
	struct davici_request *r;
	int err;

	err = davici_new_cmd("list-sas", &r);
	if (err < 0)
	{
		return err;
	}
	err = davici_queue_streamed(dd->c, r, sas_done_cb, "list-sa", sas_cb, dd);
	if (err < 0)
	{
		return err;
	}
	return 0;
}

static int dispatch(struct dispatch_data *dd)
{
	fd_set rfds, wfds, efds;
	int err, ret, maxfd;

	while (1)
	{
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		FD_ZERO(&efds);
		maxfd = 0;
		if (dd->rfd != -1)
		{
			FD_SET(dd->rfd, &rfds);
			FD_SET(dd->rfd, &efds);
			if (dd->rfd + 1 > maxfd)
			{
				maxfd = dd->rfd + 1;
			}
		}
		if (dd->wfd != -1)
		{
			FD_SET(dd->wfd, &wfds);
			FD_SET(dd->wfd, &efds);
			if (dd->wfd + 1 > maxfd)
			{
				maxfd = dd->wfd + 1;
			}
		}
		ret = select(maxfd, &rfds, &wfds, &efds, NULL);
		if (ret < 0)
		{
			return -errno;
		}
		if (ret != 1)
		{
			return -EIO;
		}
		if (FD_ISSET(dd->rfd, &rfds) || FD_ISSET(dd->wfd, &efds))
		{
			err = davici_read(dd->c);
			if (err < 0)
			{
				return err;
			}
		}
		if (FD_ISSET(dd->wfd, &wfds) || FD_ISSET(dd->wfd, &efds))
		{
			err = davici_write(dd->c);
			if (err < 0)
			{
				return err;
			}
		}
	}
	return 0;
}

int main(int argc, const char *argv[])
{
	struct dispatch_data dd = {
		.rfd = -1,
		.wfd = -1,
	};
	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(43210),
		.sin_addr.s_addr = htonl(INADDR_LOOPBACK),
	};
	int err;

	err = davici_connect_tcp((struct sockaddr*)&addr, fdcb, &dd, &dd.c);
	if (err < 0)
	{
		die(err, "connect failed");
	}
	err = davici_register(dd.c, "ike-updown", updown_cb, &dd);
	if (err < 0)
	{
		die(err, "register failed");
	}
	err = query_sas(&dd);
	if (err < 0)
	{
		die(err, "query SAs failed");
	}
	err = dispatch(&dd);
	if (err < 0)
	{
		die(err, "dispatch failed");
	}
	davici_disconnect(dd.c);
	return 0;
}
