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

#include <davici.h>

struct tester;

typedef void (*tester_srvcb)(struct tester *tester, davici_fd fd);

struct tester* tester_create(tester_srvcb srvcb);

int tester_davici_iocb(struct davici_conn *c, davici_fd fd, int ops, void *user);

void tester_runio(struct tester *tester, struct davici_conn *c);

void tester_complete(struct tester *tester);

#ifdef _WIN32

int tester_getport(struct tester *tester);

#else

const char *tester_getpath(struct tester *tester);

#endif

void tester_cleanup(struct tester *tester);

unsigned int tester_read_cmdreq(davici_fd fd, const char *name);

void tester_write_cmdres(davici_fd fd, const char *buf, unsigned int buflen);

void tester_write_cmdunknown(davici_fd fd);

unsigned int tester_read_eventreg(davici_fd fd, const char *name);

unsigned int tester_read_eventunreg(davici_fd fd, const char *name);

void tester_write_eventconfirm(davici_fd fd);

void tester_write_eventunknown(davici_fd fd);

void tester_write_event(davici_fd fd, const char *name,
						const char *buf, unsigned int buflen);
