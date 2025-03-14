# Source setenv.sh before running make
#

CFLAGS := -I$(MARVELL_ROOTFS)/usr/include/SDL3
LDFLAGS := -lSDL3

snake: snake.o
	$(CC) -o $@ $< $(LDFLAGS)

clean:
	$(RM) snake.o

distclean: clean
	$(RM) snake
	$(RM) -r steamlink
