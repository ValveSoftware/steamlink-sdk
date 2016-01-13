// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_FILE_METADATA_LINUX_H_
#define CONTENT_BROWSER_DOWNLOAD_FILE_METADATA_LINUX_H_

#include "content/common/content_export.h"

class GURL;

namespace base {
class FilePath;
}

namespace content {

// The source URL attribute is part of the XDG standard.
// The referrer URL attribute is not part of the XDG standard,
// but it is used to keep the naming consistent.
// http://freedesktop.org/wiki/CommonExtendedAttributes
CONTENT_EXPORT extern const char kSourceURLAttrName[];
CONTENT_EXPORT extern const char kReferrerURLAttrName[];

// Adds origin metadata to the file.
// |source| should be the source URL for the download, and |referrer| should be
// the URL the user initiated the download from.
CONTENT_EXPORT void AddOriginMetadataToFile(const base::FilePath& file,
                                            const GURL& source,
                                            const GURL& referrer);

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_FILE_METADATA_LINUX_H_
