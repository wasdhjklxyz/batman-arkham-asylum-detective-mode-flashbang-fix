#
# Simple Makefile for Batman: Arkham Asylum detective mode flashbang fix
#
# Copyright (C) 2026 uiop <uiop@wasdhjkl.xyz>
#

OUT   := baadmff
SRC   := src/main.c src/upkg.c

CFLAGS := -Wall -Wextra -Wpedantic -Werror

.PHONY: all clean

all: $(SRC)
	$(CC) $^ $(CFLAGS) -o $(OUT)

clean:
	$(RM) $(OUT)
