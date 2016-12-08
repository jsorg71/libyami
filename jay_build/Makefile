
OBJS = gputest.o

CFLAGS = -I/opt/yami/include -I/opt/yami/include/libyami -I/opt/yami/include/va

LDFLAGS = -L/opt/yami/lib -Wl,-rpath,/opt/yami/lib

LIBS = -lyami -pthread

all: gputest

gputest: $(OBJS)
	$(CC) -o gputest $(OBJS) $(LDFLAGS) $(LIBS)

clean:
	rm -f gputest $(OBJS)

