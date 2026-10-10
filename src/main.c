/**
 * main.c - Batman: Arkham Asylum detective mode flashbang fix
 *
 * Copyright (C) 2026 uiop <uiop@wasdhjkl.xyz>
 */

#include <stdint.h>
#include <stdlib.h>

#include "debug.h"
#include "io.h"
#include "upkg.h"

int main(int argc, char **argv) {
  if (argc != 2) {
    DEBUG("usage: %s <BmGame.u> (got argc=%d)", argc > 0 ? argv[0] : "baadmff",
          argc);
    return 1;
  }
  DEBUG("input: %s", argv[1]);

  struct io_map in;
  if (io_map_file(argv[1], &in) != 0)
    return 1;

  struct upkg pkg;
  size_t pos;
  upkg_err err = upkg_parse(in.pa, in.len, &pkg, &pos);
  if (err != UPKG_OK) {
    DEBUG("upkg_parse: %s at 0x%zx", upkg_strerror(err), pos);
    io_unmap(&in);
    return 1;
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

  uint8_t *buf;
  size_t buf_len;
  err = upkg_decompress(in.pa, in.len, &pkg, &buf, &buf_len);
  size_t in_len = in.len;
  io_unmap(&in); // WARN: pkg.folder_name points into the map, dead after this
  if (err != UPKG_OK) {
    DEBUG("upkg_decompress: %s", upkg_strerror(err));
    return 1;
  }
  DEBUG("decompressed: %zu -> %zu bytes", in_len, buf_len);

  int ret = io_write_file("decompressed.u", buf, buf_len) == 0 ? 0 : 1;
  free(buf);
  return ret;
}
