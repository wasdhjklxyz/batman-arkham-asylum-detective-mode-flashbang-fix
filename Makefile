#
# Simple Makefile for Batman: Arkham Asylum detective mode flashbang fix
#
# Copyright (C) 2026 uiop <uiop@wasdhjkl.xyz>
#

MINILZO := lib/minilzo-2.10

OUT   := baadmff
CORE  := src/baadmff.c src/upkg.c src/patch.c $(MINILZO)/minilzo.c
SRC   := src/main.c src/io.c $(CORE)

CFLAGS := -std=c23 -Wall -Wextra -Wpedantic -Werror
CFLAGS += -I$(MINILZO)

SAN    := -fsanitize=address,undefined
DBG    := -g -Og -U_FORTIFY_SOURCE $(SAN)
REL    := -O2 -DNDEBUG

debug: CFLAGS += $(DBG)
debug: LDFLAGS += $(SAN)
debug: $(SRC)
	$(CC) $(CFLAGS) -o $(OUT) $^ $(LDFLAGS)

release: CFLAGS += $(REL)
release: $(SRC)
	$(CC) $(CFLAGS) -o $(OUT) $^ $(LDFLAGS)

clean:
	$(RM) $(OUT)

.PHONY: debug release clean
