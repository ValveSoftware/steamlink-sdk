// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/file_reader.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

FileReader::FileReader(
    const extensions::ExtensionResource& resource,
    const OptionalFileThreadTaskCallback& optional_file_thread_task_callback,
    const DoneCallback& done_callback)
    : resource_(resource),
      optional_file_thread_task_callback_(optional_file_thread_task_callback),
      done_callback_(done_callback),
      origin_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

void FileReader::Start() {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&FileReader::ReadFileOnBackgroundThread, this));
}

FileReader::~FileReader() {}

void FileReader::ReadFileOnBackgroundThread() {
  std::unique_ptr<std::string> data(new std::string());
  bool success = base::ReadFileToString(resource_.GetFilePath(), data.get());

  if (!optional_file_thread_task_callback_.is_null()) {
    if (success) {
      base::ResetAndReturn(&optional_file_thread_task_callback_)
          .Run(data.get());
    } else {
      optional_file_thread_task_callback_.Reset();
    }
  }

  origin_task_runner_->PostTask(
      FROM_HERE, base::Bind(base::ResetAndReturn(&done_callback_), success,
                            base::Passed(std::move(data))));
}
