BIN_DIR := ../../build/bin
LIB_DIR := ..

CC := $(BIN_DIR)/clang
CXX := $(BIN_DIR)/clang++
CFLAGS := -O3
LDFLAGS := -fuse-ld=lld -lstdc++ $(LIB_DIR)/libanalysis.o

SRC := hello.c

hello: $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

clean:
	rm -rf *.o hello *.s *.ll
