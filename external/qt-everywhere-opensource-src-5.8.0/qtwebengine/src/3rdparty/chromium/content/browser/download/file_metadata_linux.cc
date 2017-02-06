// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/file_metadata_linux.h"

#include <stddef.h>
#include <sys/types.h>
#include <sys/xattr.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "url/gurl.h"

namespace content {

const char kSourceURLAttrName[] = "user.xdg.origin.url";
const char kReferrerURLAttrName[] = "user.xdg.referrer.url";

static void SetExtendedFileAttribute(const char* path, const char* name,
                                     const char* value, size_t value_size,
                                     int flags) {
  int result = setxattr(path, name, value, value_size, flags);
  if (result) {
    DPLOG(ERROR)
        << "Could not set extended attribute " << name << " on file " << path;
  }
}

void AddOriginMetadataToFile(const base::FilePath& file, const GURL& source,
                             const GURL& referrer) {
  DCHECK(base::PathIsWritable(file));
  if (source.is_valid()) {
    SetExtendedFileAttribute(file.value().c_str(), kSourceURLAttrName,
        source.spec().c_str(), source.spec().length(), 0);
  }
  if (referrer.is_valid()) {
    SetExtendedFileAttribute(file.value().c_str(), kReferrerURLAttrName,
        referrer.spec().c_str(), referrer.spec().length(), 0);
  }
}

}  // namespace content
