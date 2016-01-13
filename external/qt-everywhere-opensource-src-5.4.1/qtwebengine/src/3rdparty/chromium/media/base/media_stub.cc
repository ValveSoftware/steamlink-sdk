// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media.h"

#include "base/files/file_path.h"

// This file is intended for platforms that don't need to load any media
// libraries (e.g., Android).
namespace media {
namespace internal {

bool InitializeMediaLibraryInternal(const base::FilePath& module_dir) {
  return true;
}

}  // namespace internal
}  // namespace media
