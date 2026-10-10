/**
 * baadmff.c - Batman: Arkham Asylum detective mode flashbang fix
 *
 * Copyright (C) 2026 uiop <uiop@wasdhjkl.xyz>
 */

#include <stdlib.h>

#include "baadmff.h"
#include "debug.h"
#include "patch.h"
#include "upkg.h"

/* Batman: Arkham Asylum GOTY `BmGame.u` */
#define BMGAME_VERSION 576
#define BMGAME_LICENSEE 21

static enum baadmff_status from_upkg(upkg_err err, size_t pos) {
  switch (err) {
  case UPKG_OK:
    return BAADMFF_OK;
  case UPKG_BADARGS:
    return BAADMFF_BADARGS;
  case UPKG_BADSIG:
    return BAADMFF_NOTPKG;
  case UPKG_TRUNC:
    return pos < 4 ? BAADMFF_NOTPKG : BAADMFF_CORRUPT; // too short for sig
  case UPKG_NOMEM:
    return BAADMFF_NOMEM;
  case UPKG_UNSUPPORTED:
    return BAADMFF_WRONGFILE;
  default:
    return BAADMFF_CORRUPT;
  }
}

enum baadmff_status baadmff_patch(const uint8_t *in, size_t len, uint8_t **out,
                                  size_t *out_len) {
  if (!in || !out || !out_len)
    return BAADMFF_BADARGS;

  struct upkg pkg;
  size_t pos = 0;
  upkg_err err = upkg_parse(in, len, &pkg, &pos);
  if (err != UPKG_OK) {
    DEBUG("upkg_parse: %s at 0x%zx", upkg_strerror(err), pos);
    return from_upkg(err, pos);
  }
  DEBUG("summary ends at 0x%zx", pos);

  DEBUG("Version: %hu", pkg.version);
  DEBUG("Licensee Version: %hu", pkg.licensee_version);
  DEBUG("Header Size: %d", pkg.header_size);
  DEBUG("Folder Name: %s", pkg.folder_name.data);
  DEBUG("Flags: 0x%x", pkg.flags);
  DEBUG("Engine Version: %d", pkg.engine_version);
  DEBUG("Cooker Version: %hu/%hu", pkg.cooker_version & 0xFFFF,
        pkg.cooker_version >> 16); // name/type
  DEBUG("Compression Flag: %u", pkg.compression_flag);
  DEBUG("Compressed Chunks: %zu", pkg.chunk_count);

  if (pkg.version != BMGAME_VERSION || pkg.licensee_version != BMGAME_LICENSEE) {
    DEBUG("want version %d/%d, got %hu/%hu", BMGAME_VERSION, BMGAME_LICENSEE,
          pkg.version, pkg.licensee_version);
    return BAADMFF_WRONGFILE;
  }

  uint8_t *buf;
  size_t buf_len;
  err = upkg_decompress(in, len, &pkg, &buf, &buf_len);
  if (err != UPKG_OK) {
    DEBUG("upkg_decompress: %s", upkg_strerror(err));
    return from_upkg(err, pos);
  }
  DEBUG("decompressed: %zu -> %zu bytes", len, buf_len);

  enum baadmff_status s;
  switch (patch_apply(buf, buf_len)) {
  case PATCH_OK:
    *out = buf;
    *out_len = buf_len;
    return BAADMFF_OK;
  case PATCH_ALREADY:
    s = BAADMFF_ALREADY;
    break;
  case PATCH_NOTFOUND:
    s = BAADMFF_WRONGFILE;
    break;
  case PATCH_AMBIGUOUS:
    s = BAADMFF_AMBIGUOUS;
    break;
  default:
    s = BAADMFF_CORRUPT;
    break;
  }
  free(buf);
  return s;
}

const char *baadmff_strerror(enum baadmff_status s) {
  switch (s) {
  case BAADMFF_OK:
    return "patched";
  case BAADMFF_ALREADY:
    return "already patched, nothing to do";
  case BAADMFF_NOTPKG:
    return "not an Unreal package, pick BmGame.u from BmGame/CookedPC";
  case BAADMFF_WRONGFILE:
    return "not the BmGame.u this fix knows (only Steam GOTY is supported)";
  case BAADMFF_AMBIGUOUS:
    return "patch site found more than once (modded file?), refusing to guess";
  case BAADMFF_CORRUPT:
    return "BmGame.u looks damaged, verify game files in Steam and retry";
  case BAADMFF_NOMEM:
    return "out of memory";
  case BAADMFF_BADARGS:
    return "invalid argument(s)";
  default:
    return "unknown error";
  }
}
