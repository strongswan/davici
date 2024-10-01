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

/**
 * @defgroup davici davici
 * @{
 *
 * Decoupled Asynchronous strongSwan VICI client library.
 */

#ifndef _DAVICI_H_
#define _DAVICI_H_

#include <stdio.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C"
{
#endif

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
	 * Parsed message element.
	 */
	enum davici_element
	{
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
	enum davici_fdops
	{
		/** request read-ready notifications */
		DAVICI_READ = (1 << 0),
		/** request write-ready notifications */
		DAVICI_WRITE = (1 << 1),
	};

	/**
	 * Prototype for a command response or event callback function.
	 *
	 * This kind of callback is invoked for command response and event callbacks.
	 * The err parameter indicates any errors in issuing a command or registering
	 * for an event.
	 *
	 * For event registration, also implicitly for streamed command messages using
	 * events, the callback gets invoked with a NULL res parameter both after
	 * registration and deregistration to indicate any error conditions.
	 *
	 * Command and event registration responses do not carry a name on the wire,
	 * but the name parameter is populated with the name of the issued event
	 * registration or command request.
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
	 * The watch update callback requests (or updates) a file descriptor watch from
	 * the user. The fd parameter is a file descriptor the user shall monitor
	 * with select(), poll() or similar functionality. The ops argument is an ORed
	 * set of enum davici_fdops flags indicating for what kind of file descriptor
	 * events monitoring should be set up.
	 *
	 * The callback may be invoked from any davici function multiple times, but
	 * is guaranteed to get called with a zero ops parameter before returning
	 * from davici_disconnect().
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
	 * Prototype for a recursive parsing callback.
	 *
	 * This callback is used by davici_recurse(), a recursive VICI message parser.
	 * The same kind of callback is used for section, list item and key/value
	 * elements found in a message.
	 *
	 * If this callback returns a negative errno, parsing is aborted and the
	 * same errno is returned from davici_recurse().
	 *
	 * @param res		response or event message parsing
	 * @param user		user context, as passed to davici_recurse()
	 * @return			a negative errno on error to stop parsing
	 */
	typedef int (*davici_recursecb)(struct davici_response *res, void *user);

	/**
	 * Create a connection to a VICI Unix socket.
	 *
	 * Opens a Unix socket connection to a VICI service under path, using a
	 * file descriptor monitoring callback function as discussed above.
	 *
	 * Please note that this function uses connect() on a blocking socket, which
	 * in theory is a blocking call.
	 *
	 * @param path		path to Unix socket
	 * @param fdcb		callback to register for file descriptor watching
	 * @param user		user context to pass to fdcb
	 * @param connp		pointer receiving connection context on success
	 * @return			0 on success, or a negative errno
	 */
	int davici_connect_unix(const char *path, davici_fdcb fdcb, void *user,
							struct davici_conn **connp);

	/**
	 * Create a connection to a VICI TCP/IP socket.
	 *
	 * Opens a TCP/IP socket connection to a VICI service under address, using a
	 * file descriptor monitoring callback function as discussed above.
	 *
	 * Please note that this function uses connect() on a blocking socket, which
	 * in theory is a blocking call.
	 *
	 * Using TCP could pose a security issue. Messages sent to it are not encrypted
	 * and so this should only be used in a controlled, private network environment
	 * and not over public networks.
	 *
	 * @param address	address (ip:port) to TCP/IP socket
	 * @param fdcb		callback to register for file descriptor watching
	 * @param user		user context to pass to fdcb
	 * @param connp		pointer receiving connection context on success
	 * @return			0 on success, or a negative errno
	 */
	int davici_connect_tcpip(const char *address, davici_fdcb fdcb, void *user,
							 struct davici_conn **cp);

	/**
	 * Read and process pending connection data.
	 *
	 * Performs a non-blocking read on the connection, and dispatches any
	 * received response or event message. The call uses non-blocking reads
	 * only, and returns with a successful result if the call would block. It is
	 * usually an unrecoverable error if a negative errno is returned, and the
	 * user should call davici_disconnect().
	 *
	 * @param conn		opaque connection context
	 * @return			0 on success, or a negative errno
	 */
	int davici_read(struct davici_conn *conn);

	/**
	 * Write queued request data to the connection.
	 *
	 * Performs a non-blocking write on the connection for any currently queued
	 * message. The call uses non-blocking reads only, and returns with a
	 * successful result if the call would block. It is usually an unrecoverable
	 * error if a negative errno is returned, and the user should call
	 * davici_disconnect().
	 *
	 * @param conn		opaque connection context
	 * @return			0 on success, or a negative errno
	 */
	int davici_write(struct davici_conn *conn);

	/**
	 * Close an open VICI connection and free associated resources.
	 *
	 * Frees any resources associated to a connection, and invokes the file
	 * descriptor watch callback of the connection with zero-flags if a monitoring
	 * registration is active.
	 *
	 * Any request messages created with davici_new_cmd() but not queued to
	 * the connection using either davici_queue() or davici_queue_streamed()
	 * is not freed by this call. Such requests must be freed explicitly by calling
	 * davici_cancel() on them.
	 *
	 * @param conn		opaque connection context
	 */
	void davici_disconnect(struct davici_conn *conn);

	/**
	 * Allocate a new request command message.
	 *
	 * Creates a new but empty request message for a named command. The returned
	 * handle must be passed to davici_queue() or davici_queue_streamed() after
	 * adding request data. If the connection gets closed before the request
	 * could be queued, the request must be freed using davici_cancel().
	 *
	 * @param cmd		command name
	 * @param reqp		receives allocated request context
	 * @return			0 on success, or a negative errno
	 */
	int davici_new_cmd(const char *cmd, struct davici_request **reqp);

	/**
	 * Begin a new section on a request message.
	 *
	 * Begin a new named section at the current request position. Any started
	 * section must have a corresponding davici_section_end() added to form
	 * a valid request message. Creating sections within lists is invalid.
	 *
	 * @param req		request context
	 * @param name		name of section to open
	 */
	void davici_section_start(struct davici_request *req, const char *name);

	/**
	 * End a section previously opened on a request message.
	 *
	 * Close a section previously started with davici_section_start(). The
	 * call must be balanced with davici_section_start() to form a valid request
	 * message.
	 *
	 * @param req		request context
	 */
	void davici_section_end(struct davici_request *req);

	/**
	 * Add a key/value element to the request message.
	 *
	 * Adds a new key/value pair at the current request position. Key/values
	 * may be added to any explicit or the implicit root section, but not to
	 * lists.
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
	 * Similar to davici_kv(), but instead of a fixed buffer takes a printf()
	 * format string and arguments to form the value of the key/value pair.
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
	 * Very similar to davici_kvf(), but takes a va_list argument list for the
	 * format string instead of variable arguments.
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
	 * Starts a new list at the current request message position. Lists may appear
	 * in any section, but not in lists. Started lists must be closed by
	 * davici_list_end(), and both calls must be balanced to form a valid request
	 * message.
	 *
	 * @param req		request context
	 * @param name		list name to open
	 */
	void davici_list_start(struct davici_request *req, const char *name);

	/**
	 * Add a list item value to a previously opened list.
	 *
	 * List items may be added to lists only, and are invalid in any other context.
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
	 * Similar to davici_list_item(), but instead of a fixed buffer takes a
	 * printf() format string and variable arguments to form the list item value.
	 *
	 * @param req		request context
	 * @param fmt		format string for value
	 * @param ...		arguments for fmt string
	 */
	void davici_list_itemf(struct davici_request *req, const char *fmt, ...);

	/**
	 * Add a list item with a format string value and a va_list.
	 *
	 * Very similar to davici_list_itemf(), but instead of variable arguments
	 * takes a va_list for the printf() format string.
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
	 * Closes the currently open list; valid only in a list context.
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
	 * In addition to davici_queue(), this call registers for an event while
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
	 * Get the count of all queued davici request messages.
	 *
	 * @param conn		connection context
	 * @return			number of request messages in queue
	 */
	unsigned int davici_queue_len(struct davici_conn *conn);

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
	 * davici_parse() implements iterative parsing of response messages by
	 * returning enum davici_element types as parsing status. For named types
	 * davici_get_name() may be called to get the name of the just parsed element.
	 * For types having a value, davici_get_value() returns the value of the
	 * just parsed element.
	 *
	 * If parsing completes with DAVICI_END it may be parsed again using
	 * davici_parse(). When davici_parse() returns an error, additional calls
	 * to davici_parse() have undefined behavior.
	 *
	 * @param res		response or event message
	 * @return			enum davici_element, or a negative errno
	 */
	int davici_parse(struct davici_response *res);

	/**
	 * Recursive response or event message parser.
	 *
	 * Using davici_parse(), davici_recurse() parses a VICI message by invoking
	 * callbacks for the found elements. Three callbacks can be passed, but all
	 * are optional to not further handle an element or descent into a section.
	 *
	 * If a section callback is given, that callback must invoke davici_recurse()
	 * to parse the inner context, or it may return a negative errno. The return
	 * value from the recursive davici_recurse() invocation should be returned
	 * from the section callback to properly propagate inner errors.
	 *
	 * For any callback, davici_get_name() or davici_name_strcmp() can be called
	 * to get the name of the element for which the callback is called. For lists,
	 * the li callback is invoked for items only; davici_get_name() in a list item
	 * callback returns the name of the list.
	 *
	 * davici_get_value() can be used in list item and key/value callbacks to get
	 * the value of the element the callback is invoked for.
	 *
	 * @param res		response or event message
	 * @param section	section callback, NULL to skip sub-sections
	 * @param li		list item callback, NULL to ignore
	 * @param kv		key/value callback, NULL to ignore
	 * @return			a negative errno on error
	 */
	int davici_recurse(struct davici_response *res, davici_recursecb section,
					   davici_recursecb li, davici_recursecb kv, void *ctx);

	/**
	 * Get the section/list nesting level of the current parser position.
	 *
	 * The root section has a level of 0 and for each nested section the level is
	 * incremented by one. When parsing a list, the level is incremented by one.
	 * The level after parsing a section/list start is increased just after
	 * davici_parse() is called for such an element, while for end elements the
	 * level is decreased in davici_parse() for that element.
	 *
	 * @param res		response or event message
	 * @return			section/list nesting level
	 */
	unsigned int davici_get_level(struct davici_response *res);

	/**
	 * Get the element name previously parsed in davici_parse().
	 *
	 * This call has defined behavior only if davici_parse() returned an element,
	 * with a name, i.e. a section/list start or a key/value.
	 *
	 * @param res		response or event message
	 * @return			element name
	 */
	const char *davici_get_name(struct davici_response *res);

	/**
	 * Compare the previously parsed element name against a given string.
	 *
	 * This call has defined behavior only if davici_parse() returned an element,
	 * with a name, i.e. a section/list start or a key/value.
	 *
	 * @param res		response or event message
	 * @param str		string to compare against name
	 * @return			strcmp() result
	 */
	int davici_name_strcmp(struct davici_response *res, const char *str);

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
	const void *davici_get_value(struct davici_response *res, unsigned int *len);

	/**
	 * Get the element value if it is a string.
	 *
	 * This is a convenience function to get the value of an element as string,
	 * the same restrictions as to davici_get_value() apply. The function fails
	 * if the value has any non-printable characters.
	 *
	 * @param res		response or event message context
	 * @param buf		buffer to write string to
	 * @param buflen	size of the buffer
	 * @return			number of characters written, or a negative errno
	 */
	int davici_get_value_str(struct davici_response *res,
							 char *buf, unsigned int buflen);

	/**
	 * Convert value using a format specifier.
	 *
	 * This functions handles the value like a string, and then uses a scanf()
	 * format string to convert the value to the passed argument pointers.
	 *
	 * @param res		response or event message context
	 * @param fmt		scanf() format string
	 * @param ...		arguments of format string
	 * @return			number of input items matched, or a negative errno
	 */
	int davici_value_scanf(struct davici_response *res, const char *fmt, ...);

	/**
	 * Convert value using a format specifier, va_list variant.
	 *
	 * Like davici_value_scanf(), but takes arguments in a va_list.
	 *
	 * @param res		response or event message context
	 * @param fmt		scanf() format string
	 * @param args		arguments of format string
	 * @return			number of input items matched, or a negative errno
	 */
	int davici_value_vscanf(struct davici_response *res, const char *fmt,
							va_list args);

/**
 * Convert value using a format specifier, error checking variant.
 *
 * Like davici_value_scanf(), but fails if either not the full value is
 * consumed or not all input items have been matched.
 *
 * Note that davici_value_escanf() is implemented as macro, and that
 * the arguments are evaluated more than once. fmt must be a string literal.
 *
 * @param res		response or event message context
 * @param fmt		scanf() format string
 * @param ...		arguments of format string
 * @return			a negative errno on error
 */
#define davici_value_escanf(res, fmt, ...) ({                 \
	unsigned int _v;                                          \
	int _m, _e, _c;                                           \
	_e = sizeof((void *[]){__VA_ARGS__}) / sizeof(void *);    \
	_m = davici_value_scanf(res, fmt "%n", __VA_ARGS__, &_c); \
	davici_get_value(res, &_v);                               \
	(_m == _e && _c == _v) ? 0 : -EBADMSG;                    \
})

	/**
	 * Compare the element value to a given string.
	 *
	 * This is a convenience function to compare the value of an element as string
	 * to a given string. The same restrictions as to davici_get_value() apply.
	 *
	 * @param res		response or event message
	 * @param str		string to compare against value
	 * @return			strcmp() result
	 */
	int davici_value_strcmp(struct davici_response *res, const char *str);

	/**
	 * Dump a response or event message to a FILE stream.
	 *
	 * The dump output is not considered stable, but solely for debugging
	 * purposes.
	 *
	 * @param res		response or event message context
	 * @param name		name of the message to dump
	 * @param sep		value separator, such as "\n"
	 * @param level		base indentation level
	 * @param indent	number of spaces to use for indentation
	 * @param out		FILE stream to write to
	 * @return			total bytes written, or a negative errno on error
	 */
	int davici_dump(struct davici_response *res, const char *name, const char *sep,
					unsigned int level, unsigned int indent, FILE *out);

#ifdef __cplusplus
}
#endif

#endif /* _DAVICI_H_ */

/**
 * @}
 */
