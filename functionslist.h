#include "functions.h"

extern char* fn_square(void);
extern char* fn_hello_world(void);

static const Route routes[] = {
    {"/square", fn_square},
    {"/hello_world", fn_hello_world}
};
