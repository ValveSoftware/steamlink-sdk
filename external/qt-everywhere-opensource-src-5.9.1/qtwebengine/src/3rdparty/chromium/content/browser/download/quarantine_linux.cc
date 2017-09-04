// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/quarantine.h"

#include <stddef.h>
#include <sys/types.h>
#include <sys/xattr.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/download/quarantine_constants_linux.h"
#include "url/gurl.h"

namespace content {

const char kSourceURLExtendedAttrName[] = "user.xdg.origin.url";
const char kReferrerURLExtendedAttrName[] = "user.xdg.referrer.url";

namespace {

bool SetExtendedFileAttribute(const char* path,
                              const char* name,
                              const char* value,
                              size_t value_size,
                              int flags) {
  base::ThreadRestrictions::AssertIOAllowed();
  int result = setxattr(path, name, value, value_size, flags);
  if (result) {
    DPLOG(ERROR) << "Could not set extended attribute " << name << " on file "
                 << path;
    return false;
  }
  return true;
}

}  // namespace

QuarantineFileResult QuarantineFile(const base::FilePath& file,
                                    const GURL& source_url,
                                    const GURL& referrer_url,
                                    const std::string& client_guid) {
  DCHECK(base::PathIsWritable(file));

  bool source_succeeded =
      source_url.is_valid() &&
      SetExtendedFileAttribute(file.value().c_str(), kSourceURLExtendedAttrName,
                               source_url.spec().c_str(),
                               source_url.spec().length(), 0);

  // Referrer being empty is not considered an error. This could happen if the
  // referrer policy resulted in an empty referrer for the download request.
  bool referrer_succeeded =
      !referrer_url.is_valid() ||
      SetExtendedFileAttribute(
          file.value().c_str(), kReferrerURLExtendedAttrName,
          referrer_url.spec().c_str(), referrer_url.spec().length(), 0);
  return source_succeeded && referrer_succeeded
             ? QuarantineFileResult::OK
             : QuarantineFileResult::ANNOTATION_FAILED;
}

}  // namespace content
