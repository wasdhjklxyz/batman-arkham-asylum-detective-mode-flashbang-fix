#
# Simple Makefile for Batman: Arkham Asylum detective mode flashbang fix
#
# Copyright (C) 2026 uiop <uiop@wasdhjkl.xyz>
#

MINILZO := lib/minilzo-2.10

OUT   := baadmff
CORE  := src/baadmff.c src/upkg.c src/patch.c src/lzo.c
SRC   := src/main.c src/io.c $(CORE)

CFLAGS := -std=c23 -Wall -Wextra -Wpedantic -Werror
CFLAGS += -isystem $(MINILZO)

SAN    := -fsanitize=address,undefined
DBG    := -g -Og -U_FORTIFY_SOURCE $(SAN)
REL    := -O2 -DNDEBUG

# Debug logs stay on, the page shows them
WEB    := build/web
EMCC   := emcc
EMFLAGS := -O2 -sMODULARIZE -sEXPORT_ES6 -sENVIRONMENT=web -sFILESYSTEM=0
EMFLAGS += -sALLOW_MEMORY_GROWTH
EMFLAGS += -sEXPORTED_FUNCTIONS=_baadmff_patch,_baadmff_strerror,_malloc,_free
EMFLAGS += -sEXPORTED_RUNTIME_METHODS=HEAPU8,HEAPU32,UTF8ToString

debug: CFLAGS += $(DBG)
debug: LDFLAGS += $(SAN)
debug: $(SRC)
	$(CC) $(CFLAGS) -o $(OUT) $^ $(LDFLAGS)

release: CFLAGS += $(REL)
release: $(SRC)
	$(CC) $(CFLAGS) -o $(OUT) $^ $(LDFLAGS)

web: $(CORE) web/index.html web/app.js
	mkdir -p $(WEB)
	$(EMCC) $(CFLAGS) $(EMFLAGS) -o $(WEB)/baadmff.js $(CORE)
	cp web/index.html web/app.js $(WEB)/

clean:
	$(RM) $(OUT)
	$(RM) -r build

.PHONY: debug release web clean
