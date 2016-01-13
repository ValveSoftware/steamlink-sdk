// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/blink_glue.h"

#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebFileInfo.h"

namespace content {

void FileInfoToWebFileInfo(const base::File::Info& file_info,
                           blink::WebFileInfo* web_file_info) {
  DCHECK(web_file_info);
  // WebKit now expects NaN as uninitialized/null Date.
  if (file_info.last_modified.is_null())
    web_file_info->modificationTime = std::numeric_limits<double>::quiet_NaN();
  else
    web_file_info->modificationTime = file_info.last_modified.ToDoubleT();
  web_file_info->length = file_info.size;
  if (file_info.is_directory)
    web_file_info->type = blink::WebFileInfo::TypeDirectory;
  else
    web_file_info->type = blink::WebFileInfo::TypeFile;
}

COMPILE_ASSERT(std::numeric_limits<double>::has_quiet_NaN, has_quiet_NaN);

}  // namespace content
