CFLAGS = -std=gnu11 -g -O0 -Wall -Wextra -Wno-switch -fno-common
SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:.c=.o)

jcc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJS): src/jcc.h

# Compile every program in tests/programs with both gcc and jcc and compare their output,
# then check that every program in tests/errors is rejected with the expected message.
test: jcc
	./tests/run.sh

# Same, but with AddressSanitizer + UBSan watching the compiler itself for memory bugs.
asan:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) -fsanitize=address,undefined" jcc
	ASAN_OPTIONS=detect_leaks=0 ./tests/run.sh

clean:
	rm -f jcc src/*.o
	rm -rf tests/out

.PHONY: test asan clean
