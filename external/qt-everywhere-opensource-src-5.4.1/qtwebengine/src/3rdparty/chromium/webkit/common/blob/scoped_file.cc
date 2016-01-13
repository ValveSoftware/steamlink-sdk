// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/common/blob/scoped_file.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/task_runner.h"

namespace webkit_blob {

ScopedFile::ScopedFile()
    : scope_out_policy_(DONT_DELETE_ON_SCOPE_OUT) {
}

ScopedFile::ScopedFile(
    const base::FilePath& path, ScopeOutPolicy policy,
    base::TaskRunner* file_task_runner)
    : path_(path),
      scope_out_policy_(policy),
      file_task_runner_(file_task_runner) {
  DCHECK(path.empty() || policy != DELETE_ON_SCOPE_OUT || file_task_runner)
      << "path:" << path.value()
      << " policy:" << policy
      << " runner:" << file_task_runner;
}

ScopedFile::ScopedFile(RValue other) {
  MoveFrom(*other.object);
}

ScopedFile::~ScopedFile() {
  Reset();
}

void ScopedFile::AddScopeOutCallback(
    const ScopeOutCallback& callback,
    base::TaskRunner* callback_runner) {
  if (!callback_runner)
    callback_runner = base::MessageLoopProxy::current().get();
  scope_out_callbacks_.push_back(std::make_pair(callback, callback_runner));
}

base::FilePath ScopedFile::Release() {
  base::FilePath path = path_;
  path_.clear();
  scope_out_callbacks_.clear();
  scope_out_policy_ = DONT_DELETE_ON_SCOPE_OUT;
  return path;
}

void ScopedFile::Reset() {
  if (path_.empty())
    return;

  for (ScopeOutCallbackList::iterator iter = scope_out_callbacks_.begin();
       iter != scope_out_callbacks_.end(); ++iter) {
    iter->second->PostTask(FROM_HERE, base::Bind(iter->first, path_));
  }

  if (scope_out_policy_ == DELETE_ON_SCOPE_OUT) {
    file_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&base::DeleteFile),
                   path_, false /* recursive */));
  }

  // Clear all fields.
  Release();
}

void ScopedFile::MoveFrom(ScopedFile& other) {
  Reset();

  scope_out_policy_ = other.scope_out_policy_;
  scope_out_callbacks_.swap(other.scope_out_callbacks_);
  file_task_runner_ = other.file_task_runner_;
  path_ = other.Release();
}

}  // namespace webkit_blob
