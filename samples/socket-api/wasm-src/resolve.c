/*
 * Copyright (C) 2019 Intel Corporation.  All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */
#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#ifdef __wasi__
#include <wasi_socket_ext.h>
#endif

int
main(int argc, char *argv[])
{
    struct addrinfo res;
    int err;
    
    err = getaddrinfo("www.amazon.com", NULL, NULL, &res);
    if (err != 0)
    {
        printf("getaddrinfo returned error: %d\n", err);
        return 1;
    }

    return 0;
}
