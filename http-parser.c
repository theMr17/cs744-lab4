#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "http-parser.h"

void parse_http_request(char *raw_request, ParsedRequest *req) {
    req->header_count = 0;
    req->body = NULL;
    req->request_line.query_param_count = 0;

    // 1. Separate the headers section from the body
    // The body always starts after a double CRLF (\r\n\r\n)
    char *body_start = strstr(raw_request, "\r\n\r\n");
    if (body_start) {
        *body_start = '\0'; // Temporarily split headers and body
        req->body = body_start + 4; // Move pointer past "\r\n\r\n"
    }

    // 2. Tokenize line by line using thread-safe strtok_r
    char *line_saveptr;
    char *line = strtok_r(raw_request, "\r\n", &line_saveptr);
    
    if (line == NULL) return;

    // 3. Parse the Request Line (First line)
    // Format: "METHOD URI VERSION" (e.g., "GET /index.html HTTP/1.1")
    sscanf(line, "%15s %255s %15s", 
           req->request_line.method, 
           req->request_line.uri, 
           req->request_line.version);

    char *query_start = strchr(req->request_line.uri, '?');
    if (query_start != NULL) {
        *query_start = '\0';

        char *param_saveptr;
        char *param = strtok_r(query_start + 1, "&", &param_saveptr);
        while (param != NULL &&
               req->request_line.query_param_count < MAX_QUERY_PARAMS) {
            QueryParam *query_param =
                &req->request_line.query_params[req->request_line.query_param_count];
            char *equals = strchr(param, '=');

            if (equals != NULL) {
                *equals = '\0';
                strncpy(query_param->value, equals + 1,
                        sizeof(query_param->value) - 1);
            } else {
                query_param->value[0] = '\0';
            }
            query_param->value[sizeof(query_param->value) - 1] = '\0';

            strncpy(query_param->key, param, sizeof(query_param->key) - 1);
            query_param->key[sizeof(query_param->key) - 1] = '\0';

            req->request_line.query_param_count++;
            param = strtok_r(NULL, "&", &param_saveptr);
        }
    }

    // 4. Parse subsequent Header lines
    line = strtok_r(NULL, "\r\n", &line_saveptr);
    while (line != NULL && req->header_count < 32) {
        char *colon = strchr(line, ':');
        if (colon) {
            *colon = '\0'; // Split into key and value
            
            // Extract and clean key
            strncpy(req->headers[req->header_count].key, line, sizeof(req->headers[0].key) - 1);
            
            // Extract value and skip leading whitespace
            char *val_ptr = colon + 1;
            while (*val_ptr == ' ') val_ptr++;
            
            strncpy(req->headers[req->header_count].value, val_ptr, sizeof(req->headers[0].value) - 1);
            req->header_count++;
        }
        line = strtok_r(NULL, "\r\n", &line_saveptr);
    }
}

/**
 * Creates a dynamically allocated HTTP response string.
 * 
 * @param status_code   e.g., "200 OK" or "404 Not Found"
 * @param content_type  e.g., "text/html", "application/json", "image/jpeg"
 * @param data          Pointer to the body payload
 * @param data_len      Size of the payload in bytes
 * @param out_resp_len  Pointer to return the total size of the final response block
 * @return              Pointer to the allocated response block (caller must free it)
 */
char* create_http_response(const char *status_code, const char *content_type, 
                           const char *data, size_t data_len, size_t *out_resp_len) {
    char headers[512];
    
    // 1. Format the HTTP status line and essential response headers
    int headers_len = snprintf(headers, sizeof(headers),
        "HTTP/1.1 %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n", // This blank line separates headers from the body
        status_code, content_type, data_len);

    if (headers_len < 0 || (size_t)headers_len >= sizeof(headers)) {
        return NULL; // Header buffer overflow or error
    }

    // 2. Allocate exact memory for headers + body payload + null terminator
    size_t total_len = headers_len + data_len;
    char *response = (char *)malloc(total_len + 1);
    if (!response) {
        return NULL; // Memory allocation failure
    }

    // 3. Copy headers and data sequentially into the response block
    memcpy(response, headers, headers_len);
    memcpy(response + headers_len, data, data_len);
    
    // Null terminate the end (useful if treating the body as a text string)
    response[total_len] = '\0'; 

    // Return total size via pointer since body might contain embedded binary nulls
    if (out_resp_len) {
        *out_resp_len = total_len;
    }

    return response;
}


