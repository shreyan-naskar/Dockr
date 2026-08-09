/*
    demo/client.c

    Purpose:
    --------
    TCP client used to prove host-to-container networking works.

    Where this runs:
    ----------------
    This program runs on the HOST.

    The server runs inside the container.

    Example flow:
    -------------

    Terminal 1:

        sudo ./build/mini-container ./rootfs host-1 192.168.1.2 /bin/sh

    Inside container:

        cd /var/shared_folder
        ./server 8080

    Terminal 2 on host:

        ./build/client 192.168.1.2 8080

    If networking works, the server inside the container receives messages
    from this host-side client.
*/

#include <arpa/inet.h>   /* inet_pton */
#include <errno.h>       /* errno */
#include <netinet/in.h>  /* sockaddr_in */
#include <stdio.h>       /* printf, perror, fprintf */
#include <stdlib.h>      /* exit, atoi */
#include <string.h>      /* memset, strlen */
#include <sys/socket.h>  /* socket, connect */
#include <unistd.h>      /* write, sleep, close */

/*
    die()
    -----

    Prints syscall error and exits.
*/
static void die(const char* message)
{
    perror(message);
    exit(EXIT_FAILURE);
}

/*
    connect_to_server()
    -------------------

    Creates a TCP socket and connects it to the container server.

    Parameters:
        ip:
            Container IP address.

            Example:
                192.168.1.2

        port:
            Server port.

            Example:
                8080

    Returns:
        connected socket file descriptor.
*/
static int connect_to_server(const char* ip, int port)
{
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (sock_fd == -1)
    {
        die("socket");
    }

    struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((unsigned short)port);

    /*
        inet_pton()
        -----------
        Converts text IP address into binary network format.

        It is safer than inet_addr().
    */
    int result = inet_pton(
        AF_INET,
        ip,
        &server_addr.sin_addr
    );

    if (result == 0)
    {
        fprintf(stderr, "Invalid IPv4 address: %s\n", ip);
        exit(EXIT_FAILURE);
    }

    if (result == -1)
    {
        die("inet_pton");
    }

    if (connect(
            sock_fd,
            (struct sockaddr*)&server_addr,
            sizeof(server_addr)
        ) == -1)
    {
        die("connect");
    }

    return sock_fd;
}

/*
    send_loop()
    -----------

    Sends a message every second forever.

    This makes it easy to watch server output inside the container.
*/
static void send_loop(int sock_fd, const char* ip, int port)
{
    const char* message = "Hello from host client";

    while (1)
    {
        ssize_t bytes_written = write(
            sock_fd,
            message,
            strlen(message)
        );

        if (bytes_written == -1)
        {
            die("write");
        }

        printf(
            "Sent %zd bytes to %s:%d\n",
            bytes_written,
            ip,
            port
        );

        sleep(1);
    }
}

/*
    main()
    ------

    Usage:
        ./client <server-ip> <port>

    Example:
        ./client 192.168.1.2 8080
*/
int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        fprintf(
            stderr,
            "Usage: %s <server-ip> <port>\n",
            argv[0]
        );

        return EXIT_FAILURE;
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]);

    if (port <= 0 || port > 65535)
    {
        fprintf(stderr, "Invalid port: %s\n", argv[2]);
        return EXIT_FAILURE;
    }

    int sock_fd = connect_to_server(ip, port);

    printf("Connected to %s:%d\n", ip, port);

    send_loop(sock_fd, ip, port);

    close(sock_fd);
    return EXIT_SUCCESS;
}