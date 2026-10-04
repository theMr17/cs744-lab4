#include <stdio.h>
#include "functionslist.h"
#include "http-parser.h"

char* fn_square(void) {
    char* data = "square\n";
    size_t response_size = 0;
    return create_http_response("200 OK", "text/html", data, strlen(data), &response_size);
}

char* fn_hello_world(void) {
    char* data = "hello\n";
    size_t response_size = 0;
    return create_http_response("200 OK", "text/html", data, strlen(data), &response_size);
}
