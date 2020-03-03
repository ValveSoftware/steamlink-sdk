# Source setenv.sh before running make
#

CFLAGS := $(shell sdl2-config --cflags)
LDFLAGS := $(shell sdl2-config --libs) -lSDL2_test

testgles2: testgles2.o
	$(CC) -o $@ $< $(LDFLAGS)

clean:
	$(RM) testgles2.o

distclean: clean
	$(RM) testgles2
	$(RM) -r steamlink
