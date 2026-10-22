CFLAGS = -std=c11 -g -Wall -Wextra -fsanitize=address,undefined
PROGS = 01_pointers 02_vector 03_hashmap 04_arena

test: $(PROGS)
	@for p in $(PROGS); do ./$$p || exit 1; done

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(PROGS)

.PHONY: test clean
