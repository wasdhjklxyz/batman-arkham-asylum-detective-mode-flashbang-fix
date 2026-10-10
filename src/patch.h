/**
 * patch.h - Detective mode flashbang patch
 *
 * Copyright (C) 2026 uiop <uiop@wasdhjkl.xyz>
 */

#ifndef BAADMFF_PATCH_H
#define BAADMFF_PATCH_H

#include <stddef.h>
#include <stdint.h>

enum patch_result {
  PATCH_OK,        // patched exactly one site
  PATCH_ALREADY,   // already-patched bytes found, nothing to do
  PATCH_NOTFOUND,  // neither original nor patched bytes found
  PATCH_AMBIGUOUS, // needle matched more than once, refusing to guess
};

/**
 * Patch buf in place. buf is left untouched unless PATCH_OK.
 */
enum patch_result patch_apply(uint8_t *buf, size_t len);

#endif // BAADMFF_PATCH_H
