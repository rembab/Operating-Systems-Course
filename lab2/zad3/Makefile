CC = gcc

CFLAGS = -Wall -O3

OBJECTS = sig_default.o sig_mask.o sig_ignore.o sig_handle.o 

.PHONY: all clean static shared dynamic

all: static shared dynamic

%.o: %.c sig_lib.h
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

sig_lib.a: $(OBJECTS)
	ar rcs $@ $^

sig_lib.so: $(OBJECTS)
	$(CC) -shared -o $@ $^

main: main.c
	$(CC) $(CFLAGS) $< -o $@

static: main child.c sig_lib.a
	$(CC) $(CFLAGS) child.c sig_lib.a -o child

shared: main child.c sig_lib.so
	$(CC) $(CFLAGS) child.c -L. -l:sig_lib.a -Wl,-rpath=. -o child

dynamic: main child.c sig_lib.so
	$(CC) $(CFLAGS) -DDYNAMIC child.c -ldl -o child

.PHONY: clean
clean:
	-@ rm -f *.o sig_lib.a sig_lib.so

