typedef char* (*FunctionHandler)(void);

typedef struct {
    const char *path;
    FunctionHandler handler;
} Route;
