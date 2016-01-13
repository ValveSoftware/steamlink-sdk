/*
 * Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdarg.h>

#include "ppapi/native_client/src/untrusted/nacl_ppapi_util/string_buffer.h"

namespace nacl {

StringBuffer::StringBuffer() {
  nbytes_ = 1024;
  insert_ = 0;
  buffer_ = reinterpret_cast<char *>(malloc(nbytes_));
  if (NULL == buffer_) {
    perror("StringBuffer Ctor malloc");
    abort();
  }
  buffer_[0] = '\0';
}

void StringBuffer::DiscardOutput() {
  insert_ = 0;
  buffer_[0] = '\0';
}

StringBuffer::~StringBuffer() {
  nbytes_ = 0;
  insert_ = 0;
  free(buffer_);
  buffer_ = NULL;
}

void StringBuffer::Printf(char const *fmt, ...) {
  size_t space;
  char *insert_pt;
  va_list ap;
  size_t written = 0;
  char *new_buffer;

  for (;;) {
    space = nbytes_ - insert_;
    insert_pt = buffer_ + insert_;
    va_start(ap, fmt);
    written = vsnprintf(insert_pt, space, fmt, ap);
    va_end(ap);
    if (written < space) {
      insert_ += written;
      break;
    }
    // insufficient space -- grow the buffer
    new_buffer = reinterpret_cast<char *>(realloc(buffer_, 2 * nbytes_));
    if (NULL == new_buffer) {
      // give up
      fprintf(stderr, "StringBufferPrintf: no memory\n");
      break;
    }
    nbytes_ *= 2;
    buffer_ = new_buffer;
  }
}

}  // namespace nacl
