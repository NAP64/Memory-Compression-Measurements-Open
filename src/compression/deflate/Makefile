IFLAGS=
TARGET=
INCLUDE=
LDLIBS=-pthread -ldl
CFLAGS=-ggdb3 -O2
SFLAGS=-shared -fPIC

default: deflate

deflate:
	$(CC) $(CFLAGS) $(SFLAGS) $(IFLAGS) -w -o $(TARGET)/deflate.so ./deflate.c $(INCLUDE)/zlib/*.c
