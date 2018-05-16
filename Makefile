INC_DIR = include
SRC_DIR = src
TEST_DIR = tests
BENCH_DIR = bench

CC      = gcc
CFLAGS  = -O2 -Wall -g -I$(INC_DIR) -I$(SRC_DIR) -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include
LDFLAGS = -levent -lglib-2.0
LDFLAGS_TEST = $(LDFLAGS) -lpthread -levent_pthreads
VERBOSE =
TARGET  = $(TEST_DIR)/node_init_free $(TEST_DIR)/node_connect $(TEST_DIR)/node_messages $(TEST_DIR)/node_amcast_data $(TEST_DIR)/pqueue_unit $(BENCH_DIR)/node-bench
OBJS    = $(SRC_DIR)/node.o $(SRC_DIR)/events.o $(SRC_DIR)/message.o $(SRC_DIR)/amcast.o $(SRC_DIR)/pqueue.o

all: $(TARGET)

$(TEST_DIR)/node_init_free: $(TEST_DIR)/node_init_free.o $(OBJS)
	$(CC) $(VERBOSE) -o $@ $< $(OBJS) $(LDFLAGS)

$(TEST_DIR)/node_connect: $(TEST_DIR)/node_connect.o $(OBJS)
	$(CC) $(VERBOSE) -o $@ $< $(OBJS) $(LDFLAGS_TEST)

$(TEST_DIR)/node_messages: $(TEST_DIR)/node_messages.o $(OBJS)
	$(CC) $(VERBOSE) -o $@ $< $(OBJS) $(LDFLAGS)

$(TEST_DIR)/node_amcast_data: $(TEST_DIR)/node_amcast_data.o $(OBJS)
	$(CC) $(VERBOSE) -o $@ $< $(OBJS) $(LDFLAGS)

$(TEST_DIR)/pqueue_unit: $(TEST_DIR)/pqueue_unit.o $(OBJS)
	$(CC) $(VERBOSE) -o $@ $< $(OBJS) $(LDFLAGS)

$(BENCH_DIR)/node-bench: $(BENCH_DIR)/node-bench.o $(OBJS)
	$(CC) $(VERBOSE) -o $@ $< $(OBJS) $(LDFLAGS) -lpthread

$(SRC_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< $(VERBOSE)

clean:
	rm -f $(TEST_DIR)/*.o $(SRC_DIR)/*.o $(BENCH_DIR)/*.o $(OBJS) $(TARGET)
