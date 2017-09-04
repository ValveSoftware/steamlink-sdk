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

DownloadSaveInfo::DownloadSaveInfo(DownloadSaveInfo&& that)
    : file_path(std::move(that.file_path)),
      suggested_name(std::move(that.suggested_name)),
      file(std::move(that.file)),
      offset(that.offset),
      hash_state(std::move(that.hash_state)),
      hash_of_partial_file(std::move(that.hash_of_partial_file)),
      prompt_for_save_location(that.prompt_for_save_location) {}

}  // namespace content
