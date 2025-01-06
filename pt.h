#pragma once

#include <stddef.h>
#include <stdint.h>

/* Protothread status values */
#define PT_STATUS_BLOCKED 0
#define PT_STATUS_FINISHED -1
#define PT_STATUS_YIELDED -2
#define PT_STATUS_SUSPENDED -3

/* Helper macros to generate unique labels */
#define _pt_line3(name, line) _pt_##name##line
#define _pt_line2(name, line) _pt_line3(name, line)
#define _pt_line(name) _pt_line2(name, __LINE__)

/*
 * Local continuation based on goto label references.
 *
 * Pros: works with all control sturctures.
 * Cons: requires GCC or Clang, doesn't preserve local variables.
 */
struct pt {
    void *label;
    int8_t status;
};

#define pt_init()                                                              \
  { .label = NULL, .status = PT_STATUS_BLOCKED }

#define pt_begin(pt)                                                           \
  do {                                                                         \
    if ((pt)->label != NULL) {                                                 \
      goto *(pt)->label;                                                       \
    }                                                                          \
  } while (0)

#define pt_label(pt, stat)                                                     \
  do {                                                                         \
    (pt)->status = (stat);                                                     \
    _pt_line(label) : (pt)->label = &&_pt_line(label);                         \
  } while (0)

#define pt_end(pt) pt_label(pt, PT_STATUS_FINISHED)

/*
 * Core protothreads API
 */
#define pt_status(pt) (pt)->status

#define pt_wait(pt, cond)                                                      \
  do {                                                                         \
    pt_label(pt, PT_STATUS_BLOCKED);                                           \
    if (!(cond)) {                                                             \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define pt_yield(pt)                                                           \
  do {                                                                         \
    pt_label(pt, PT_STATUS_YIELDED);                                           \
    if (pt_status(pt) == PT_STATUS_YIELDED) {                                  \
      (pt)->status = PT_STATUS_BLOCKED;                                        \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define pt_exit(pt, stat)                                                      \
  do {                                                                         \
    pt_label(pt, stat);                                                        \
    return;                                                                    \
  } while (0)
