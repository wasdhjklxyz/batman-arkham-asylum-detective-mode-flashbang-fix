/**
 * upkg.c - Unreal Engine package parser
 *
 * Copyright (C) 2026 uiop <uiop@wasdhjkl.xyz>
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "minilzo.h"
#include "upkg.h"

#define UPKG_SIGNATURE 0x9E2A83C1
#define UPKG_GUID_BYTES 16

/**
 * Size of the "GenerationInfo" object for UE3 (three i32).
 * https://github.com/gildor2/UEViewer/blob/master/Unreal/UnrealPackage/UnPackage.h#L21
 */
#define UPKG_GENINFO_BYTES (sizeof(int32_t) * 3)

#define UPKG_COMPRESSION_NONE 0
#define UPKG_COMPRESSION_LZO 2

#define UPKG_PKG_STORE_COMPRESSED 0x02000000

#define UPKG_CHUNK_BYTES 16     // 4x i32
#define UPKG_CHUNK_HDR_BYTES 16 // 4x i32
#define UPKG_BLOCK_ENT_BYTES 8  // { c_size, u_size }

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
    DEBUG("truncated at off %zu: need %zu, have %zu", c->offset, n, c->left);
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

static void write_u32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
  p[2] = (uint8_t)(v >> 16);
  p[3] = (uint8_t)(v >> 24);
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
    DEBUG("bad count %" PRId32 " at off %zu (left %zu)", n, c->offset - 4,
          c->left);
    c->err = UPKG_BADIDX;
    return 0;
  }
  return (size_t)n;
}

static size_t read_size(struct cursor *c) {
  int32_t n = read_i32(c);
  if (c->err == UPKG_OK && n < 0) {
    DEBUG("negative size %" PRId32 " at off %zu", n, c->offset - 4);
    c->err = UPKG_BADSIZE;
  }
  return c->err == UPKG_OK ? (size_t)n : 0;
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
    DEBUG("negative string len %" PRId32 " at off %zu (UTF-16?)", n,
          c->offset - 4);
    c->err = UPKG_BADSTR;
    return s;
  }
  if (!need(c, (size_t)n))
    return s;
  if (c->ptr[n - 1] != '\0') {
    DEBUG("unterminated string len %" PRId32 " at off %zu", n, c->offset);
    c->err = UPKG_BADSTR;
    return s;
  }
  s.data = (const char *)c->ptr; // WARN: c->ptr must outlive string
  s.len = (size_t)n - 1;
  advance(c, (size_t)n);
  return s;
}

static struct upkg_chunk read_chunk(struct cursor *c, size_t len) {
  struct upkg_chunk ch = {0};
  size_t at = c->offset;
  ch.u_offset = read_size(c);
  ch.u_size = read_size(c);
  ch.c_offset = read_size(c);
  ch.c_size = read_size(c);
  if (c->err != UPKG_OK)
    return ch;
  DEBUG("chunk @%zu: u_off=%zu u_size=%zu c_off=%zu c_size=%zu", at,
        (size_t)ch.u_offset, (size_t)ch.u_size, (size_t)ch.c_offset,
        (size_t)ch.c_size);
  if (ch.u_offset == 0 || ch.u_size == 0 || ch.c_offset == 0 ||
      ch.c_size < UPKG_CHUNK_HDR_BYTES + UPKG_BLOCK_ENT_BYTES + 1 ||
      ch.c_offset > len || ch.c_size > len - ch.c_offset) { // no overflow
    DEBUG("chunk @%zu rejected (file len %zu)", at, len);
    c->err = UPKG_BADCHUNK;
  }
  return ch;
}

static upkg_err decompress_chunk(const uint8_t *in, const struct upkg_chunk *ch,
                                 uint8_t *out, size_t out_len) {
  if (ch->u_offset > out_len || ch->u_size > out_len - ch->u_offset) {
    DEBUG("chunk u_off=%zu u_size=%zu exceeds out_len %zu",
          (size_t)ch->u_offset, (size_t)ch->u_size, out_len);
    return UPKG_BADCHUNK;
  }

  struct cursor c = {
      .ptr = in + ch->c_offset, .left = ch->c_size, .offset = ch->c_offset};
  uint32_t sig = read_u32(&c);
  if (sig != UPKG_SIGNATURE && c.err == UPKG_OK) {
    DEBUG("chunk @%zu: bad sig 0x%08" PRIx32, (size_t)ch->c_offset, sig);
    c.err = UPKG_BADCHUNK;
  }

  size_t block_size = read_size(&c);
  size_t c_total = read_size(&c);
  size_t u_total = read_size(&c);
  if (c.err != UPKG_OK)
    return c.err;
  DEBUG("chunk @%zu hdr: block_size=%zu c_total=%zu u_total=%zu",
        (size_t)ch->c_offset, block_size, c_total, u_total);
  if (block_size == 0 || u_total != ch->u_size) {
    DEBUG("chunk @%zu: block_size=%zu, u_total %zu vs table u_size %zu",
          (size_t)ch->c_offset, block_size, u_total, (size_t)ch->u_size);
    return UPKG_BADCHUNK;
  }

  size_t nblocks = (u_total + block_size - 1) / block_size;
  DEBUG("chunk @%zu: %zu blocks", (size_t)ch->c_offset, nblocks);
  struct cursor tbl = c;
  struct cursor dat = c;
  skip(&dat, nblocks * UPKG_BLOCK_ENT_BYTES);
  if (dat.err != UPKG_OK) {
    DEBUG("chunk @%zu: block table (%zu entries) overruns chunk",
          (size_t)ch->c_offset, nblocks);
    return UPKG_BADCHUNK;
  }

  uint8_t *dst = out + ch->u_offset;
  size_t pos = 0, c_sum = 0;
  for (size_t i = 0; i < nblocks; i++) {
    size_t bc = read_size(&tbl);
    size_t bu = read_size(&tbl);
    if (tbl.err != UPKG_OK)
      return tbl.err;

    size_t want = u_total - pos < block_size ? u_total - pos : block_size;
    if (bu != want || bc == 0) {
      DEBUG("block %zu/%zu: bc=%zu bu=%zu want=%zu", i, nblocks, bc, bu, want);
      return UPKG_BADCHUNK; // every block pinned
    }
    if (!need(&dat, bc)) {
      DEBUG("block %zu/%zu: bc=%zu overruns chunk data", i, nblocks, bc);
      return UPKG_BADCHUNK;
    }

    lzo_uint got = bu;
    int rc =
        lzo1x_decompress_safe(dat.ptr, (lzo_uint)bc, dst + pos, &got, NULL);
    if (rc != LZO_E_OK || got != bu) {
      DEBUG("block %zu/%zu @%zu: lzo rc=%d got=%zu want=%zu", i, nblocks,
            dat.offset, rc, (size_t)got, bu);
      return UPKG_BADLZO;
    }

    advance(&dat, bc);
    pos += bu;
    c_sum += bc;
  }
  if (pos != u_total || c_sum != c_total) {
    DEBUG("chunk @%zu totals: pos=%zu/%zu c_sum=%zu/%zu", (size_t)ch->c_offset,
          pos, u_total, c_sum, c_total);
    return UPKG_BADCHUNK;
  }
  return UPKG_OK;
}

upkg_err upkg_parse(const uint8_t *in, size_t len, struct upkg *out,
                    size_t *pos) {
  if (!in || !out || !pos)
    return UPKG_BADARGS;

  struct cursor c = {.ptr = in, .left = len};
  struct upkg p = {0};

  DEBUG("parsing %zu bytes", len);
  uint32_t sig = read_u32(&c);
  if (sig != UPKG_SIGNATURE && c.err == UPKG_OK) {
    DEBUG("bad sig 0x%08" PRIx32 " (want 0x%08" PRIx32 ")", sig,
          (uint32_t)UPKG_SIGNATURE);
    c.err = UPKG_BADSIG;
  }

  p.version = read_u16(&c);
  p.licensee_version = read_u16(&c);
  p.header_size = read_i32(&c);
  p.folder_name = read_str(&c);
  p.flags_offset = c.offset;
  p.flags = read_u32(&c);
  DEBUG("version=%u licensee=%u header_size=%" PRId32 " folder=\"%.*s\" "
        "flags=0x%08" PRIx32,
        (unsigned)p.version, (unsigned)p.licensee_version,
        (int32_t)p.header_size, (int)p.folder_name.len, p.folder_name.data,
        (uint32_t)p.flags);

  /* NameCount, NameOffset, ExportCount, ExportOffset,
   * ImportCount, ImportOffset, DependsOffset, GUID, GenerationInfo[]
   */
  skip(&c, 7 * sizeof(uint32_t));
  skip(&c, UPKG_GUID_BYTES);
  skip(&c, UPKG_GENINFO_BYTES * read_count(&c));

  p.engine_version = read_i32(&c);
  p.cooker_version = read_i32(&c);
  p.compression_flag = read_u32(&c);
  DEBUG("engine=%" PRId32 " cooker=%" PRId32 " compression=%" PRIu32,
        (int32_t)p.engine_version, (int32_t)p.cooker_version,
        (uint32_t)p.compression_flag);

  p.chunk_count = read_count(&c);
  p.chunk_table = c.offset;
  skip(&c, UPKG_CHUNK_BYTES * p.chunk_count);
  DEBUG("chunk_count=%zu chunk_table=@%zu", (size_t)p.chunk_count,
        (size_t)p.chunk_table);

  *pos = c.offset;
  if (c.err != UPKG_OK) {
    DEBUG("parse failed at off %zu: %s", c.offset, upkg_strerror(c.err));
    return c.err;
  }
  DEBUG("summary parsed, ends @%zu", c.offset);
  *out = p;
  return UPKG_OK;
}

upkg_err upkg_decompress(const uint8_t *in, size_t len, const struct upkg *pkg,
                         uint8_t **out, size_t *out_len) {
  if (!in || !pkg || !out || !out_len)
    return UPKG_BADARGS;

  if (pkg->compression_flag == UPKG_COMPRESSION_NONE) {
    DEBUG("uncompressed, copying %zu bytes", len);
    uint8_t *buf = malloc(len);
    if (!buf) {
      PDEBUG("malloc(%zu)", len);
      return UPKG_NOMEM;
    }
    memcpy(buf, in, len);
    *out = buf;
    *out_len = len;
    return UPKG_OK;
  }
  if (pkg->compression_flag != UPKG_COMPRESSION_LZO) {
    DEBUG("unsupported compression %" PRIu32, (uint32_t)pkg->compression_flag);
    return UPKG_UNSUPPORTED;
  }
  if (pkg->chunk_count == 0) {
    DEBUG("LZO flag set but chunk_count == 0");
    return UPKG_BADCHUNK;
  }
  if (lzo_init() != LZO_E_OK) {
    DEBUG("lzo_init failed");
    return UPKG_BADLZO;
  }

  const struct cursor table = {.ptr = in + pkg->chunk_table,
                               .left = len - pkg->chunk_table,
                               .offset = pkg->chunk_table};

  /**
   * Pass 1
   *  Validate the table, size the output
   */
  DEBUG("pass 1: validating %zu chunks", (size_t)pkg->chunk_count);
  struct cursor c = table;
  struct upkg_chunk first = read_chunk(&c, len);
  size_t end = first.u_offset + first.u_size;
  for (size_t i = 1; i < pkg->chunk_count; i++) {
    struct upkg_chunk ch = read_chunk(&c, len);
    if (c.err != UPKG_OK)
      return c.err;
    if (ch.u_offset != end) {
      DEBUG("chunk %zu: u_off=%zu, expected contiguous %zu", i,
            (size_t)ch.u_offset, end);
      return UPKG_BADCHUNK; // contiguous: no gaps, no overlap
    }
    end = ch.u_offset + ch.u_size;
  }
  if (c.err != UPKG_OK)
    return c.err;

  /* Bytes before chunk 0 are the summary, stored verbatim. Uncompressed offsets
   * assume the chunk table is gone, so u_offset must be c_offset minus it
   */
  if (first.c_offset < pkg->chunk_table + pkg->chunk_count * UPKG_CHUNK_BYTES ||
      first.c_offset - pkg->chunk_count * UPKG_CHUNK_BYTES != first.u_offset) {
    DEBUG("summary mismatch: first c_off=%zu u_off=%zu table=@%zu+%zu",
          (size_t)first.c_offset, (size_t)first.u_offset,
          (size_t)pkg->chunk_table,
          (size_t)pkg->chunk_count * UPKG_CHUNK_BYTES);
    return UPKG_BADCHUNK;
  }

  DEBUG("output size %zu (input %zu)", end, len);
  uint8_t *buf = malloc(end);
  if (!buf) {
    PDEBUG("malloc(%zu)", end);
    return UPKG_NOMEM;
  }
  /* Summary minus the chunk table: no compression, zero chunks */
  size_t tbl_end = pkg->chunk_table + pkg->chunk_count * UPKG_CHUNK_BYTES;
  memcpy(buf, in, pkg->chunk_table);
  write_u32(buf + pkg->flags_offset, pkg->flags & ~UPKG_PKG_STORE_COMPRESSED);
  write_u32(buf + pkg->chunk_table - 8, UPKG_COMPRESSION_NONE);
  write_u32(buf + pkg->chunk_table - 4, 0);
  memcpy(buf + pkg->chunk_table, in + tbl_end, first.c_offset - tbl_end);

  /**
   * Pass 2
   *  Decompress each chunk to its uncompressed offset
   */
  DEBUG("pass 2: decompressing");
  c = table;
  for (size_t i = 0; i < pkg->chunk_count; i++) {
    struct upkg_chunk ch = read_chunk(&c, len);
    upkg_err e = c.err != UPKG_OK ? c.err : decompress_chunk(in, &ch, buf, end);
    if (e != UPKG_OK) {
      DEBUG("chunk %zu/%zu failed: %s", i, (size_t)pkg->chunk_count,
            upkg_strerror(e));
      free(buf);
      return e;
    }
  }

  DEBUG("decompressed %zu -> %zu bytes", len, end);
  *out = buf;
  *out_len = end;
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
  case UPKG_NOMEM:
    return "out of memory";
  case UPKG_UNSUPPORTED:
    return "unsupported";
  case UPKG_BADCHUNK:
    return "invalid chunk";
  case UPKG_BADSIZE:
    return "invalid size";
  case UPKG_BADLZO:
    return "LZO";
  default:
    return "unknown";
  }
}
