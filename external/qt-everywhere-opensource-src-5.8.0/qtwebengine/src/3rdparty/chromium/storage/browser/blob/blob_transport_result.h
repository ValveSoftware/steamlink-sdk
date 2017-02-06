// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_TRANSPORT_RESULT_H_
#define STORAGE_BROWSER_BLOB_BLOB_TRANSPORT_RESULT_H_

namespace storage {

// This 'result' enum is used in the blob transport logic.
enum class BlobTransportResult {
  // This means we should flag the incoming IPC as a bad IPC.
  BAD_IPC,
  // Cancel reasons.
  CANCEL_MEMORY_FULL,
  CANCEL_FILE_ERROR,
  CANCEL_REFERENCED_BLOB_BROKEN,
  CANCEL_UNKNOWN,
  // This means we're waiting on responses from the renderer.
  PENDING_RESPONSES,
  // We're done registering or transferring the blob.
  DONE
};

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_TRANSPORT_RESULT_H_
