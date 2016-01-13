// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/download_save_info.h"

namespace content {

DownloadSaveInfo::DownloadSaveInfo()
    : offset(0), prompt_for_save_location(false) {
}

DownloadSaveInfo::~DownloadSaveInfo() {
}

}  // namespace content
