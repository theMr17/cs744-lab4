#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "http-parser.h"
#include "functionslist.h"
#include <pthread.h>

#define MAX_PENDING_REQ 100
#define BUF_SIZE 1024

char mode;
int num_threads = 0;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_queue_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_queue_full = PTHREAD_COND_INITIALIZER;

int req_queue[MAX_PENDING_REQ];

int curr_pending_req = 0;
int producer_idx = 0;
int consumer_idx = 0;

int init_server(int port)
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

    return server_socket_fd;
}

int accept_request(int server_fd, int *client_fd)
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    *client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (*client_fd < 0)
    {
        perror("accept");
        return -1;
    }

    if (mode == 't')
    {
        pthread_mutex_lock(&lock);

        while (curr_pending_req == MAX_PENDING_REQ)
        {
            pthread_cond_wait(&cond_queue_empty, &lock);
        }

        req_queue[producer_idx] = *client_fd;
        producer_idx = (producer_idx + 1) % MAX_PENDING_REQ;
        curr_pending_req++;

        pthread_cond_signal(&cond_queue_full);

        pthread_mutex_unlock(&lock);
    }

    return 0;
}

int read_request(int client_fd, char *req)
{
    memset(req, 0, BUF_SIZE);
    ssize_t data_size = recv(client_fd, req, BUF_SIZE, 0);
    if (data_size < 0)
    {
        perror("recv");
        return -1;
    }
    return 0;
}

void parse_request(char *raw_req, ParsedRequest *parsed_req)
{
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

void send_response(int client_fd, const char *res)
{
    size_t response_len = strlen(res);
    ssize_t send_status = send(client_fd, res, response_len, 0);
    if (send_status < 0)
    {
        perror("send");
        return;
    }
}

void request_handler(int *client_fd)
{
    char *raw_req = (char *)malloc(BUF_SIZE * sizeof(char));
    if (raw_req == NULL)
    {
        perror("malloc");
        close(*client_fd);
        free(client_fd);
        return;
    }

    if (read_request(*client_fd, raw_req) < 0)
    {
        close(*client_fd);
        free(client_fd);
        free(raw_req);
        return;
    }

    ParsedRequest *parsed_req = (ParsedRequest *)malloc(sizeof(ParsedRequest));
    if (parsed_req == NULL)
    {
        perror("malloc");
        close(*client_fd);
        free(client_fd);
        free(raw_req);
        return;
    }

    parse_request(raw_req, parsed_req);

    const char *status;
    char *http_response = dispatch_request(parsed_req, &status);

    printf("[thread %ld] %s %s -> %s\n",
           (long)pthread_self(),
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

void *thread_worker(void *ptr)
{
    if (mode == 'm')
    {
        int *client_fd = (int *)ptr;
        request_handler(client_fd);
        return NULL;
    }
    else if (mode == 't')
    {
        while (true)
        {
            pthread_mutex_lock(&lock);

            while (curr_pending_req == 0)
            {
                pthread_cond_wait(&cond_queue_full, &lock);
            }

            int client_fd = req_queue[consumer_idx];

            consumer_idx = (consumer_idx + 1) % MAX_PENDING_REQ;
            curr_pending_req--;

            pthread_cond_signal(&cond_queue_empty);

            pthread_mutex_unlock(&lock);

            int *client_fd_ptr = malloc(sizeof(int));
            if (client_fd_ptr == NULL)
            {
                perror("malloc");
                close(client_fd);
                continue;
            }

            *client_fd_ptr = client_fd;
            request_handler(client_fd_ptr);
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc < 3 || (argv[1][0] == 't' && argc < 4))
    {
        printf("Usage: ./cflask <mode> <port> <num_threads>\n");
        return 1;
    }
    mode = *argv[1];
    int port = atoi(argv[2]);

    int server_fd = init_server(port);
    if (server_fd < 0)
    {
        return 1;
    }

    char *mode_str;
    if (mode == 's')
    {
        mode_str = "single-threaded";
    }
    else if (mode == 'm')
    {
        mode_str = "thread-per-request";
    }
    else if (mode == 't')
    {
        num_threads = atoi(argv[3]);
        if (num_threads <= 0)
        {
            fprintf(stderr, "num_threads must be greater than zero\n");
            close(server_fd);
            return 1;
        }
        asprintf(&mode_str, "thread pool, %d threads", num_threads);
    }

    // create the threads only once, so this can't be in the while loop below
    if (mode == 't')
    {
        pthread_t threads[num_threads];
        for (int i = 0; i < num_threads; i++)
        {
            if (pthread_create(&threads[i], NULL, thread_worker, NULL) != 0)
            {
                perror("pthread_create");
                close(server_fd);
                return 1;
            }
            pthread_detach(threads[i]);
        }
    }

    printf("cflask [%s] on http://localhost:%d\n", mode_str, port);

    while (true)
    {
        int *client_fd = malloc(sizeof(int));
        if (client_fd == NULL)
        {
            perror("malloc");
            continue;
        }

        if (accept_request(server_fd, client_fd) < 0)
        {
            free(client_fd);
            continue;
        }

        if (mode == 's')
        {
            request_handler(client_fd);
            continue;
        }
        else if (mode == 'm')
        {
            pthread_t thread;
            if (pthread_create(&thread, NULL, thread_worker, client_fd) != 0)
            {
                perror("pthread_create");
                close(*client_fd);
                free(client_fd);
                continue;
            }
            pthread_detach(thread);
        }
        else if (mode == 't')
        {
            free(client_fd);
            continue;
        }
    }

    return 0;
}
