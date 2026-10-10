/**
 * baadmff.h - Batman: Arkham Asylum detective mode flashbang fix
 *
 * Copyright (C) 2026 uiop <uiop@wasdhjkl.xyz>
 */

#ifndef BAADMFF_H
#define BAADMFF_H

#include <stddef.h>
#include <stdint.h>

/**
 * NOTE: web/app.js mirrors these values, keep them in sync
 */
enum baadmff_status {
  BAADMFF_OK = 0,        // patched, *out holds the new file
  BAADMFF_ALREADY = 1,   // already patched, nothing to do
  BAADMFF_NOTPKG = 2,    // not an Unreal package at all
  BAADMFF_WRONGFILE = 3, // Unreal package, but not the BmGame.u we know
  BAADMFF_AMBIGUOUS = 4, // patch site matched more than once
  BAADMFF_CORRUPT = 5,   // looks like BmGame.u but fails to parse/decompress
  BAADMFF_NOMEM = 6,
  BAADMFF_BADARGS = 7,
};

/**
 * Parse, decompress and patch BmGame.u held in `in`.
 *
 * On BAADMFF_OK `*out` is a malloc'd buffer of `*out_len` bytes the caller
 * frees. On anything else `*out` and `*out_len` are untouched.
 */
enum baadmff_status baadmff_patch(const uint8_t *in, size_t len, uint8_t **out,
                                  size_t *out_len);

/**
 * Human readable, single line description of a status.
 */
const char *baadmff_strerror(enum baadmff_status s);

#endif // BAADMFF_H
