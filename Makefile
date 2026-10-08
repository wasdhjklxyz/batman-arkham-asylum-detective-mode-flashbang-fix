#
# Simple Makefile for Batman: Arkham Asylum detective mode flashbang fix
#
# Copyright (C) 2026 uiop <uiop@wasdhjkl.xyz>
#

MINILZO := lib/minilzo-2.10

OUT   := baadmff
SRC   := src/main.c src/upkg.c $(MINILZO)/minilzo.c

CFLAGS := -Wall -Wextra -Wpedantic -Werror
CFLAGS += -I$(MINILZO)

.PHONY: all clean

all: $(SRC)
	$(CC) $^ $(CFLAGS) -o $(OUT)

clean:
	$(RM) $(OUT)
