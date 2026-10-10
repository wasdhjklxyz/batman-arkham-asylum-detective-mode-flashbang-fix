/**
 * io.c - Input/output utilities
 *
 * Copyright (C) 2026 uiop <uiop@wasdhjkl.xyz>
 */

#define _POSIX_C_SOURCE 200809L // O_CLOEXEC, fsync, mmap under strict -std=

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "debug.h"
#include "io.h"

#define TMP_SUFFIX ".tmp"

int io_map_file(const char *path, struct io_map *out) {
  if (!path || !out) {
    errno = EINVAL;
    return -1;
  }

  const int fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd == -1) {
    PDEBUG("open %s", path);
    return -1;
  }

  int ret = -1;
  struct stat st;
  if (fstat(fd, &st) != 0) {
    PDEBUG("fstat %s", path);
    goto out;
  }
  if (!S_ISREG(st.st_mode)) {
    DEBUG("%s: not a regular file", path);
    errno = EINVAL;
    goto out;
  }
  if (st.st_size < 0 || (uintmax_t)st.st_size > SIZE_MAX) {
    DEBUG("%s: bad size %lld", path, (long long)st.st_size);
    errno = EFBIG;
    goto out;
  }

  size_t len = (size_t)st.st_size;
  if (len == 0) { // mmap(len = 0) is EINVAL
    DEBUG("%s: empty file", path);
    *out = (struct io_map){.pa = NULL, .len = 0};
    ret = 0;
    goto out;
  }

  void *pa = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
  if (pa == MAP_FAILED) {
    PDEBUG("mmap %s (%zu bytes)", path, len);
    goto out;
  }

  DEBUG("mapped %s: %zu bytes @%p", path, len, pa);
  *out = (struct io_map){.pa = pa, .len = len};
  ret = 0;

out: {
  int e = errno;
  close(fd);
  errno = e;
}
  return ret;
}

void io_unmap(struct io_map *m) {
  if (!m)
    return;
  if (m->pa && m->len && munmap((void *)m->pa, m->len) != 0)
    PDEBUG("munmap %p (%zu bytes)", (const void *)m->pa, m->len);
  *m = (struct io_map){0};
}

int io_write_file(const char *path, const void *buf, size_t len) {
  if (!path || (!buf && len)) {
    errno = EINVAL;
    return -1;
  }

  size_t plen = strlen(path);
  char *tmp = malloc(plen + sizeof(TMP_SUFFIX));
  if (!tmp) {
    PDEBUG("malloc");
    return -1;
  }
  memcpy(tmp, path, plen);
  memcpy(tmp + plen, TMP_SUFFIX, sizeof(TMP_SUFFIX));

  int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
  if (fd == -1) {
    PDEBUG("open %s", tmp);
    free(tmp);
    return -1;
  }

  const uint8_t *p = buf;
  size_t left = len;
  while (left > 0) {
    ssize_t n = write(fd, p, left);
    if (n == -1) {
      if (errno == EINTR)
        continue;
      PDEBUG("write %s (%zu/%zu written)", tmp, len - left, len);
      goto fail;
    }
    if (n == 0) { // shouldn't happen on a regular file; don't spin
      DEBUG("write %s returned 0 (%zu/%zu written)", tmp, len - left, len);
      errno = EIO;
      goto fail;
    }
    p += n;
    left -= (size_t)n;
  }

  if (fsync(fd) != 0) {
    PDEBUG("fsync %s", tmp);
    goto fail;
  }
  if (close(fd) != 0) {
    fd = -1; // released even on error (linux)
    PDEBUG("close %s", tmp);
    goto fail;
  }
  fd = -1;

  if (rename(tmp, path) != 0) {
    PDEBUG("rename %s -> %s", tmp, path);
    goto fail;
  }

  DEBUG("wrote %zu bytes to %s", len, path);
  free(tmp);
  return 0;

fail: {
  int e = errno;
  if (fd != -1)
    close(fd);
  unlink(tmp);
  free(tmp);
  errno = e;
}
  return -1;
}
