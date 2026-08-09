/*
    demo/server.c

    Purpose:
    --------
    TCP server used to prove container networking works.

    Where this runs:
    ----------------
    This program is meant to run INSIDE the container.

    Build target:
        shared_folder/server

    Why shared_folder?
    ------------------
    The runtime bind-mounts ./shared_folder into the container at:

        /var/shared_folder

    So inside the container you can run:

        cd /var/shared_folder
        ./server 8080

    Then from the host you can connect using:

        ./build/client 192.168.1.2 8080
*/

#include <arpa/inet.h>   /* inet_ntoa */
#include <errno.h>       /* errno */
#include <netinet/in.h>  /* sockaddr_in, INADDR_ANY */
#include <stdio.h>       /* printf, perror, fprintf */
#include <stdlib.h>      /* exit, atoi */
#include <string.h>      /* memset */
#include <sys/socket.h>  /* socket, bind, listen, accept */
#include <unistd.h>      /* read, close */

/*
    die()
    -----

    Prints the reason for a failed system call and exits.

    Most socket APIs return -1 on failure and set errno.
    perror() prints a readable errno message.
*/
static void die(const char* message)
{
    perror(message);
    exit(EXIT_FAILURE);
}

/*
    create_listen_socket()
    ----------------------

    Creates a TCP socket, binds it to the requested port, and starts listening.

    Returns:
        file descriptor for the listening socket.
*/
static int create_listen_socket(int port)
{
    /*
        AF_INET:
            IPv4 socket.

        SOCK_STREAM:
            TCP socket.

        protocol 0:
            Let the kernel choose the correct protocol for TCP.
    */
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (listen_fd == -1)
    {
        die("socket");
    }

    /*
        SO_REUSEADDR:
            Allows quickly restarting the server on the same port.

        Without this, bind() may fail for a short time after restart because
        the old socket can remain in TIME_WAIT.
    */
    int reuse = 1;

    if (setsockopt(
            listen_fd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuse,
            sizeof(reuse)
        ) == -1)
    {
        die("setsockopt SO_REUSEADDR");
    }

    /*
        sockaddr_in describes an IPv4 address and port.
    */
    struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;

    /*
        INADDR_ANY means:
            listen on all interfaces inside this network namespace.

        Inside the container, that includes the container veth IP.
    */
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    /*
        Network protocols use big-endian byte order.

        htons() converts host byte order to network byte order.
    */
    server_addr.sin_port = htons((unsigned short)port);

    if (bind(
            listen_fd,
            (struct sockaddr*)&server_addr,
            sizeof(server_addr)
        ) == -1)
    {
        die("bind");
    }

    /*
        listen backlog 16:
            The kernel can queue a small number of pending connections.
    */
    if (listen(listen_fd, 16) == -1)
    {
        die("listen");
    }

    return listen_fd;
}

/*
    handle_client()
    ---------------

    Reads messages from one connected client until the client disconnects.
*/
static void handle_client(int client_fd)
{
    char buffer[256];

    while (1)
    {
        memset(buffer, 0, sizeof(buffer));

        /*
            read() returns:
                > 0  number of bytes read
                0    client disconnected
                -1   error
        */
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);

        if (bytes_read == -1)
        {
            die("read");
        }

        if (bytes_read == 0)
        {
            printf("Client disconnected\n");
            break;
        }

        printf("Message from client: %s\n", buffer);
    }
}

/*
    main()
    ------

    Usage:
        ./server <port>

    Example:
        ./server 8080
*/
int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int port = atoi(argv[1]);

    if (port <= 0 || port > 65535)
    {
        fprintf(stderr, "Invalid port: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    int listen_fd = create_listen_socket(port);

    printf("Server listening on port %d\n", port);

    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(
            listen_fd,
            (struct sockaddr*)&client_addr,
            &client_len
        );

        if (client_fd == -1)
        {
            die("accept");
        }

        printf(
            "Accepted connection from %s\n",
            inet_ntoa(client_addr.sin_addr)
        );

        handle_client(client_fd);

        close(client_fd);
    }

    close(listen_fd);
    return EXIT_SUCCESS;
}