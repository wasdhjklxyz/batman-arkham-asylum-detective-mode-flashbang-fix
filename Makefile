OUT := baadmff
SRC := $(OUT).c

CFLAGS := -Wall -Wextra -Wpedantic -Werror

.PHONY: all clean

all: $(SRC)
	$(CC) $^ $(CFLAGS) -o $(OUT)

clean:
	$(RM) $(OUT)
