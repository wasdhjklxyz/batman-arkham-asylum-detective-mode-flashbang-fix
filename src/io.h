/**
 * io.h - Input/output utilities
 *
 * Copyright (C) 2026 uiop <uiop@wasdhjkl.xyz>
 */

#ifndef BAADMFF_IO_H
#define BAADMFF_IO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct io_map {
  const uint8_t *pa;
  size_t len;
};

/**
 * Map a regular file read-only. An empty file yields { NULL, 0 }.
 * Returns 0 on success, -1 with errno set on failure.
 */
int io_map_file(const char *path, struct io_map *out);

/**
 * Unmap and zero. Safe on a zeroed or empty map.
 */
void io_unmap(struct io_map *m);

/**
 * Atomically replace path with buf: write path.tmp, fsync, rename.
 * Returns 0 on success, -1 with errno set on failure (path untouched).
 */
int io_write_file(const char *path, const void *buf, size_t len);

/**
 * True if path exists (any file type).
 *
 * WARN: Racy by nature
 */
bool io_exists(const char *path);

#endif // BAADMFF_IO_H
