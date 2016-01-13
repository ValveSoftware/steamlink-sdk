// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/blob/file_stream_reader.h"

#include "base/time/time.h"

namespace webkit_blob {

// Verify if the underlying file has not been modified.
bool FileStreamReader::VerifySnapshotTime(
    const base::Time& expected_modification_time,
    const base::File::Info& file_info) {
  return expected_modification_time.is_null() ||
         expected_modification_time.ToTimeT() ==
             file_info.last_modified.ToTimeT();
}

}  // namespace webkit_blob
