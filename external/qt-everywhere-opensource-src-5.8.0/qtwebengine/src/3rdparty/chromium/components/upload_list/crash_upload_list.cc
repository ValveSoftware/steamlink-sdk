// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/upload_list/crash_upload_list.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/threading/sequenced_worker_pool.h"

// static
const char* CrashUploadList::kReporterLogFilename = "uploads.log";

CrashUploadList::CrashUploadList(
    Delegate* delegate,
    const base::FilePath& upload_log_path,
    const scoped_refptr<base::SequencedWorkerPool>& worker_pool)
    : UploadList(delegate, upload_log_path, worker_pool) {}

CrashUploadList::~CrashUploadList() {}
