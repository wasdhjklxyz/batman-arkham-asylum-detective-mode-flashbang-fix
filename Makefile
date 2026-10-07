OUT := baadmff
SRC := $(OUT).c

.PHONY: all clean

all: $(SRC)
	$(CC) $^ -o $(OUT)

clean:
	$(RM) $(OUT)
