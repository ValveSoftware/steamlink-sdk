#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include <fcntl.h>

#include <pulsecore/core-util.h>

int main(int argc, char *argv[]) {

    open("/dev/null", O_RDONLY);
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_RDONLY);

    pa_close_all(5, -1);

    return 0;
}
