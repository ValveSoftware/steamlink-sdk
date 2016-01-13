SRCS=libudev.c \
    libudev-device.c \
    libudev-device-private.c \
    libudev-enumerate.c \
    libudev-hwdb.c \
    libudev-list.c \
    libudev-monitor.c \
    libudev-queue.c \
    libudev-util.c \
    strxcpyx.c \
    log.c \
    device-nodes.c \
    utf8.c \
    util.c \
    MurmurHash2.c \
    time-util.c \
    fileio.c \
    path-util.c \
    strv.c \
    gunicode.c \
    hashmap.c \
    set.c \
    env-util.c \
    mkdir.c \
    virt.c \
    siphash24.c


OBJS=$(SRCS:.c=.o)

CPPFLAGS=-I.
CFLAGS:=-include ./config.h -fPIC $(CFLAGS) -g

all: libudev.a libudev.so.1

libudev.a: $(OBJS)
	$(AR) rcs $@ $^

libudev.so.1: $(OBJS)
	$(CC) $(CFLAGS) -shared -Wl,--no-undefined -o $@ $^ -lrt

install:
	install -m 644 libudev.h $(DESTDIR)/usr/include/
	install -m 644 libudev.a $(DESTDIR)/usr/lib/
	install -m 755 libudev.so.1 $(DESTDIR)/usr/lib/
	ln -sf libudev.so.1 $(DESTDIR)/usr/lib/libudev.so

uninstall:
	rm -f $(DESTDIR)/usr/include/libudev.h
	rm -f $(DESTDIR)/usr/lib/libudev.a
	rm -f $(DESTDIR)/usr/lib/libudev.so*

clean:
	rm -f $(OBJS) libudev.a libudev.so.1

.PHONY: all clean
