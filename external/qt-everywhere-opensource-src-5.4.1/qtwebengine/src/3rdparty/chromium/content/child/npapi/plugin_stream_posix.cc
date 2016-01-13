// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/npapi/plugin_stream.h"

#include <string.h>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "content/child/npapi/plugin_instance.h"

namespace content {

void PluginStream::ResetTempFileHandle() {
  temp_file_ = NULL;
}

void PluginStream::ResetTempFileName() {
  temp_file_path_ = base::FilePath();
}

void PluginStream::WriteAsFile() {
  if (RequestedPluginModeIsAsFile())
    instance_->NPP_StreamAsFile(&stream_, temp_file_path_.value().c_str());
}

size_t PluginStream::WriteBytes(const char* buf, size_t length) {
  return fwrite(buf, sizeof(char), length, temp_file_);
}

bool PluginStream::OpenTempFile() {
  DCHECK_EQ(static_cast<FILE*>(NULL), temp_file_);

  if (base::CreateTemporaryFile(&temp_file_path_))
    temp_file_ = base::OpenFile(temp_file_path_, "a");

  if (!temp_file_) {
    base::DeleteFile(temp_file_path_, false);
    ResetTempFileName();
    return false;
  }
  return true;
}

void PluginStream::CloseTempFile() {
  if (!TempFileIsValid())
    return;

  base::CloseFile(temp_file_);
  ResetTempFileHandle();
}

bool PluginStream::TempFileIsValid() const {
  return temp_file_ != NULL;
}

}  // namespace content
