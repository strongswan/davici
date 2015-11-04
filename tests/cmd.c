/*
 * Copyright (c) 2015 CloudGuard Software AG. All rights reserved.
 */

#include "tester.h"

#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

static void echocb(struct tester *t, int fd)
{
	char buf[2048];
	uint32_t len;

	len = tester_read_cmdreq(fd, "echoreq");
	assert(len < sizeof(buf));
	assert(read(fd, buf, len) == len);
	tester_write_cmdres(fd, buf, len);
}

static void reqcb(struct davici_conn *c, int err, const char *name,
				  struct davici_response *res, void *user)
{
	struct tester *t = user;
	char buf[64];
	unsigned int len;
	const void *v;
	int ret, i;

	assert(err >= 0);

	for (i = 0;; i++)
	{
		ret = davici_parse(res);
		switch (i)
		{
			case 0:
				assert(ret == DAVICI_SECTION_START);
				assert(strcmp(davici_get_name(res), "section") == 0);
				break;
			case 1:
				assert(ret == DAVICI_KEY_VALUE);
				assert(strcmp(davici_get_name(res), "key") == 0);
				v = davici_get_value(res, &len);
				assert(v);
				assert(len == strlen("value"));
				assert(memcmp(v, "value", len) == 0);
				break;
			case 2:
				assert(ret == DAVICI_LIST_START);
				assert(strcmp(davici_get_name(res), "list") == 0);
				assert(davici_name_strcmp(res, "list") == 0);
				assert(davici_name_strcmp(res, "a") > 0);
				assert(davici_name_strcmp(res, "x") < 0);
				break;
			case 3:
				assert(ret == DAVICI_LIST_ITEM);
				assert(davici_get_value_str(res, buf,
											sizeof(buf)) == strlen("item"));
				assert(strcmp(buf, "item") == 0);
				assert(davici_value_strcmp(res, "item") == 0);
				assert(davici_value_strcmp(res, "a") > 0);
				assert(davici_value_strcmp(res, "x") < 0);
				assert(davici_value_strcmp(res, "itemxx") < 0);
				break;
			case 4:
				assert(ret == DAVICI_LIST_END);
				break;
			case 5:
				assert(ret == DAVICI_SECTION_END);
				break;
			case 6:
				assert(ret == DAVICI_END);
				return tester_complete(t);
			default:
				assert(0);
				break;
		}
	}
}

int main(int argc, char *argv[])
{
	struct tester *t;
	struct davici_conn *c;
	struct davici_request *r;

	t = tester_create(echocb);
	assert(davici_connect_unix(tester_getpath(t),
							   tester_davici_iocb, t, &c) >= 0);
	assert(davici_new_cmd("tocancel", &r) >= 0);
	davici_cancel(r);
	assert(davici_new_cmd("one", &r) >= 0);
	assert(davici_queue(c, r, reqcb, t) >= 0);
	assert(davici_new_cmd("two", &r) >= 0);
	assert(davici_queue(c, r, reqcb, t) >= 0);
	assert(davici_new_cmd("three", &r) >= 0);
	assert(davici_queue(c, r, reqcb, t) >= 0);
	davici_disconnect(c);

	assert(davici_connect_unix(tester_getpath(t),
							   tester_davici_iocb, t, &c) >= 0);
	assert(davici_new_cmd("echoreq", &r) >= 0);
	davici_section_start(r, "section");
	davici_kvf(r, "key", "%s", "value");
	davici_list_start(r, "list");
	davici_list_itemf(r, "%s", "item");
	davici_list_end(r);
	davici_section_end(r);

	assert(davici_queue(c, r, reqcb, t) >= 0);

	tester_runio(t, c);
	davici_disconnect(c);
	tester_cleanup(t);
	return 0;
}
