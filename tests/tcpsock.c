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

#include "tester.h"

#include <assert.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <unistd.h>

void *mock_server(void *add_port)
{
    unsigned int port = (__UINTPTR_TYPE__)add_port;
    int server_fd;
    struct sockaddr_in address;
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(server_fd >= 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    assert(bind(server_fd, (struct sockaddr *)&address, sizeof(address)) >= 0);
    assert(listen(server_fd, 3) >= 0);
    sleep(1);
    return (void *)(__intptr_t)close(server_fd);
}

int iocb(struct davici_conn *c, int fd, int ops, void *user)
{
    assert(0);
}

int main(int argc, char *argv[])
{
    struct davici_conn *c;
    pthread_t thread_id;
    void *port = (void *)55555;
    pthread_create(&thread_id, NULL, mock_server, port);

    int ret = davici_connect_tcpip("127.0.0.1:55555", iocb, NULL, &c);

    pthread_join(thread_id, NULL);
    assert(ret == 0);
    return 0;
}
