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

#include <davici.h>

struct tester;

typedef void (*tester_srvcb)(struct tester *tester, int fd);

struct tester* tester_create(tester_srvcb srvcb);

int tester_davici_iocb(struct davici_conn *c, int fd, int ops, void *user);

void tester_runio(struct tester *tester, struct davici_conn *c);

void tester_complete(struct tester *tester);

const char *tester_getpath(struct tester *tester);

void tester_cleanup(struct tester *tester);

unsigned int tester_read_cmdreq(int fd, const char *name);

void tester_write_cmdres(int fd, const char *buf, unsigned int buflen);

void tester_write_cmdunknown(int fd);

unsigned int tester_read_eventreg(int fd, const char *name);

unsigned int tester_read_eventunreg(int fd, const char *name);

void tester_write_eventconfirm(int fd);

void tester_write_eventunknown(int fd);

void tester_write_event(int fd, const char *name,
						const char *buf, unsigned int buflen);
