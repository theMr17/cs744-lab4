#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include "functionslist.h"
#include "http-parser.h"

static const char *query_value(const ParsedRequest *request, const char *key) {
    for (int i = 0; i < request->request_line.query_param_count; i++) {
        if (strcmp(request->request_line.query_params[i].key, key) == 0) {
            return request->request_line.query_params[i].value;
        }
    }
    return NULL;
}

static long long query_number(const ParsedRequest *request, const char *key,
                              long long default_value) {
    const char *value = query_value(request, key);
    if (value == NULL || *value == '\0') {
        return default_value;
    }

    char *end;
    errno = 0;
    long long number = strtoll(value, &end, 10);
    if (errno == ERANGE || *end != '\0') {
        return default_value;
    }
    return number;
}

static char *text_response(const char *data) {
    char response_data[512];
    int data_len = snprintf(response_data, sizeof(response_data), "%s\n", data);
    size_t response_size = 0;
    return create_http_response("200 OK", "text/plain", response_data, data_len, &response_size);
}

static char *number_response(long long value) {
    char data[64];
    snprintf(data, sizeof(data), "%lld", value);
    return text_response(data);
}

char *fn_root(const ParsedRequest *request) {
    (void)request;
    return text_response("Hello World");
}

char *fn_square(const ParsedRequest *request) {
    long long number = query_number(request, "num", 1);
    return number_response(number * number);
}

char *fn_cube(const ParsedRequest *request) {
    long long number = query_number(request, "num", 1);
    return number_response(number * number * number);
}

char *fn_helloworld(const ParsedRequest *request) {
    const char *value = query_value(request, "str");
    if (value == NULL || *value == '\0') {
        return text_response("Hello");
    }

    char data[sizeof("Hello, ") + 256];
    snprintf(data, sizeof(data), "Hello, %s", value);
    return text_response(data);
}

char *fn_pingpong(const ParsedRequest *request) {
    const char *value = query_value(request, "str");
    return text_response(value == NULL || *value == '\0' ? "PingPong" : value);
}

char *fn_prime(const ParsedRequest *request) {
    long long number = query_number(request, "num", 0);
    int prime = number >= 2;
    for (long long divisor = 2; prime && divisor <= number / divisor; divisor++) {
        if (number % divisor == 0) {
            prime = 0;
        }
    }
    return text_response(prime ? "True" : "False");
}

char *fn_fibonacci(const ParsedRequest *request) {
    long long index = query_number(request, "k", 0);
    long long previous = 0;
    long long current = 1;

    if (index < 0) {
        return number_response(0);
    }
    for (long long i = 0; i < index; i++) {
        long long next = previous + current;
        previous = current;
        current = next;
    }
    return number_response(previous);
}
