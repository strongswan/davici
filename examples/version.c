/*
 * Copyright (C) 2021 onway ag
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

struct dispatch_data {
	struct pollfd pfd;
	struct davici_conn *c;
	int pending;
};

static short ops2poll(int ops)
{
	short events = 0;

	if (ops & DAVICI_READ)
	{
		events |= POLLIN;
	}
	if (ops & DAVICI_WRITE)
	{
		events |= POLLOUT;
	}
	return events;
}

static int fdcb(struct davici_conn *conn, int fd, int ops, void *user)
{
	struct dispatch_data *dd = user;

	dd->pfd.events = ops2poll(ops);
	if (dd->pfd.events)
	{
		dd->pfd.fd = fd;
	}
	else
	{
		dd->pfd.fd = -1;
	}
	return 0;
}

static void die(int err, const char *msg)
{
	fprintf(stderr, "%s: %s\n", msg, strerror(-err));
	exit(-err);
}

static void version_cb(struct davici_conn *conn, int err, const char *name,
					   struct davici_response *res, void *user)
{
	struct dispatch_data *dd = user;

	if (err < 0)
	{
		die(err, "version");
	}
	davici_dump(res, "version", "\n", 0, 2, stdout);
	dd->pending--;
}

static int query_version(struct dispatch_data *dd)
{
	struct davici_request *r;
	int err;

	err = davici_new_cmd("version", &r);
	if (err < 0)
	{
		return err;
	}
	err = davici_queue(dd->c, r, version_cb, dd);
	if (err < 0)
	{
		return err;
	}
	dd->pending++;
	return 0;
}

static int dispatch(struct dispatch_data *dd)
{
	int err, ret;

	while (dd->pending && dd->pfd.fd != -1)
	{
		ret = poll(&dd->pfd, 1, -1);
		if (ret < 0)
		{
			return -errno;
		}
		if (ret != 1)
		{
			return -EIO;
		}
		if (dd->pfd.revents & POLLIN)
		{
			err = davici_read(dd->c);
			if (err < 0)
			{
				return err;
			}
		}
		if (dd->pfd.revents & POLLOUT)
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
		.pfd = {
			.fd = -1,
		},
	};
	int err;

	err = davici_connect_unix("/var/run/charon.vici", fdcb, &dd, &dd.c);
	if (err < 0)
	{
		die(err, "connect failed");
	}
	err = query_version(&dd);
	if (err < 0)
	{
		die(err, "query version failed");
	}
	err = dispatch(&dd);
	if (err < 0)
	{
		die(err, "dispatch failed");
	}
	davici_disconnect(dd.c);
	return 0;
}
