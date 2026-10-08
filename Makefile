OUT := baadmff
SRC := main.c upkg.c

CFLAGS := -Wall -Wextra -Wpedantic -Werror

.PHONY: all clean

all: $(SRC)
	$(CC) $^ $(CFLAGS) -o $(OUT)

clean:
	$(RM) $(OUT)
