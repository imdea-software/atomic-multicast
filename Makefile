INC_DIR = include
SRC_DIR = src
TEST_DIR = tests

CC      = gcc
CFLAGS  = -O2 -Wall -g -I$(INC_DIR) -I$(SRC_DIR)
LDFLAGS = -levent
LDFLAGS_TEST = $(LDFLAGS) -lpthread -levent_pthreads
VERBOSE =
TARGET  = $(TEST_DIR)/node_init_free $(TEST_DIR)/node_connect
OBJS    = $(SRC_DIR)/node.o

all: $(TARGET)

$(TEST_DIR)/node_init_free: $(TEST_DIR)/node_init_free.o $(OBJS)
	$(CC) $(VERBOSE) -o $@ $< $(OBJS) $(LDFLAGS)

$(TEST_DIR)/node_connect: $(TEST_DIR)/node_connect.o $(OBJS)
	$(CC) $(VERBOSE) -o $@ $< $(OBJS) $(LDFLAGS_TEST)

$(SRC_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< $(VERBOSE)

clean:
	rm -f $(TEST_DIR)/*.o $(SRC_DIR)/*.o $(TARGET)
