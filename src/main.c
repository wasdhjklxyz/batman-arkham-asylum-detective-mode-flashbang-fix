/**
 * main.c - Batman: Arkham Asylum detective mode flashbang fix
 *
 * Copyright (C) 2026 uiop <uiop@wasdhjkl.xyz>
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "baadmff.h"
#include "debug.h"
#include "io.h"

#define BAK_SUFFIX ".bak"

#define ERR(fmt, ...)                                                          \
  fprintf(stderr, "baadmff: " fmt "\n" __VA_OPT__(, ) __VA_ARGS__)

int main(int argc, char **argv) {
  if (argc != 2) {
    ERR("usage: %s <path/to/BmGame/CookedPC/BmGame.u>",
        argc > 0 ? argv[0] : "baadmff");
    return 1;
  }
  const char *path = argv[1];
  DEBUG("input: %s", path);

  int ret = 1;
  uint8_t *buf = NULL;
  char *bak = NULL;

  struct io_map in;
  if (io_map_file(path, &in) != 0) {
    ERR("%s: %s", path, strerror(errno));
    return 1;
  }

  size_t buf_len;
  enum baadmff_status s = baadmff_patch(in.pa, in.len, &buf, &buf_len);
  if (s == BAADMFF_ALREADY) {
    printf("baadmff: %s: %s\n", path, baadmff_strerror(s));
    ret = 0;
    goto out;
  }
  if (s != BAADMFF_OK) {
    ERR("%s: %s", path, baadmff_strerror(s));
    goto out;
  }

  size_t plen = strlen(path);
  bak = malloc(plen + sizeof(BAK_SUFFIX));
  if (!bak) {
    ERR("%s", strerror(errno));
    goto out;
  }
  memcpy(bak, path, plen);
  memcpy(bak + plen, BAK_SUFFIX, sizeof(BAK_SUFFIX));

  if (io_exists(bak)) {
    DEBUG("%s exists, keeping it", bak);
  } else if (io_write_file(bak, in.pa, in.len) != 0) {
    ERR("%s: %s (not touching %s)", bak, strerror(errno), path);
    goto out;
  }

  io_unmap(&in);

  if (io_write_file(path, buf, buf_len) != 0) {
    ERR("%s: %s", path, strerror(errno));
    goto out;
  }
  printf("baadmff: patched %s (backup: %s)\n", path, bak);
  ret = 0;

out:
  io_unmap(&in);
  free(bak);
  free(buf);
  return ret;
}
