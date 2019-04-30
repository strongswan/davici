# davici - Decoupled Asynchronous VICI library #

The strongSwan VICI interface is an RPC-like interface to configure, monitor and
control the IKE daemon charon. It is implemented in the vici plugin and used
by the swanctl configuration backend. VICI stands for Versatile IKE
Configuration Interface, details about the protocol are provided in the
strongSwan documentation.

strongSwan ships client libraries for the VICI protocol implemented in Ruby
and Python, and the libvici library provides a client side implementation
of the protocol in C. The libvici library, however, uses the libstrongswan
base library, which is not well suited for integration in other software
stacks: libstrongswan is GPLv2-licensed, so is libvici, and it makes use of
threads for asynchronous event delivery.

davici is an alternative implementation of the VICI client protocol, targeting
better integration in other software stacks. It uses an asynchronous,
non-blocking API and can be integrated in third-party main dispatching loops
without the use of threads. davici is a very low-level library intended as a
foundation to build higher level control interfaces for strongSwan.

## I/O dispatching and main loop integration ##

davici requires integration in an asynchronous I/O dispatching framework
with file descriptors, such as ``select()``, ``poll()`` or similar interfaces.
Higher level main loop processing APIs can use davici as well, as long as they
provide a mechanism to watch for file descriptor state changes.

When creating a VICI connection with davici, the constructor takes a
``davici_fdcb`` callback function. Any davici function may invoke the passed
callback function to perform I/O dispatching registration. The callback
receives a file descriptor, and a request for a set of file descriptor
operations to monitor. That information must be used to add or update file
descriptor watch operations using ``select()``, ``poll()`` or similar calls.

When dispatching the passed file descriptor, the user code shall call
``davici_read()`` if the file descriptor is read-ready, and ``davici_write()``
if the file descriptor is write-ready. Both calls are non-blocking, and on
failure the VICI connection shall be closed using ``davici_disconnect()``.

## Use in multithreaded code ##

davici is not thread safe in the sense that multiple threads may operate on
a single connection concurrently. It does not perform any locking. Individual
threads may operate on individual connections, though, and multiple threads
may also use a single connection if any call to davici functions is
synchronized to a single concurrent thread.

## Invoking commands ##

Commands are client-initiated exchanges including a request message sent by
the client and a response message sent by the server. The ``davici_new_cmd()``
function creates a new and empty request message for a specific command, which
then may be populated using the different functions provided by davici.

Once the request is complete (and balanced in regards to sections/lists), it may
be queued for execution using ``davici_queue()``. The provided user callback is
invoked asynchronously once the server response is received or the exchange
failed. If a valid response has been received, a non-negative error value is
passed to the callback together with the response message. An arbitrary number
of requests may be queued, but they do get executed synchronized one by one on
a single connection.

Within the response callback the user may parse the response message using
``davici_parse()`` and associated functions. The function implements an
iterative parser to process any kind of response message.

## Streaming command response ##

Some commands in the VICI protocol use response streaming, that is, upon
receiving a request the vici plugin starts sending related event messages.
The end of the event stream is indicated by the response message matching
the request.

The ``davici_queue_streamed()`` function queues a request for streamed response
processing. It takes an additional event callback that is invoked for each
streamed response. The event callback is also invoked for the implicit event
registration with a NULL response to indicate any registration error.

As with the command response callback the user may use ``davici_parse()`` to
process messages in the event callback. One may even use the same callback
function for both usages, but care must be taken to properly handle the
registration/deregistration invocations properly.

## Event handling ##

To register for normal events, the ``davici_register()`` and
``davici_unregister()`` functions may be used. Both event registration and
deregistration are confirmed (asynchronously) by invoking the event callback
with a NULL response message. The error code indicates any failure, and should
be checked in all invocations.

To parse event messages, the callback may invoke ``davici_parse()`` and related
functions.
