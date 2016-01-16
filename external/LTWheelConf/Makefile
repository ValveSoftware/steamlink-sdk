OBJS=main.o wheelfunctions.o wheels.o
LIBS=-lusb-1.0

all: ltwheelconf

ltwheelconf: $(OBJS)
	$(CC) -o ltwheelconf $(OBJS) $(LIBS)

main.o: main.c
wheels.o: wheels.c wheels.h
wheelfunctions.o: wheelfunctions.c wheelfunctions.h wheels.h

install:
	install -m 755 ltwheelconf $(DESTDIR)/usr/bin

clean:
	rm -rf ltwheelconf $(OBJS)
