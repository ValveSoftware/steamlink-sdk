// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_ERROR_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_ERROR_H_

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"

namespace content {

class IndexedDBDatabaseError {
 public:
  explicit IndexedDBDatabaseError(uint16 code) : code_(code) {}
  IndexedDBDatabaseError(uint16 code, const char* message)
      : code_(code), message_(base::ASCIIToUTF16(message)) {}
  IndexedDBDatabaseError(uint16 code, const base::string16& message)
      : code_(code), message_(message) {}
  ~IndexedDBDatabaseError() {}

  uint16 code() const { return code_; }
  const base::string16& message() const { return message_; }

 private:
  const uint16 code_;
  const base::string16 message_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBDatabaseError);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_ERROR_H_
