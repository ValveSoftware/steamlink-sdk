// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/indexed_db_info.h"

namespace content {

IndexedDBInfo::IndexedDBInfo(const GURL& origin,
                             int64 size,
                             const base::Time& last_modified,
                             const base::FilePath& path,
                             size_t connection_count)
    : origin_(origin),
      size_(size),
      last_modified_(last_modified),
      path_(path),
      connection_count_(connection_count) {}

}  // namespace content
