CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99 -O2 -I.
LDLIBS  = -lncursesw

OBJS    = cwgrphc.o cscrn.o

all: libtwrenderer.a test_renderer

libtwrenderer.a: $(OBJS)
	ar rcs $@ $^

test_renderer: test_renderer.o libtwrenderer.a
	$(CC) $(CFLAGS) -o $@ test_renderer.o libtwrenderer.a $(LDLIBS)

%.o: %.c cwgrphc.h
	$(CC) $(CFLAGS) -c -o $@ $<

test_renderer.o: test_renderer.c cwgrphc.h cscrn.h

test: test_renderer
	TERM=xterm-256color ./test_renderer headless

clean:
	rm -f $(OBJS) test_renderer.o libtwrenderer.a test_renderer

.PHONY: all test clean
