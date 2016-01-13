// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_DUMP_CACHE_DUMP_FILES_H_
#define NET_TOOLS_DUMP_CACHE_DUMP_FILES_H_

// Performs basic inspection of the disk cache files with minimal disruption
// to the actual files (they still may change if an error is detected on the
// files).

#include "base/files/file_path.h"

// Returns the major version of the specified cache.
int GetMajorVersion(const base::FilePath& input_path);

// Dumps all entries from the cache.
int DumpContents(const base::FilePath& input_path);

// Dumps the headers of all files.
int DumpHeaders(const base::FilePath& input_path);

#endif  // NET_TOOLS_DUMP_CACHE_DUMP_FILES_H_
