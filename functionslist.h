#include "functions.h"

extern char *fn_square(const ParsedRequest *request);
extern char *fn_hello_world(const ParsedRequest *request);

static const Route routes[] = {
    {"/square", fn_square},
    {"/hello_world", fn_hello_world}
};
