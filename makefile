
############################## BUILD DIRECTIVES ##############################

CC=gcc
CFLAGS=-Wall -m32

SOURCES=server_common.c args.c server_filesystem.c server_http.c http_request.c
OBJECTS=$(SOURCES:.c=.o)

all: server_f server_p

server_f: $(OBJECTS) server_f.o
	$(CC) $(CFLAGS) -o server_f $(OBJECTS) server_f.o

server_p: $(OBJECTS) server_p.o
	$(CC) $(CFLAGS) -pthread -o server_p $(OBJECTS) server_p.o

.c.o:
	$(CC) $(CFLAGS) -c $<

clean:
	rm *.o

################################# TEST UTILS #################################

### Configurable Test Params ###
TEST_ROOT=/cshome/langen/CMPUT_Assignment2
TEST_DIR_NAME=testsrv
TEST_LOG_NAME=logfile
TEST_PORT=8000
################################

TEST_DIR=$(TEST_ROOT)/$(TEST_DIR_NAME)
TEST_LOG=$(TEST_ROOT)/$(TEST_LOG_NAME)
TEST_ARGS=$(TEST_PORT) $(TEST_DIR) $(TEST_LOG)

test_f: server_f
	./server_f $(TEST_ARGS)

test_p: server_p
	./server_p $(TEST_ARGS)

findserver:
	ps -A | grep 'server_' | grep -o '^\s*[0-9]*'

resettest:
	# Make the testsrv root directory if it doesn't exist
	mkdir -p $(TEST_DIR)

	# Clear the Log file
	rm -rf $(TEST_LOG)
	echo "" > $(TEST_LOG)

checkcode:
	./checkcode.sh .

loadtest:
	./multiget.sh