// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_QUARANTINE_CONSTANTS_LINUX_H_
#define CONTENT_BROWSER_DOWNLOAD_QUARANTINE_CONSTANTS_LINUX_H_

#include "content/common/content_export.h"

namespace content {

// Attribute names to be used with setxattr and friends.
//
// The source URL attribute is part of the XDG standard.
// The referrer URL attribute is not part of the XDG standard,
// but it is used to keep the naming consistent.
// http://freedesktop.org/wiki/CommonExtendedAttributes
CONTENT_EXPORT extern const char kSourceURLExtendedAttrName[];
CONTENT_EXPORT extern const char kReferrerURLExtendedAttrName[];

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_QUARANTINE_CONSTANTS_LINUX_H_
