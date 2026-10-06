#include "http-parser.h"

typedef char *(*FunctionHandler)(const ParsedRequest *request);

typedef struct {
    const char *path;
    FunctionHandler handler;
} Route;
