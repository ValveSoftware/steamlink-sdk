# Source setenv.sh before running make
#

CFLAGS := -I$(MARVELL_ROOTFS)/usr/include/SDL2 -DHAVE_OPENGLES2
LDFLAGS := -lSDL2 -lSDL2_test

testgles2: testgles2.o
	$(CC) -o $@ $< $(LDFLAGS)

clean:
	$(RM) testgles2.o

distclean: clean
	$(RM) testgles2
	$(RM) -r steamlink
