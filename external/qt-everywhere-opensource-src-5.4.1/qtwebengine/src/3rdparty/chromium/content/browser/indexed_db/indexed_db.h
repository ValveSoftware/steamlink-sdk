// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_H_

namespace content {

namespace indexed_db {

enum TransactionMode {
  TRANSACTION_READ_ONLY = 0,
  TRANSACTION_READ_WRITE = 1,
  TRANSACTION_VERSION_CHANGE = 2
};

enum CursorDirection {
  CURSOR_NEXT = 0,
  CURSOR_NEXT_NO_DUPLICATE = 1,
  CURSOR_PREV = 2,
  CURSOR_PREV_NO_DUPLICATE = 3,
};

enum CursorType {
  CURSOR_KEY_AND_VALUE = 0,
  CURSOR_KEY_ONLY
};

}  // namespace indexed_db

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_H_
