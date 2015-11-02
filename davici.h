/*
 * Copyright (c) 2015 CloudGuard Software AG. All rights reserved.
 */

#ifndef _DAVICI_H_
#define _DAVICI_H_

#include <stdio.h>

/**
 * Opaque connection context.
 */
struct davici_conn;

/**
 * Opaque request message context.
 */
struct davici_request;

/**
 * Opaque response message context.
 */
struct davici_response;

/**
 * Parsed message element
 */
enum davici_element {
	/** valid end of message */
	DAVICI_END = 0,
	/** begin of a section */
	DAVICI_SECTION_START,
	/** end of a section */
	DAVICI_SECTION_END,
	/** key/value pair */
	DAVICI_KEY_VALUE,
	/** begin of a list */
	DAVICI_LIST_START,
	/** list item */
	DAVICI_LIST_ITEM,
	/** end of a list */
	DAVICI_LIST_END,
};

/**
 * File descriptor watch operations requested.
 */
enum davici_fdops {
	/** request read-ready notifications */
	DAVICI_READ = (1<<0),
	/** request write-ready notifications */
	DAVICI_WRITE = (1<<1),
};

/**
 * Prototype for a command response or event callback function.
 *
 * @param conn		opaque connection context
 * @param err		0 on success, or a negative receive errno
 * @param name		command or event name raising the callback
 * @param res		event or response message
 * @param user		user context passed to registration function
 */
typedef void (*davici_cb)(struct davici_conn *conn, int err, const char *name,
						  struct davici_response *res, void *user);

/**
 * Prototype for a file descriptor watch update callback.
 *
 * @param conn		opaque connection context
 * @param fd		file descriptor to watch
 * @param ops		watch operations to register for, enum davici_fdops
 * @param user		user context passed during connection
 * @return			0 if watch updated, or a negative errno
 */
typedef int (*davici_fdcb)(struct davici_conn *conn, int fd, int ops,
						   void *user);

/**
 * Create a connection to a VICI Unix socket.
 *
 * @param path		path to Unix socket
 * @param cp		pointer receiving opaque connection handle
 * @param fdcb		callback to register for file descriptor watching
 * @param user		user context to pass to fdcb
 * @param connp		pointer receiving connection context on success
 * @return			0 on success, or a negative errno
 */
int davici_connect_unix(const char *path, davici_fdcb fdcb, void *user,
						struct davici_conn **connp);

/**
 * Read and process pending connection data.
 *
 * Performs a non-blocking read on the connection, and dispatches any
 * received response or event message.
 *
 * @param conn		opaque connection context
 * @return			0 on success, or a negative errno
 */
int davici_read(struct davici_conn *conn);

/**
 * Write queued request data to the connection.
 *
 * Performs a non-blocking write on the connection for any currently queued
 * message.
 *
 * @param conn		opaque connection context
 * @return			0 on success, or a negative errno
 */
int davici_write(struct davici_conn *conn);

/**
 * Close an open VICI connection and free associated resources.
 *
 * @param conn		opaque connection context
 */
void davici_disconnect(struct davici_conn *conn);

/**
 * Allocate a new request command message.
 *
 * @param cmd		command name
 * @param reqp		receives allocated request context
 * @return			0 on success, or a negative errno
 */
int davici_new_cmd(const char *cmd, struct davici_request **reqp);

/**
 * Begin a new section on a request message.
 *
 * @param req		request context
 * @param name		name of section to open
 */
void davici_section_start(struct davici_request *req, const char *name);

/**
 * End a section previously opened on a request message.
 *
 * @param req		request context
 */
void davici_section_end(struct davici_request *req);

/**
 * Add a key/value element to the request message.
 *
 * @param req		request context
 * @param name		key name
 * @param buf		value buffer
 * @param buflen	size, in bytes, of buf
 */
void davici_kv(struct davici_request *req, const char *name,
			   const void *buf, unsigned int buflen);

/**
 * Add a key with a format string value to the request message.
 *
 * @param req		request context
 * @param name		key name
 * @param fmt		format string for value
 * @param ...		arguments for fmt string
 */
void davici_kvf(struct davici_request *req, const char *name,
				const char *fmt, ...);

/**
 * Add a key with a format string value and a va_list to the request message.
 *
 * @param req		request context
 * @param name		key name
 * @param fmt		format string for value
 * @param args		argument list for fmt string
 */
void davici_vkvf(struct davici_request *req, const char *name,
				 const char *fmt, va_list args);

/**
 * Begin a list of unnamed items in a request message.
 *
 * @param req			request context
 * @param name		list name to open
 */
void davici_list_start(struct davici_request *req, const char *name);

/**
 * Add a list item value to a previously opened list.
 *
 * @param req		request context
 * @param buf		value buffer
 * @param buflen	size, in bytes, of buf
 */
void davici_list_item(struct davici_request *req, const void *buf,
					  unsigned int buflen);

/**
 * Add a list item with a format string value to a previously opened list.
 *
 * @param req		request context
 * @param fmt		format string for value
 * @param ...		arguments for fmt string
 */
void davici_list_itemf(struct davici_request *req, const char *fmt, ...);

/**
 * Add a list item with a format string value and a va_list.
 *
 * @param req		request context
 * @param fmt		format string for value
 * @param args		argument list for fmt string
 */
void davici_list_vitemf(struct davici_request *req, const char *fmt,
						va_list args);

/**
 * End a list previously opened for a request message.
 *
 * @param req		request context
 */
void davici_list_end(struct davici_request *req);

/**
 * Clean up a request if it is not passed to davici_queue().
 *
 * Messages get automatically cleaned up when passed to davici_queue() or
 * after it has been fed to callbacks. Use this call only if an allocated
 * message does not get queued(), but the request is aborted.
 *
 * @param req		request context to free
 */
void davici_cancel(struct davici_request *req);

/**
 * Queue a command request message for submission.
 *
 * The request gets queued, and is sent to the daemon with davici_write()
 * when the daemon accepts new data and all previously queued requests have
 * been processed. Once the daemon responds to the request, davici_read()
 * invokes the passed callback with the response message and the provided
 * user context.
 *
 * @param conn		connection context
 * @param req		request message to queue
 * @param cb		callback to invoke for response message
 * @param user		user context to pass to callback
 * @return			0 on success, or a negative errno
 */
int davici_queue(struct davici_conn *conn, struct davici_request *req,
				 davici_cb cb, void *user);

/**
 * Queue a command request using event based streaming.
 *
 * In addition to davici_queue(), this call register for an event while
 * the command is active and invokes an event callback for each streamed
 * object. Upon completion of the call, it invokes the result callback.
 *
 * @param conn		connection context
 * @param req		request message to queue
 * @param res_cb	callback to invoke for response message
 * @param event		streamed event name to register for
 * @param event_cb	event callback invoked for each streamed event message
 * @param user		user context to pass to callbacks
 * @return			0 on success, or a negative errno
 */
int davici_queue_streamed(struct davici_conn *conn, struct davici_request *req,
						  davici_cb res_cb, const char *event,
						  davici_cb event_cb, void *user);

/**
 * Register for event messages.
 *
 * Event registration is asynchronous; once registration completed the
 * callback is invoked with a NULL event message. The error code indicates
 * the registration status.
 *
 * @param conn		connection context
 * @param event		event name to register
 * @param cb		callback to invoke on events
 * @param user		user context to pass to cb
 * @return			0 on success, or a negative errno
 */
int davici_register(struct davici_conn *conn, const char *event,
					davici_cb cb, void *user);

/**
 * Unregister for event messages.
 *
 * Event deregistration is asynchronous; once deregistration completed the
 * callback is invoked with a NULL event message. The error code indicates
 * the deregistration status.
 *
 * @param conn		connection context
 * @param event		event name to unregister
 * @param cb		callback to invoke on events
 * @param user		user context to pass to cb
 * @return			0 on success, or a negative errno
 */
int davici_unregister(struct davici_conn *conn, const char *event,
					  davici_cb cb, void *user);

/**
 * Parse a response or event message.
 *
 * @param res		response or event message
 * @return			enum davici_element, or a negative errno
 */
int davici_parse(struct davici_response *res);

/**
 * Get the element name previously parsed in davici_parse().
 *
 * This call has defined behavior only if davici_parse() returned an element,
 * with a name, i.e. a section/list start or a key/value.
 *
 * @param rres		response or event message
 * @return			element name
 */
const char* davici_get_name(struct davici_response *res);

/**
 * Get the element value previously parsed in davici_parse().
 *
 * This call has defined behavior only if davici_parse() returned an element,
 * with a value, i.e. a section/list start or a key/value.
 *
 * @param res		response or event message
 * @param len		pointer receiving buffer length
 * @return			buffer containing value
 */
const void* davici_get_value(struct davici_response *res, unsigned int *len);

/**
 * Get the element value if it is a string.
 *
 * This is a convenience function to get the value of an element as string,
 * the same restirctions as to davici_get_value() apply. The function fails
 * the value has any non-printable characters.
 *
 * @param res		response or event message context
 * @param buf		buffer to write string to
 * @param buflen	size of the buffer
 * @return			number of characters written, or a negative errno
 */
int davici_get_value_str(struct davici_response *res,
						 char *buf, unsigned int buflen);

/**
 * Dump a response or event message to a FILE stream.
 *
 * @param res		response or event message context
 * @param name		name of the message to dump
 * @param sep		value separator, such as "\n"
 * @param level		base identation level
 * @param ident		number of spaces to use for identation
 * @param out		FILE stream to write to
 * @return			total bytes written, or a negative errno on error
 */
int davici_dump(struct davici_response *res, const char *name, const char *sep,
				unsigned int level, unsigned int ident, FILE *out);

#endif /* _DAVICI_H_ */
