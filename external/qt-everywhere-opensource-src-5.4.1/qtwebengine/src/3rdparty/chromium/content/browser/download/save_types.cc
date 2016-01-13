// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/save_types.h"

namespace content {

SaveFileCreateInfo::SaveFileCreateInfo(const base::FilePath& path,
                                       const GURL& url,
                                       SaveFileSource save_source,
                                       int32 save_id)
    : path(path),
      url(url),
      save_id(save_id),
      render_process_id(-1),
      render_view_id(-1),
      request_id(-1),
      total_bytes(0),
      save_source(save_source) {
}

SaveFileCreateInfo::SaveFileCreateInfo()
    : save_id(-1),
      render_process_id(-1),
      render_view_id(-1),
      request_id(-1),
      total_bytes(0),
      save_source(SAVE_FILE_FROM_UNKNOWN) {
}

SaveFileCreateInfo::~SaveFileCreateInfo() {}

}  // namespace content
