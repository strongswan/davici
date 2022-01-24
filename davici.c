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

#include "davici.h"

#include <stdint.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <limits.h>

/* buffer size for a name tag */
#define NAME_BUF_LEN (UCHAR_MAX + 1)

enum davici_packet_type {
	DAVICI_CMD_REQUEST = 0,
	DAVICI_CMD_RESPONSE = 1,
	DAVICI_CMD_UNKNOWN = 2,
	DAVICI_EVENT_REGISTER = 3,
	DAVICI_EVENT_UNREGISTER = 4,
	DAVICI_EVENT_CONFIRM = 5,
	DAVICI_EVENT_UNKNOWN = 6,
	DAVICI_EVENT = 7,
};

struct davici_request {
	struct davici_request *next;
	unsigned int allocated;
	unsigned int used;
	unsigned int sent;
	char *buf;
	int err;
	davici_cb cb;
	void *user;
};

struct davici_packet {
	unsigned int received;
	char len[sizeof(uint32_t)];
	char *buf;
};

struct davici_response {
	struct davici_packet *pkt;
	unsigned int pos;
	unsigned int buflen;
	void *buf;
	char name[NAME_BUF_LEN];
	unsigned int section;
	unsigned int list;
};

struct davici_event {
	struct davici_event *next;
	davici_cb cb;
	void *user;
	char name[0];
};

struct davici_conn {
	int s;
	struct davici_request *reqs;
	struct davici_event *events;
	struct davici_packet pkt;
	davici_fdcb fdcb;
	void *user;
	enum davici_fdops ops;
};

static int connect_and_fcntl(int fd, const char *path)
{
	struct sockaddr_un addr;
	int len, flags;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
	len = offsetof(struct sockaddr_un, sun_path) + strlen(addr.sun_path);

	if (connect(fd, (struct sockaddr*)&addr, len) != 0)
	{
		return -errno;
	}
	flags = fcntl(fd, F_GETFL);
	if (flags == -1)
	{
		return -errno;
	}
#ifdef O_CLOEXEC
	flags |= O_CLOEXEC;
#endif
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
	{
		return -errno;
	}
	return 0;
}

int davici_connect_unix(const char *path, davici_fdcb fdcb, void *user,
						struct davici_conn **cp)
{
	struct davici_conn *c;
	int err;

	c = calloc(1, sizeof(*c));
	if (!c)
	{
		return -errno;
	}
	c->fdcb = fdcb;
	c->user = user;

	c->s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (c->s < 0)
	{
		err = -errno;
		free(c);
		return err;
	}
	err = connect_and_fcntl(c->s, path);
	if (err < 0)
	{
		close(c->s);
		free(c);
		return err;
	}

	*cp = c;
	return 0;
}

static unsigned int max_integer(unsigned int a, unsigned int b)
{
	return a > b ? a : b;
}

static int update_ops(struct davici_conn *c, enum davici_fdops ops)
{
	int ret;

	if (ops == c->ops)
	{
		return 0;
	}
	ret = c->fdcb(c, c->s, ops, c->user);
	if (ret == 0)
	{
		c->ops = ops;
	}
	return -abs(ret);
}

static int copy_name(char *out, unsigned int outlen,
					 const char *in, unsigned int inlen)
{
	int i;

	if (inlen >= outlen)
	{
		return -ENOBUFS;
	}
	for (i = 0; i < inlen; i++)
	{
		if (!isprint((unsigned char)in[i]))
		{
			return -EINVAL;
		}
	}
	memcpy(out, in, inlen);
	out[inlen] = '\0';
	return 0;
}

static struct davici_request* pop_request(struct davici_conn *c,
										  enum davici_packet_type type,
										  char *name, unsigned int namelen)
{
	struct davici_request *req;

	req = c->reqs;
	if (!req || !req->cb || req->used < 2 ||
		req->buf[0] != type || req->used - 2 < req->buf[1])
	{
		return NULL;
	}
	if (copy_name(name, namelen, req->buf + 2, req->buf[1]) < 0)
	{
		return NULL;
	}
	c->reqs = req->next;
	return req;
}

static void destroy_request(struct davici_request *req)
{
	free(req->buf);
	free(req);
}

static int handle_cmd_response(struct davici_conn *c, struct davici_packet *pkt)
{
	struct davici_request *req;
	struct davici_response res = {
		.pkt = pkt,
	};
	char name[NAME_BUF_LEN];

	req = pop_request(c, DAVICI_CMD_REQUEST, name, sizeof(name));
	if (!req)
	{
		return -EBADMSG;
	}

	req->cb(c, 0, name, &res, req->user);
	destroy_request(req);
	return 0;
}

static int handle_cmd_unknown(struct davici_conn *c)
{
	struct davici_request *req;
	char name[NAME_BUF_LEN];

	req = pop_request(c, DAVICI_CMD_REQUEST, name, sizeof(name));
	if (!req)
	{
		return -EBADMSG;
	}

	req->cb(c, -ENOSYS, name, NULL, req->user);
	destroy_request(req);
	return 0;
}

static int handle_event_unknown(struct davici_conn *c)
{
	struct davici_request *req;
	char name[NAME_BUF_LEN];

	req = pop_request(c, DAVICI_EVENT_REGISTER, name, sizeof(name));
	if (!req)
	{
		req = pop_request(c, DAVICI_EVENT_UNREGISTER, name, sizeof(name));
	}
	if (!req)
	{
		return -EBADMSG;
	}

	req->cb(c, -ENOENT, name, NULL, req->user);
	destroy_request(req);
	return 0;
}

static int remove_event(struct davici_conn *c, const char *name, davici_cb cb)
{
	struct davici_event *ev, *prev = NULL;

	ev = c->events;
	while (ev)
	{
		if (strcmp(ev->name, name) == 0 && ev->cb == cb)
		{
			if (prev)
			{
				prev->next = ev->next;
			}
			else
			{
				c->events = ev->next;
			}
			free(ev);
			return 0;
		}
		prev = ev;
		ev = ev->next;
	}
	return -ENOENT;
}

static int add_event(struct davici_conn *c, const char *name,
					 davici_cb cb, void *user)
{
	struct davici_event *ev;
	int len;

	len = strlen(name);
	ev = calloc(sizeof(*ev) + len + 1, 1);
	if (!ev)
	{
		return -errno;
	}
	memcpy(ev->name, name, len);
	ev->cb = cb;
	ev->user = user;
	ev->next = c->events;
	c->events = ev;
	return 0;
}

static int handle_event_confirm(struct davici_conn *c)
{
	struct davici_request *req;
	char name[NAME_BUF_LEN];
	int err;

	req = pop_request(c, DAVICI_EVENT_REGISTER, name, sizeof(name));
	if (req)
	{
		err = add_event(c, name, req->cb, req->user);
	}
	else
	{
		req = pop_request(c, DAVICI_EVENT_UNREGISTER, name, sizeof(name));
		if (req)
		{
			err = remove_event(c, name, req->cb);
		}
		else
		{
			return -EBADMSG;
		}
	}
	req->cb(c, err, name, NULL, req->user);
	destroy_request(req);
	return 0;
}

static int handle_event(struct davici_conn *c, struct davici_packet *pkt)
{
	struct davici_packet inner = {
		.buf = pkt->buf + 1 + pkt->buf[0],
		.received = pkt->received - 1 - pkt->buf[0],
	};
	struct davici_response res = {
		.pkt = &inner,
	};
	struct davici_event *ev;
	char name[NAME_BUF_LEN];
	int err;

	if (!pkt->received || pkt->buf[0] >= c->pkt.received - 1)
	{
		return -EBADMSG;
	}
	err = copy_name(name, sizeof(name), pkt->buf + 1, pkt->buf[0]);
	if (err < 0)
	{
		return err;
	}
	ev = c->events;
	while (ev)
	{
		if (strcmp(name, ev->name) == 0)
		{
			ev->cb(c, 0, ev->name, &res, ev->user);
		}
		ev = ev->next;
	}
	return 0;
}

static int handle_message(struct davici_conn *c)
{
	struct davici_packet pkt = {
		.buf = c->pkt.buf + 1,
		.received = c->pkt.received - sizeof(c->pkt.len) - 1,
	};

	switch (c->pkt.buf[0])
	{
		case DAVICI_CMD_RESPONSE:
			return handle_cmd_response(c, &pkt);
		case DAVICI_CMD_UNKNOWN:
			return handle_cmd_unknown(c);
		case DAVICI_EVENT_UNKNOWN:
			return handle_event_unknown(c);
		case DAVICI_EVENT_CONFIRM:
			return handle_event_confirm(c);
		case DAVICI_EVENT:
			return handle_event(c, &pkt);
		default:
			return 0;
	}
}

int davici_read(struct davici_conn *c)
{
	uint32_t size;
	int len, err = 0;

	while (!err)
	{
		while (c->pkt.received < sizeof(c->pkt.len))
		{
			len = recv(c->s, c->pkt.len + c->pkt.received,
					   sizeof(c->pkt.len) - c->pkt.received, 0);
			if (len == -1)
			{
				if (errno == EWOULDBLOCK || errno == EINTR)
				{
					return 0;
				}
				return -errno;
			}
			if (len == 0)
			{
				return -ECONNRESET;
			}
			c->pkt.received += len;
		}
		memcpy(&size, c->pkt.len, sizeof(size));
		size = ntohl(size);
		if (!c->pkt.buf)
		{
			c->pkt.buf = malloc(size);
			if (!c->pkt.buf)
			{
				return -errno;
			}
		}
		while (c->pkt.received < size + sizeof(c->pkt.len))
		{
			len = recv(c->s, c->pkt.buf + c->pkt.received - sizeof(c->pkt.len),
					   size - (c->pkt.received - sizeof(c->pkt.len)), 0);
			if (len == -1)
			{
				if (errno == EWOULDBLOCK || errno == EINTR)
				{
					return 0;
				}
				return -errno;
			}
			if (len == 0)
			{
				return -ECONNRESET;
			}
			c->pkt.received += len;
		}
		if (size)
		{
			err = handle_message(c);
		}
		else
		{
			err = 0;
		}
		free(c->pkt.buf);
		c->pkt.buf = NULL;
		c->pkt.received = 0;
	}
	return err;
}

int davici_write(struct davici_conn *c)
{
	struct davici_request *req;
	uint32_t size;
	int len, err;

	req = c->reqs;
	while (req)
	{
		while (req->sent < sizeof(req->used))
		{
			size = htonl(req->used);
			len = send(c->s, (char*)&size + req->sent,
					   sizeof(size) - req->sent, 0);
			if (len == -1)
			{
				if (errno == EWOULDBLOCK || errno == EINTR)
				{
					return 0;
				}
				return -errno;
			}
			req->sent += len;
		}
		while (req->sent < req->used + sizeof(size))
		{
			len = send(c->s, req->buf + req->sent - sizeof(size),
					   req->used - (req->sent - sizeof(size)), 0);
			if (len == -1)
			{
				if (errno == EWOULDBLOCK || errno == EINTR)
				{
					return 0;
				}
				return -errno;
			}
			req->sent += len;
		}
		err = update_ops(c, c->ops | DAVICI_READ);
		if (err)
		{
			return err;
		}
		req = req->next;
	}
	return update_ops(c, c->ops & ~DAVICI_WRITE);
}

void davici_disconnect(struct davici_conn *c)
{
	struct davici_event *event;
	struct davici_request *req;
	void *next;

	update_ops(c, 0);

	event = c->events;
	while (event)
	{
		next = event->next;
		free(event);
		event = next;
	}
	req = c->reqs;
	while (req)
	{
		next = req->next;
		free(req->buf);
		free(req);
		req = next;
	}
	close(c->s);
	free(c);
}

static int create_request(enum davici_packet_type type, const char *name,
						  struct davici_request **rp)
{
	struct davici_request *req;
	int err;

	req = calloc(1, sizeof(*req));
	if (!req)
	{
		return -errno;
	}
	req->used = 2;
	if (name)
	{
		req->used += strlen(name);
	}
	req->allocated = max_integer(32, req->used);
	req->buf = malloc(req->allocated);
	if (!req->buf)
	{
		err = -errno;
		free(req);
		return err;
	}
	req->buf[0] = type;
	req->buf[1] = req->used - 2;
	if (name)
	{
		memcpy(req->buf + 2, name, req->used - 2);
	}
	*rp = req;
	return 0;
}

int davici_new_cmd(const char *cmd, struct davici_request **rp)
{
	return create_request(DAVICI_CMD_REQUEST, cmd, rp);
}

static void* add_element(struct davici_request *r, enum davici_element type,
						 unsigned int size)
{
	unsigned int newlen;
	void *ret, *new;

	if (r->used + size + 1 > r->allocated)
	{
		newlen = r->allocated;
		while (newlen < r->used + size + 1)
		{
			newlen *= 2;
		}
		new = realloc(r->buf, newlen);
		if (!new)
		{
			r->err = -errno;
			return NULL;
		}
		r->buf = new;
		r->allocated = newlen;
	}
	r->buf[r->used++] = type;
	ret = r->buf + r->used;
	r->used += size;
	return ret;
}

void davici_section_start(struct davici_request *r, const char *name)
{
	uint8_t nlen;
	char *pos;

	nlen = strlen(name);
	pos = add_element(r, DAVICI_SECTION_START, 1 + nlen);
	if (pos)
	{
		pos[0] = nlen;
		memcpy(pos + 1, name, nlen);
	}
}

void davici_section_end(struct davici_request *r)
{
	add_element(r, DAVICI_SECTION_END, 0);
}

void davici_kv(struct davici_request *r, const char *name,
			   const void *buf, unsigned int buflen)
{
	uint8_t nlen;
	uint16_t vlen;
	char *pos;

	nlen = strlen(name);
	pos = add_element(r, DAVICI_KEY_VALUE, 1 + nlen + sizeof(vlen) + buflen);
	if (pos)
	{
		pos[0] = nlen;
		memcpy(pos + 1, name, nlen);
		vlen = htons(buflen);
		memcpy(pos + 1 + nlen, &vlen, sizeof(vlen));
		memcpy(pos + 1 + nlen + sizeof(vlen), buf, buflen);
	}
}

void davici_kvf(struct davici_request *r, const char *name,
				const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	davici_vkvf(r, name, fmt, args);
	va_end(args);
}

void davici_vkvf(struct davici_request *r, const char *name,
				 const char *fmt, va_list args)
{
	char buf[512], *m = buf;
	va_list copy;
	int len;

	va_copy(copy, args);
	len = vsnprintf(buf, sizeof(buf), fmt, copy);
	va_end(copy);
	if (len >= sizeof(buf))
	{
		m = malloc(len + 1);
		if (m)
		{
			len = vsnprintf(m, len + 1, fmt, args);
		}
		else
		{
			len = -1;
		}
	}
	if (len < 0)
	{
		r->err = -errno;
	}
	else
	{
		davici_kv(r, name, m, len);
	}
	if (m != buf)
	{
		free(m);
	}
}

void davici_list_start(struct davici_request *r, const char *name)
{
	uint8_t nlen;
	char *pos;

	nlen = strlen(name);
	pos = add_element(r, DAVICI_LIST_START, 1 + nlen);
	if (pos)
	{
		pos[0] = nlen;
		memcpy(pos + 1, name, nlen);
	}
}

void davici_list_item(struct davici_request *r, const void *buf,
					  unsigned int buflen)
{
	uint16_t vlen;
	char *pos;

	pos = add_element(r, DAVICI_LIST_ITEM, sizeof(vlen) + buflen);
	if (pos)
	{
		vlen = htons(buflen);
		memcpy(pos, &vlen, sizeof(vlen));
		memcpy(pos + sizeof(vlen), buf, buflen);
	}
}

void davici_list_itemf(struct davici_request *r, const char *fmt, ...)
{

	va_list args;

	va_start(args, fmt);
	davici_list_vitemf(r, fmt, args);
	va_end(args);
}

void davici_list_vitemf(struct davici_request *r, const char *fmt, va_list args)
{
	char buf[512], *m = buf;
	va_list copy;
	int len;

	va_copy(copy, args);
	len = vsnprintf(buf, sizeof(buf), fmt, copy);
	va_end(copy);
	if (len >= sizeof(buf))
	{
		m = malloc(len + 1);
		if (m)
		{
			len = vsnprintf(m, len + 1, fmt, args);
		}
		else
		{
			len = -1;
		}
	}
	if (len < 0)
	{
		r->err = -errno;
	}
	else
	{
		davici_list_item(r, m, len);
	}
	if (m != buf)
	{
		free(m);
	}
}

void davici_list_end(struct davici_request *r)
{
	add_element(r, DAVICI_LIST_END, 0);
}

void davici_cancel(struct davici_request *r)
{
	free(r->buf);
	free(r);
}

static void append_req(struct davici_conn *c, struct davici_request *r)
{
	struct davici_request *cur;

	if (!c->reqs)
	{
		c->reqs = r;
	}
	else
	{
		cur = c->reqs;
		while (cur->next)
		{
			cur = cur->next;
		}
		cur->next = r;
	}
}

int davici_queue(struct davici_conn *c, struct davici_request *r,
				 davici_cb cmd_cb, void *user)
{
	int err;

	if (r->err)
	{
		err = r->err;
		davici_cancel(r);
		return err;
	}
	r->cb = cmd_cb;
	r->user = user;

	append_req(c, r);

	return update_ops(c, c->ops | DAVICI_WRITE);
}

int davici_queue_streamed(struct davici_conn *c, struct davici_request *r,
						  davici_cb cmd_cb, const char *event,
						  davici_cb event_cb, void *user)
{
	int err;

	if (r->err)
	{
		err = r->err;
		davici_cancel(r);
		return err;
	}
	err = davici_register(c, event, event_cb, user);
	if (err)
	{
		return err;
	}
	err = davici_queue(c, r, cmd_cb, user);
	davici_unregister(c, event, event_cb, user);
	return err;
}

unsigned int davici_queue_len(struct davici_conn *c)
{
	struct davici_request *req;
	unsigned int count = 0;

	req = c->reqs;
	while (req)
	{
		req = req->next;
		count++;
	}
	return count;
}

int davici_register(struct davici_conn *c, const char *event,
					davici_cb cb, void *user)
{
	struct davici_request *req;
	int err;

	err = create_request(DAVICI_EVENT_REGISTER, event, &req);
	if (err)
	{
		return err;
	}
	req->cb = cb;
	req->user = user;
	append_req(c, req);

	return update_ops(c, c->ops | DAVICI_WRITE);
}

int davici_unregister(struct davici_conn *c, const char *event,
					  davici_cb cb, void *user)
{
	struct davici_request *req;
	int err;

	err = create_request(DAVICI_EVENT_UNREGISTER, event, &req);
	if (err)
	{
		return err;
	}
	req->cb = cb;
	req->user = user;
	append_req(c, req);

	return update_ops(c, c->ops | DAVICI_WRITE);
}

static int parse_name(struct davici_response *res)
{
	unsigned char len;
	int err;

	if (res->pos > res->pkt->received - sizeof(len))
	{
		return -EBADMSG;
	}
	len = res->pkt->buf[res->pos++];
	if (len > res->pkt->received - res->pos)
	{
		return -EBADMSG;
	}
	err = copy_name(res->name, sizeof(res->name),
					res->pkt->buf + res->pos, len);
	if (err < 0)
	{
		return err;
	}
	res->pos += len;
	return 0;
}

static int parse_value(struct davici_response *res)
{
	uint16_t len;

	if (res->pos > res->pkt->received - sizeof(len))
	{
		return -EBADMSG;
	}
	memcpy(&len, res->pkt->buf + res->pos, sizeof(len));
	len = ntohs(len);
	res->pos += sizeof(len);
	if (len > res->pkt->received - res->pos)
	{
		return -EBADMSG;
	}
	res->buf = res->pkt->buf + res->pos;
	res->buflen = len;
	res->pos += len;
	return 0;
}

int davici_parse(struct davici_response *res)
{
	int type, err;

	if (res->pos == res->pkt->received)
	{
		if (res->list || res->section)
		{
			return -EBADMSG;
		}
		res->pos = 0;
		return DAVICI_END;
	}
	if (res->pos > res->pkt->received)
	{
		return -EINVAL;
	}
	type = res->pkt->buf[res->pos++];
	switch (type)
	{
		case DAVICI_SECTION_START:
			if (res->list)
			{
				return -EBADMSG;
			}
			res->section++;
			err = parse_name(res);
			if (err < 0)
			{
				return err;
			}
			return type;
		case DAVICI_LIST_START:
			if (res->list)
			{
				return -EBADMSG;
			}
			err = parse_name(res);
			if (err < 0)
			{
				return err;
			}
			res->list++;
			return type;
		case DAVICI_LIST_ITEM:
			if (!res->list)
			{
				return -EBADMSG;
			}
			err = parse_value(res);
			if (err < 0)
			{
				return err;
			}
			return type;
		case DAVICI_KEY_VALUE:
			if (res->list)
			{
				return -EBADMSG;
			}
			err = parse_name(res);
			if (err < 0)
			{
				return err;
			}
			err = parse_value(res);
			if (err < 0)
			{
				return err;
			}
			return type;
		case DAVICI_SECTION_END:
			if (!res->section || res->list)
			{
				return -EBADMSG;
			}
			res->section--;
			return type;
		case DAVICI_LIST_END:
			if (!res->list)
			{
				return -EBADMSG;
			}
			res->list--;
			return type;
		default:
			return -EBADMSG;
	}
}

int davici_recurse(struct davici_response *res, davici_recursecb section,
				   davici_recursecb li, davici_recursecb kv, void *user)
{
	int type, err;

	while (1)
	{
		type = davici_parse(res);
		switch (type)
		{
			case DAVICI_SECTION_START:
				if (section)
				{
					err = section(res, user);
				}
				else
				{
					err = davici_recurse(res, NULL, NULL, NULL, NULL);
				}
				if (err < 0)
				{
					return err;
				}
				break;
			case DAVICI_KEY_VALUE:
				if (kv)
				{
					err = kv(res, user);
					if (err < 0)
					{
						return err;
					}
				}
				break;
			case DAVICI_LIST_START:
				while (1)
				{
					type = davici_parse(res);
					switch (type)
					{
						case DAVICI_LIST_ITEM:
							if (li)
							{
								err = li(res, user);
								if (err < 0)
								{
									return err;
								}
							}
							continue;
						case DAVICI_LIST_END:
							break;
						default:
							if (type < 0)
							{
								return type;
							}
							return -EBADMSG;
					}
					break;
				}
				break;
			case DAVICI_SECTION_END:
			case DAVICI_END:
				return 0;
			default:
				if (type < 0)
				{
					return type;
				}
				return -EBADMSG;
		}
	}
}

unsigned int davici_get_level(struct davici_response *res)
{
	if (res->list)
	{
		return res->section + 1;
	}
	return res->section;
}

const char* davici_get_name(struct davici_response *res)
{
	return res->name;
}

int davici_name_strcmp(struct davici_response *res, const char *str)
{
	return strcmp(res->name, str);
}

const void* davici_get_value(struct davici_response *res, unsigned int *len)
{
	*len = res->buflen;
	return res->buf;
}

int davici_value_scanf(struct davici_response *res, const char *fmt, ...)
{
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = davici_value_vscanf(res, fmt, args);
	va_end(args);
	return ret;
}

int davici_value_vscanf(struct davici_response *res, const char *fmt,
						va_list args)
{
	char buf[1024];
	int err;

	err = davici_get_value_str(res, buf, sizeof(buf));
	if (err < 0)
	{
		return err;
	}
	return vsscanf(buf, fmt, args);
}

int davici_get_value_str(struct davici_response *res,
						 char *buf, unsigned int buflen)
{
	const char *val = res->buf;
	int i, len;

	for (i = 0; i < res->buflen; i++)
	{
		if (!isprint((unsigned char)val[i]))
		{
			return -EINVAL;
		}
	}
	len = snprintf(buf, buflen, "%.*s", res->buflen, val);
	if (len < 0)
	{
		return -errno;
	}
	if (len >= buflen)
	{
		return -ENOBUFS;
	}
	return len;
}

int davici_value_strcmp(struct davici_response *res, const char *str)
{
	int ret;

	ret = strncmp(res->buf, str, res->buflen);
	if (ret)
	{
		return ret;
	}
	if (strlen(str) == res->buflen)
	{
		return 0;
	}
	return -1;
}

int davici_dump(struct davici_response *res, const char *name, const char *sep,
				unsigned int level, unsigned int indent, FILE *out)
{
	ssize_t len, total = 0;
	char buf[4096];
	int err;

	len = fprintf(out, "%*s%s {%s", level * indent, "", name, sep);
	if (len < 0)
	{
		return -errno;
	}
	level++;
	total += len;
	while (1)
	{
		err = davici_parse(res);
		switch (err)
		{
			case DAVICI_END:
				level--;
				len = fprintf(out, "%*s}", level * indent, "");
				if (len < 0)
				{
					return -errno;
				}
				return total + len;
			case DAVICI_SECTION_START:
				len = fprintf(out, "%*s%s {%s", level * indent, "",
							  res->name, sep);
				level++;
				break;
			case DAVICI_SECTION_END:
				level--;
				len = fprintf(out, "%*s}%s", level * indent, "", sep);
				break;
			case DAVICI_KEY_VALUE:
				err = davici_get_value_str(res, buf, sizeof(buf));
				if (err < 0)
				{
					return err;
				}
				len = fprintf(out, "%*s%s = %s%s", level * indent, "",
							  res->name, buf, sep);
				break;
			case DAVICI_LIST_START:
				len = fprintf(out, "%*s%s [%s", level * indent, "",
							  res->name, sep);
				level++;
				break;
			case DAVICI_LIST_ITEM:
				err = davici_get_value_str(res, buf, sizeof(buf));
				if (err < 0)
				{
					return err;
				}
				len = fprintf(out, "%*s%s%s", level * indent, "", buf, sep);
				break;
			case DAVICI_LIST_END:
				level--;
				len = fprintf(out, "%*s]%s", level * indent, "", sep);
				break;
			default:
				return err;
		}
		if (len < 0)
		{
			return -errno;
		}
		total += len;
	}
}
