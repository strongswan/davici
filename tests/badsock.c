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
#include <errno.h>

int iocb(struct davici_conn *c, davici_fd fd, int ops, void *user)
{
	assert(0);
}

int main(int argc, char *argv[])
{
	struct davici_conn *c;
	int ret;

	ret = davici_connect_unix("/does/not/exist", iocb, NULL, &c);
	assert(ret == -ENOENT);
	return 0;
}
