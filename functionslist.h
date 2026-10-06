#include "functions.h"

extern char *fn_square(const ParsedRequest *request);
extern char *fn_root(const ParsedRequest *request);
extern char *fn_cube(const ParsedRequest *request);
extern char *fn_helloworld(const ParsedRequest *request);
extern char *fn_pingpong(const ParsedRequest *request);
extern char *fn_prime(const ParsedRequest *request);
extern char *fn_fibonacci(const ParsedRequest *request);

static const Route routes[] = {
    {"/", fn_root},
    {"/square", fn_square},
    {"/cube", fn_cube},
    {"/helloworld", fn_helloworld},
    {"/pingpong", fn_pingpong},
    {"/arithmetic/prime", fn_prime},
    {"/arithmetic/fibonacci", fn_fibonacci}
};
