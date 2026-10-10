/**
 * patch.c - Detective mode flashbang patch
 *
 * Copyright (C) 2026 uiop <uiop@wasdhjkl.xyz>
 */

#include <stddef.h>
#include <string.h>

#include "debug.h"
#include "patch.h"

#define PATCH_AT 10

/**
 * RViewPostProcess: Let(InstanceVar[0xaae] = LocalVar[0xaba]); Return; Nothing;
 * EndOfScript we replace the RHS LocalVariable (5 bytes) with EX_FloatConst
 * 0.0f (5 bytes).
 */
static const uint8_t NEEDLE[] = {
    0x0e, 0x00, 0x00, 0x00, 0x0f,
    0x01, 0xae, 0x0a, 0x00, 0x00, // InstanceVariable 0xaae
    0x00, 0xba, 0x0a, 0x00, 0x00, // LocalVariable 0xaba  <- patched
    0x04, 0x0b, 0x53,
};

static const uint8_t PATCH[] = {0x1e, 0x00, 0x00, 0x00, 0x00};

static_assert(PATCH_AT + sizeof(PATCH) <= sizeof(NEEDLE),
              "patch must fit inside needle");
static_assert(sizeof(PATCH) == 5, "debug hexdump below assumes 5 bytes");

/**
 * Count matches, *first = offset of the first one
 */
static size_t find_all(const uint8_t *hay, size_t hay_len,
                       const uint8_t *needle, size_t n, size_t *first) {
  size_t count = 0;
  for (size_t i = 0; i + n <= hay_len; i++)
    if (hay[i] == needle[0] && memcmp(hay + i, needle, n) == 0 && count++ == 0)
      *first = i;
  return count;
}

enum patch_result patch_apply(uint8_t *buf, size_t len) {
  size_t at = 0;
  size_t n = find_all(buf, len, NEEDLE, sizeof(NEEDLE), &at);
  DEBUG("needle: %zu match(es)%s", n, n ? "" : ", checking if already patched");

  if (n == 0) {
    uint8_t patched[sizeof(NEEDLE)];
    memcpy(patched, NEEDLE, sizeof(NEEDLE));
    memcpy(patched + PATCH_AT, PATCH, sizeof(PATCH));

    size_t pn = find_all(buf, len, patched, sizeof(patched), &at);
    if (pn)
      DEBUG("patched needle: %zu match(es), first @0x%zx", pn, at);
    else
      DEBUG("patched needle: 0 matches");
    return pn ? PATCH_ALREADY : PATCH_NOTFOUND;
  }
  if (n > 1) {
    DEBUG("needle ambiguous: %zu matches, first @0x%zx", n, at);
    return PATCH_AMBIGUOUS;
  }

  uint8_t *site = buf + at + PATCH_AT;
  DEBUG("patching @0x%zx: %02x %02x %02x %02x %02x -> %02x %02x %02x %02x %02x",
        at + PATCH_AT, site[0], site[1], site[2], site[3], site[4], PATCH[0],
        PATCH[1], PATCH[2], PATCH[3], PATCH[4]);
  memcpy(site, PATCH, sizeof(PATCH));
  return PATCH_OK;
}
