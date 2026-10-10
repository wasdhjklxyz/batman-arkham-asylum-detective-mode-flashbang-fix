/**
 * debug.h - Debug logging macros
 *
 * Copyright (C) 2026 uiop <uiop@wasdhjkl.xyz>
 */

#ifndef BAADMFF_DEBUG_H
#define BAADMFF_DEBUG_H

#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifndef NDEBUG

// Both preserve errno, callers report it after logging

#define DEBUG(fmt, ...)                                                        \
  do {                                                                         \
    int log_errno_ = errno;                                                    \
    fprintf(stderr, "%s:%d:%s: " fmt "\n", __FILE__, __LINE__,                 \
            __func__ __VA_OPT__(, ) __VA_ARGS__);                              \
    errno = log_errno_;                                                        \
  } while (0)

#define PDEBUG(fmt, ...)                                                       \
  do {                                                                         \
    int log_errno_ = errno;                                                    \
    fprintf(stderr, "%s:%d:%s: " fmt ": %s\n", __FILE__, __LINE__,             \
            __func__ __VA_OPT__(, ) __VA_ARGS__, strerror(log_errno_));        \
    errno = log_errno_;                                                        \
  } while (0)

#else // NDEBUG

#define DEBUG(fmt, ...)                                                        \
  do {                                                                         \
    if (0)                                                                     \
      fprintf(stderr, fmt __VA_OPT__(, ) __VA_ARGS__);                         \
  } while (0)

#define PDEBUG(fmt, ...) DEBUG(fmt __VA_OPT__(, ) __VA_ARGS__)

#endif // NDEBUG

#endif // BAADMFF_DEBUG_H
