/**
 * upkg.c - Unreal Engine package parser
 *
 * Copyright (C) 2026 uiop <uiop@wasdhjkl.xyz>
 */

#include <stdbool.h>
#include <string.h>

#include "upkg.h"

#define UPKG_SIGNATURE 0x9E2A83C1
#define UPKG_GUID_BYTES 16

/**
 * Size of the "GenerationInfo" object for UE3 (three i32).
 * https://github.com/gildor2/UEViewer/blob/master/Unreal/UnrealPackage/UnPackage.h#L21
 */
#define UPKG_GENINFO_BYTES (sizeof(int32_t) * 3)

struct cursor {
  const uint8_t *ptr;
  size_t left;
  size_t offset;
  upkg_err err;
};

static bool need(struct cursor *c, size_t n) {
  if (c->err != UPKG_OK)
    return false;
  if (c->left < n) {
    c->err = UPKG_TRUNC;
    return false;
  }
  return true;
}

static void advance(struct cursor *c, size_t n) {
  c->ptr += n;
  c->left -= n;
  c->offset += n;
}

static uint32_t read_u32(struct cursor *c) {
  if (!need(c, 4))
    return 0;
  uint32_t v = (uint32_t)c->ptr[0] | (uint32_t)c->ptr[1] << 8 |
               (uint32_t)c->ptr[2] << 16 | (uint32_t)c->ptr[3] << 24;
  advance(c, 4);
  return v;
}

static uint16_t read_u16(struct cursor *c) {
  if (!need(c, 2))
    return 0;
  uint16_t v = (uint16_t)c->ptr[0] | (uint16_t)c->ptr[1] << 8;
  advance(c, 2);
  return v;
}

static int32_t read_i32(struct cursor *c) {
  uint32_t u = read_u32(c);
  int32_t v;
  memcpy(&v, &u, sizeof(v));
  return v;
}

/**
 * Read array/string length. Package version MUST be >= 178.
 * Returns 0 on error indicating value is either negative or > bytes left.
 */
static size_t read_count(struct cursor *c) {
  int32_t n = read_i32(c);
  if (c->err != UPKG_OK)
    return 0;
  if (n < 0 || (size_t)n > c->left) {
    c->err = UPKG_BADIDX;
    return 0;
  }
  return (size_t)n;
}

static void skip(struct cursor *c, size_t n) {
  if (need(c, n))
    advance(c, n);
}

static struct upkg_str read_str(struct cursor *c) {
  struct upkg_str s = {.len = 0, .data = ""};
  int32_t n = read_i32(c);
  if (c->err != UPKG_OK || n == 0)
    return s;
  if (n < 0) {
    c->err = UPKG_BADSTR;
    return s;
  }
  if (!need(c, (size_t)n))
    return s;
  if (c->ptr[n - 1] != '\0') {
    c->err = UPKG_BADSTR;
    return s;
  }
  s.data = (const char *)c->ptr; // WARN: c->ptr must outlive string
  s.len = (size_t)n - 1;
  advance(c, (size_t)n);
  return s;
}

upkg_err upkg_parse(const uint8_t *in, size_t len, struct upkg *out,
                    size_t *pos) {
  if (!in || !out || !pos)
    return UPKG_BADARGS;

  struct cursor c = {.ptr = in, .left = len};
  struct upkg p = {0};

  if (read_u32(&c) != UPKG_SIGNATURE && c.err == UPKG_OK)
    c.err = UPKG_BADSIG;

  p.version = read_u16(&c);
  p.licensee_version = read_u16(&c);
  p.header_size = read_i32(&c);
  p.folder_name = read_str(&c);
  p.flags = read_u32(&c);

  /* NameCount, NameOffset, ExportCount, ExportOffset,
   * ImportCount, ImportOffset, DependsOffset, GUID, GenerationInfo[]
   */
  skip(&c, 7 * sizeof(uint32_t));
  skip(&c, UPKG_GUID_BYTES);
  skip(&c, UPKG_GENINFO_BYTES * read_count(&c));

  p.engine_version = read_i32(&c);
  p.cooker_version = read_i32(&c);
  p.compression_flag = read_u32(&c);

  *pos = c.offset;
  if (c.err != UPKG_OK)
    return c.err;
  *out = p;
  return UPKG_OK;
}

const char *upkg_strerror(upkg_err err) {
  switch (err) {
  case UPKG_OK:
    return "ok";
  case UPKG_BADARGS:
    return "invalid argument(s)";
  case UPKG_BADSIG:
    return "invalid signature";
  case UPKG_TRUNC:
    return "truncated";
  case UPKG_BADIDX:
    return "malformed index";
  case UPKG_BADSTR:
    return "invalid string";
  default:
    return "unknown";
  }
}
