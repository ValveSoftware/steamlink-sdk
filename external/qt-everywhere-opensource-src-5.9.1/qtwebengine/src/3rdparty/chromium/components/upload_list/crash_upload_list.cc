// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/upload_list/crash_upload_list.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/path_service.h"

// static
const char CrashUploadList::kReporterLogFilename[] = "uploads.log";

CrashUploadList::CrashUploadList(Delegate* delegate,
                                 const base::FilePath& upload_log_path,
                                 scoped_refptr<base::TaskRunner> task_runner)
    : UploadList(delegate, upload_log_path, std::move(task_runner)) {}

CrashUploadList::~CrashUploadList() = default;
