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
#include <errno.h>

static void srvcb(struct tester *t, int fd)
{
	tester_read_eventreg(fd, "nosuchevent");
	tester_write_eventunknown(fd);
}

static void eventcb(struct davici_conn *c, int err, const char *name,
					struct davici_response *res, void *user)
{
	struct tester *t = user;

	assert(strcmp(name, "nosuchevent") == 0);
	assert(err == -EBADSLT);
	assert(res == NULL);

	tester_complete(t);
}

int main(int argc, char *argv[])
{
	struct tester *t;
	struct davici_conn *c;

	t = tester_create(srvcb);
	assert(davici_connect_unix(tester_getpath(t),
							   tester_davici_iocb, t, &c) >= 0);
	assert(davici_register(c, "nosuchevent", eventcb, t) >= 0);

	tester_runio(t, c);
	davici_disconnect(c);
	tester_cleanup(t);
	return 0;
}
