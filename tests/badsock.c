/*
 * Copyright (c) 2015 CloudGuard Software AG. All rights reserved.
 */

#include "tester.h"

#include <assert.h>
#include <errno.h>

int iocb(struct davici_conn *c, int fd, int ops, void *user)
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
