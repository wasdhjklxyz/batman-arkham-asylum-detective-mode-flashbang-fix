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

#include "upkg.h"

int main(int argc, char **argv) {
  if (argc != 2)
    return 1;

  const int fd = open(argv[1], O_RDONLY);
  if (fd == -1) {
    perror("open");
    return 1;
  }

  struct stat st = {0};
  if (fstat(fd, &st) != 0) {
    perror("fstat");
    close(fd);
    return 1;
  }

  void *pa = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (pa == MAP_FAILED) {
    perror("mmap");
    close(fd);
    return 1;
  }

  struct upkg pkg;
  size_t pos;
  upkg_err err = upkg_parse(pa, (size_t)st.st_size, &pkg, &pos);
  if (err != UPKG_OK) {
    fprintf(stderr, "upkg_parse: %s at 0x%zx\n", upkg_strerror(err), pos);
    munmap(pa, st.st_size);
    close(fd);
    return 1;
  }

  printf("Version: %hu\n", pkg.version);
  printf("Licensee Version: %hu\n", pkg.licensee_version);
  printf("Header Size: %d\n", pkg.header_size);
  printf("Folder Name: %s\n", pkg.folder_name.data);
  printf("Flags: 0x%x\n", pkg.flags);
  printf("Engine Version: %d\n", pkg.engine_version);
  printf("Cooker Version: %hu/%hu\n", pkg.cooker_version & 0xFFFF,
         pkg.cooker_version >> 16); // name/type
  printf("Compression Flag: %u\n", pkg.compression_flag);
  printf("Compressed Chunks: %zu\n", pkg.chunk_count);

  uint8_t *buf;
  size_t buf_len;
  err = upkg_decompress(pa, (size_t)st.st_size, &pkg, &buf, &buf_len);
  if (err != UPKG_OK) {
    fprintf(stderr, "upkg_decompress: %s\n", upkg_strerror(err));
    munmap(pa, st.st_size);
    close(fd);
    return 1;
  }

  munmap(pa, st.st_size);
  close(fd);

  FILE *fp = fopen("decompressed.u", "w");
  if (!fp) {
    perror("fopen");
    return 1;
  }
  if (fwrite(buf, 1, buf_len, fp) != buf_len) {
    perror("fwrite");
    fclose(fp);
    free(buf);
    return 1;
  }

  fclose(fp);
  free(buf);
  return 0;
}
