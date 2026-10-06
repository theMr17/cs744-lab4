#include <stdio.h>
#include "functionslist.h"
#include "http-parser.h"

char *fn_square(const ParsedRequest *request) {
    if (request->request_line.query_param_count == 0) {
        char *error_msg = "Missing query parameter for square function\n";
        size_t response_size = 0;
        return create_http_response("400 Bad Request", "text/html", error_msg, strlen(error_msg), &response_size);
    } else if (request->request_line.query_param_count > 1) {
        char *error_msg = "Too many query parameters for square function\n";
        size_t response_size = 0;
        return create_http_response("400 Bad Request", "text/html", error_msg, strlen(error_msg), &response_size);
    }

    if (request->request_line.query_params[0].key[0] != '\0' &&
        strcmp(request->request_line.query_params[0].key, "num") != 0) {
        char *error_msg = "Invalid query parameter key for square function\n";
        size_t response_size = 0;
        return create_http_response("400 Bad Request", "text/html", error_msg, strlen(error_msg), &response_size);
    }

    int num = atoi(request->request_line.query_params[0].value);
    char data[1024];
    int data_len = snprintf(data, sizeof(data), "%d\n", num * num);
    size_t response_size = 0;
    return create_http_response("200 OK", "text/html", data, data_len, &response_size);
}

char *fn_cube(const ParsedRequest *request) {
    if (request->request_line.query_param_count == 0) {
        char *error_msg = "Missing query parameter for cube function\n";
        size_t response_size = 0;
        return create_http_response("400 Bad Request", "text/html", error_msg, strlen(error_msg), &response_size);
    } else if (request->request_line.query_param_count > 1) {
        char *error_msg = "Too many query parameters for cube function\n";
        size_t response_size = 0;
        return create_http_response("400 Bad Request", "text/html", error_msg, strlen(error_msg), &response_size);
    }

    if (request->request_line.query_params[0].key[0] != '\0' &&
        strcmp(request->request_line.query_params[0].key, "num") != 0) {
        char *error_msg = "Invalid query parameter key for cube function\n";
        size_t response_size = 0;
        return create_http_response("400 Bad Request", "text/html", error_msg, strlen(error_msg), &response_size);
    }
    
    int num = atoi(request->request_line.query_params[0].value);
    char data[1024];
    int data_len = snprintf(data, sizeof(data), "%d\n", num * num * num);
    size_t response_size = 0;
    return create_http_response("200 OK", "text/html", data, data_len, &response_size);
}

char *fn_hello_world(const ParsedRequest *request) {
    char data[1024];
    int data_len = snprintf(data, sizeof(data), "hello\nquery_params=%d\n",
                            request->request_line.query_param_count);
    size_t response_size = 0;
    return create_http_response("200 OK", "text/html", data, data_len, &response_size);
}
