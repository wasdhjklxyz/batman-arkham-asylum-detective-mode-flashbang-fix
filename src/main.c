/**
 * main.c - Batman: Arkham Asylum detective mode flashbang fix
 *
 * Copyright (C) 2026 uiop <uiop@wasdhjkl.xyz>
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "io.h"
#include "patch.h"
#include "upkg.h"

#define BAK_SUFFIX ".bak"

int main(int argc, char **argv) {
  if (argc != 2) {
    DEBUG("usage: %s <BmGame.u> (got argc=%d)", argc > 0 ? argv[0] : "baadmff",
          argc);
    return 1;
  }
  const char *path = argv[1];
  DEBUG("input: %s", path);

  int ret = 1;
  uint8_t *buf = NULL;
  char *bak = NULL;

  struct io_map in;
  if (io_map_file(path, &in) != 0)
    return 1;

  struct upkg pkg;
  size_t pos;
  upkg_err err = upkg_parse(in.pa, in.len, &pkg, &pos);
  if (err != UPKG_OK) {
    DEBUG("upkg_parse: %s at 0x%zx", upkg_strerror(err), pos);
    goto out;
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

  size_t buf_len;
  err = upkg_decompress(in.pa, in.len, &pkg, &buf, &buf_len);
  if (err != UPKG_OK) {
    DEBUG("upkg_decompress: %s", upkg_strerror(err));
    goto out;
  }
  DEBUG("decompressed: %zu -> %zu bytes", in.len, buf_len);

  switch (patch_apply(buf, buf_len)) {
  case PATCH_OK:
    break;
  case PATCH_ALREADY:
    DEBUG("%s already patched, nothing to do", path);
    ret = 0;
    goto out;
  case PATCH_NOTFOUND:
    DEBUG("patch site not found (wrong file or game version?)");
    goto out;
  case PATCH_AMBIGUOUS:
    DEBUG("patch site ambiguous, refusing to guess");
    goto out;
  }

  size_t plen = strlen(path);
  bak = malloc(plen + sizeof(BAK_SUFFIX));
  if (!bak) {
    PDEBUG("malloc");
    goto out;
  }
  memcpy(bak, path, plen);
  memcpy(bak + plen, BAK_SUFFIX, sizeof(BAK_SUFFIX));

  if (io_exists(bak)) {
    DEBUG("%s exists, keeping it", bak);
  } else if (io_write_file(bak, in.pa, in.len) != 0) {
    DEBUG("backup to %s failed, not touching %s", bak, path);
    goto out;
  }

  io_unmap(&in); // WARN: pkg.folder_name points into the map, dead after this

  if (io_write_file(path, buf, buf_len) != 0)
    goto out;
  DEBUG("patched %s (backup: %s)", path, bak);
  ret = 0;

out:
  io_unmap(&in);
  free(bak);
  free(buf);
  return ret;
}
