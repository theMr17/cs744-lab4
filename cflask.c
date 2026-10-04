#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "http-parser.h"
#include "functionslist.h"

#define MAX_PENDING_REQ 1
#define BUF_SIZE 1024

/*
 * Implement all the cflask logic here.
 * 1. web server initialization
 * 2. accept http requests
 * 3. parse http requests
 * 4. dispatch http requests
 * 5. process, generate and send http responses
 */

int init_server(char mode, int port)
{
    int server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket_fd < 0)
    {
        perror("socket");
        return -1;
    }

    struct sockaddr_in *addr = malloc(sizeof(struct sockaddr_in));
    memset(addr, 0, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);

    int bind_status = bind(server_socket_fd, (struct sockaddr *)addr, sizeof(*addr));
    if (bind_status < 0)
    {
        perror("bind");
        return -1;
    }

    int listen_status = listen(server_socket_fd, MAX_PENDING_REQ);
    if (listen_status < 0)
    {
        perror("listen");
        return -1;
    }

    printf("cflask [single-threaded] on http://0.0.0.0:%d\n", port);
    return server_socket_fd;
}

int accept_request(int server_fd, char* req, int* client_fd)
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    *client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (*client_fd < 0)
    {
        perror("accept");
        return 0;
    }

    memset(req, 0, BUF_SIZE);
    ssize_t data_size = recv(*client_fd, req, BUF_SIZE, 0);
    if (data_size < 0)
    {
        perror("recv");
        close(*client_fd);
        return 0;
    }

    return 1;
}

void parse_request(char* raw_req, ParsedRequest* parsed_req) {
    parse_http_request(raw_req, parsed_req);
}

char *dispatch_request(ParsedRequest *parsed_req, const char **status)
{
    char *path = parsed_req->request_line.uri;

    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++)
    {
        if (strcmp(path, routes[i].path) == 0)
        {
            *status = "200 OK";
            return routes[i].handler();
        }
    }

    *status = "404 Not Found";
    return create_http_response(*status, "text/plain", "Not Found\n", strlen("Not Found\n"), NULL);
}

void send_response(int client_fd, const char *res) {
    size_t response_len = strlen(res);
    ssize_t send_status = send(client_fd, res, response_len, 0);
    if (send_status < 0)
    {
        perror("send");
        return;
    }
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("Usage: ./cflask <mode> <port>\n");
        return 1;
    }

    int server_fd = init_server(*argv[1], atoi(argv[2]));
    if (server_fd < 0)
    {
        return 1;
    }

    while (true)
    {
        char* raw_req = (char*) malloc(BUF_SIZE * sizeof(char));
        int* client_fd = malloc(sizeof(int));
        if (!accept_request(server_fd, raw_req, client_fd))
        {
            free(client_fd);
            free(raw_req);
            break;
        }

        ParsedRequest* parsed_req = (ParsedRequest*) malloc(sizeof(ParsedRequest));
        parse_request(raw_req, parsed_req);

        const char *status;
        char* http_response = dispatch_request(parsed_req, &status);

        printf("[thread %ld] %s %s -> %s\n",
               (long)getpid(),
               parsed_req->request_line.method,
               parsed_req->request_line.uri,
               status);

        send_response(*client_fd, http_response);

        free(http_response);
        close(*client_fd);
        free(client_fd);
        free(parsed_req);
        free(raw_req);
    }

    return 0;
}
