#ifndef GP2X_WRAPPER
#define GP2X_WRAPPER

#define malloc(size) gp2x_malloc(size)
#define calloc(n,size) gp2x_calloc(n,size)
#define realloc(ptr,size) gp2x_realloc(ptr,size)
#define free(size) gp2x_free(size)

#define printf gp2x_printf

extern void gp2x_printf(char* fmt, ...);

#endif
