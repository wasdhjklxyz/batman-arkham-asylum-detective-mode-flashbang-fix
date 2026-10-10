/**
 * upkg.h - Unreal Engine package parser
 *
 * Copyright (C) 2026 uiop <uiop@wasdhjkl.xyz>
 */

#ifndef BAADMFF_UPKG_H
#define BAADMFF_UPKG_H

#include <stddef.h>
#include <stdint.h>

struct upkg_str {
  size_t len;
  const char *data;
};

/**
 * Unreal Engine package compressed chunk.
 * https://github.com/gildor2/UEViewer/blob/master/Unreal/UnrealPackage/UnPackage.h#L45
 */
struct upkg_chunk {
  size_t u_offset; // uncompressed offset
  size_t u_size;   // uncompressed size
  size_t c_offset; // compressed offset
  size_t c_size;   // compressed size
};

/**
 * Unreal Engine package structure for versions <= 576.
 * https://eliotvu.com/page/unreal-package-file-format
 */
struct upkg {
  uint16_t version;
  uint16_t licensee_version;
  int32_t header_size;
  struct upkg_str folder_name;
  uint32_t flags;
  int32_t engine_version;
  int32_t cooker_version;
  uint32_t compression_flag;
  size_t chunk_count;
  size_t chunk_table; // offset from file
};

typedef enum {
  UPKG_OK = 0,
  UPKG_BADARGS,
  UPKG_BADSIG,
  UPKG_TRUNC,
  UPKG_BADIDX,
  UPKG_BADSTR,
  UPKG_NOMEM,
  UPKG_UNSUPPORTED,
  UPKG_BADCHUNK,
  UPKG_BADSIZE,
  UPKG_BADLZO,
} upkg_err;

/**
 * Parse Unreal Engine package.
 *
 * Based on Batman Arkham Asylum `BmGame.u` (version 576, licensee 21).
 *
 * On success fills `*out` and returns `UPKG_OK`. `out->folder_name.data` points
 * into `in` and lives as long as it does. On failure, `*out` is untouched and
 * the return value indicates why (see `upkg_strerror()`). `*pos` is where the
 * cursor stopped.
 */
upkg_err upkg_parse(const uint8_t *in, size_t len, struct upkg *out,
                    size_t *pos);

/**
 * Decompress Unreal Engine package chunks.
 *
 * Returns `UPKG_OK` on success or if no compression is used. On error, return
 * value indicates why (see `upkg_strerror()`).
 */
upkg_err upkg_decompress(const uint8_t *in, size_t len, const struct upkg *pkg,
                         uint8_t **out, size_t *out_len);

/**
 * Returns pointer to a string that describes the error code passed.
 */
const char *upkg_strerror(upkg_err err);

#endif // BAADMFF_UPKG_H
