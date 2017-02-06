// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/file_reader.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

FileReader::FileReader(const extensions::ExtensionResource& resource,
                       const Callback& callback)
    : resource_(resource),
      callback_(callback),
      origin_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

void FileReader::Start() {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&FileReader::ReadFileOnBackgroundThread, this));
}

FileReader::~FileReader() {}

void FileReader::ReadFileOnBackgroundThread() {
  std::string data;
  bool success = base::ReadFileToString(resource_.GetFilePath(), &data);
  origin_task_runner_->PostTask(FROM_HERE,
                                base::Bind(callback_, success, data));
}
