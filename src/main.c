/**
 * main.c - Batman: Arkham Asylum detective mode flashbang fix
 *
 * Copyright (C) 2026 uiop <uiop@wasdhjkl.xyz>
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "debug.h"
#include "upkg.h"

int main(int argc, char **argv) {
  if (argc != 2) {
    DEBUG("usage: %s <BmGame.u> (got argc=%d)", argc > 0 ? argv[0] : "baadmff",
          argc);
    return 1;
  }
  DEBUG("input: %s", argv[1]);

  const int fd = open(argv[1], O_RDONLY);
  if (fd == -1) {
    PDEBUG("open %s", argv[1]);
    return 1;
  }

  struct stat st = {0};
  if (fstat(fd, &st) != 0) {
    PDEBUG("fstat %s", argv[1]);
    close(fd);
    return 1;
  }
  DEBUG("fd=%d size=%lld", fd, (long long)st.st_size);

  void *pa = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (pa == MAP_FAILED) {
    PDEBUG("mmap %lld bytes", (long long)st.st_size);
    close(fd);
    return 1;
  }
  DEBUG("mapped @%p", pa);

  struct upkg pkg;
  size_t pos;
  upkg_err err = upkg_parse(pa, (size_t)st.st_size, &pkg, &pos);
  if (err != UPKG_OK) {
    DEBUG("upkg_parse: %s at 0x%zx", upkg_strerror(err), pos);
    munmap(pa, st.st_size);
    close(fd);
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
  err = upkg_decompress(pa, (size_t)st.st_size, &pkg, &buf, &buf_len);
  if (err != UPKG_OK) {
    DEBUG("upkg_decompress: %s", upkg_strerror(err));
    munmap(pa, st.st_size);
    close(fd);
    return 1;
  }
  DEBUG("decompressed: %lld -> %zu bytes", (long long)st.st_size, buf_len);

  munmap(pa, st.st_size);
  close(fd);

  FILE *fp = fopen("decompressed.u", "w");
  if (!fp) {
    PDEBUG("fopen decompressed.u");
    return 1;
  }
  if (fwrite(buf, 1, buf_len, fp) != buf_len) {
    PDEBUG("fwrite %zu bytes", buf_len);
    fclose(fp);
    free(buf);
    return 1;
  }
  DEBUG("wrote %zu bytes to decompressed.u", buf_len);

  fclose(fp);
  free(buf);
  return 0;
}
