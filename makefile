
CC=gcc
CFLAGS=-Wall -m32

SOURCES=server_f.c server_common.c args.c server_filesystem.c server_http.c
OBJECTS=$(SOURCES:.c=.o)

all: $(SOURCES)

server: $(OBJECTS)
	$(CC) $(CFLAGS) -o server_f $(OBJECTS)

.c.o:
	$(CC) $(CFLAGS) -c $<

test: server
	./server_f 8000 /cscome/langen/CMPUT_Assignment2/testsrv /cshome/langen/CMPUT_Assignment2/logfile

resettest:
	rm -rf testsrv
	mkdir -p testsrv
	rm -rf logfile
	echo "" > logfile

clean:
	rm *.o
