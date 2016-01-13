/*
 * Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_TESTS_MANIFEST_FILE_STRING_BUFFER_H_
#define NATIVE_CLIENT_TESTS_MANIFEST_FILE_STRING_BUFFER_H_

#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

namespace nacl {

class StringBuffer {
 public:
  StringBuffer();
  ~StringBuffer();
  void DiscardOutput();
  void Printf(char const *fmt, ...) __attribute__((format(printf, 2, 3)));
  std::string ToString() {
    return std::string(buffer_, insert_);
  }

 private:
  size_t  nbytes_;
  size_t  insert_;
  char    *buffer_;
};

}  // namespace nacl

#endif
