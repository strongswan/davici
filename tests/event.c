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

static unsigned int seen = 0;

static void echocb(struct tester *t, int fd)
{
	static int state = 0;

	switch (state++)
	{
		case 0:
			tester_read_eventreg(fd, "anevent");
			tester_write_eventconfirm(fd);
			tester_write_event(fd, "anevent", NULL, 0);
			break;
		case 1:
			tester_read_eventreg(fd, "another");
			tester_write_eventconfirm(fd);
			tester_write_event(fd, "another", NULL, 0);
			break;
		case 2:
			tester_read_eventunreg(fd, "another");
			tester_write_eventconfirm(fd);
			break;
		case 3:
			tester_read_eventunreg(fd, "anevent");
			tester_write_eventconfirm(fd);
			break;
		default:
			assert(0);
			break;
	}
}

static void eventcb(struct davici_conn *c, int err, const char *name,
					struct davici_response *res, void *user)
{
	struct tester *t = user;

	assert(strcmp(name, "anevent") == 0);
	assert(err >= 0);
	if (res)
	{
		assert(davici_parse(res) == DAVICI_END);
		seen++;
	}
	else if (seen == 2)
	{
		tester_complete(t);
	}
}

static void anothercb(struct davici_conn *c, int err, const char *name,
					  struct davici_response *res, void *user)
{
	assert(strcmp(name, "another") == 0);
	assert(err >= 0);
	if (res)
	{
		assert(davici_parse(res) == DAVICI_END);
		seen++;
	}
}

int main(int argc, char *argv[])
{
	struct tester *t;
	struct davici_conn *c;

	t = tester_create(echocb);
	assert(davici_connect_unix(tester_getpath(t),
							   tester_davici_iocb, t, &c) >= 0);
	assert(davici_register(c, "anevent", eventcb, t) >= 0);
	assert(davici_register(c, "another", anothercb, t) >= 0);
	assert(davici_unregister(c, "another", anothercb, t) >= 0);
	assert(davici_unregister(c, "anevent", eventcb, t) >= 0);

	tester_runio(t, c);
	assert(seen == 2);
	davici_disconnect(c);
	tester_cleanup(t);
	return 0;
}
