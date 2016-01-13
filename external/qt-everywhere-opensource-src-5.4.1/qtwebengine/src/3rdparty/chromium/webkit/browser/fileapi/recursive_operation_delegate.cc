// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/recursive_operation_delegate.h"

#include "base/bind.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation_runner.h"

namespace fileapi {

namespace {
// Don't start too many inflight operations.
const int kMaxInflightOperations = 5;
}

RecursiveOperationDelegate::RecursiveOperationDelegate(
    FileSystemContext* file_system_context)
    : file_system_context_(file_system_context),
      inflight_operations_(0),
      canceled_(false) {
}

RecursiveOperationDelegate::~RecursiveOperationDelegate() {
}

void RecursiveOperationDelegate::Cancel() {
  canceled_ = true;
  OnCancel();
}

void RecursiveOperationDelegate::StartRecursiveOperation(
    const FileSystemURL& root,
    const StatusCallback& callback) {
  DCHECK(pending_directory_stack_.empty());
  DCHECK(pending_files_.empty());
  DCHECK_EQ(0, inflight_operations_);

  callback_ = callback;
  ++inflight_operations_;
  ProcessFile(
      root,
      base::Bind(&RecursiveOperationDelegate::DidTryProcessFile,
                 AsWeakPtr(), root));
}

FileSystemOperationRunner* RecursiveOperationDelegate::operation_runner() {
  return file_system_context_->operation_runner();
}

void RecursiveOperationDelegate::OnCancel() {
}

void RecursiveOperationDelegate::DidTryProcessFile(
    const FileSystemURL& root,
    base::File::Error error) {
  DCHECK(pending_directory_stack_.empty());
  DCHECK(pending_files_.empty());
  DCHECK_EQ(1, inflight_operations_);

  --inflight_operations_;
  if (canceled_ || error != base::File::FILE_ERROR_NOT_A_FILE) {
    Done(error);
    return;
  }

  pending_directory_stack_.push(std::queue<FileSystemURL>());
  pending_directory_stack_.top().push(root);
  ProcessNextDirectory();
}

void RecursiveOperationDelegate::ProcessNextDirectory() {
  DCHECK(pending_files_.empty());
  DCHECK(!pending_directory_stack_.empty());
  DCHECK(!pending_directory_stack_.top().empty());
  DCHECK_EQ(0, inflight_operations_);

  const FileSystemURL& url = pending_directory_stack_.top().front();

  ++inflight_operations_;
  ProcessDirectory(
      url,
      base::Bind(
          &RecursiveOperationDelegate::DidProcessDirectory, AsWeakPtr()));
}

void RecursiveOperationDelegate::DidProcessDirectory(
    base::File::Error error) {
  DCHECK(pending_files_.empty());
  DCHECK(!pending_directory_stack_.empty());
  DCHECK(!pending_directory_stack_.top().empty());
  DCHECK_EQ(1, inflight_operations_);

  --inflight_operations_;
  if (canceled_ || error != base::File::FILE_OK) {
    Done(error);
    return;
  }

  const FileSystemURL& parent = pending_directory_stack_.top().front();
  pending_directory_stack_.push(std::queue<FileSystemURL>());
  operation_runner()->ReadDirectory(
      parent,
      base::Bind(&RecursiveOperationDelegate::DidReadDirectory,
                 AsWeakPtr(), parent));
}

void RecursiveOperationDelegate::DidReadDirectory(
    const FileSystemURL& parent,
    base::File::Error error,
    const FileEntryList& entries,
    bool has_more) {
  DCHECK(!pending_directory_stack_.empty());
  DCHECK_EQ(0, inflight_operations_);

  if (canceled_ || error != base::File::FILE_OK) {
    Done(error);
    return;
  }

  for (size_t i = 0; i < entries.size(); i++) {
    FileSystemURL url = file_system_context_->CreateCrackedFileSystemURL(
        parent.origin(),
        parent.mount_type(),
        parent.virtual_path().Append(entries[i].name));
    if (entries[i].is_directory)
      pending_directory_stack_.top().push(url);
    else
      pending_files_.push(url);
  }

  // Wait for next entries.
  if (has_more)
    return;

  ProcessPendingFiles();
}

void RecursiveOperationDelegate::ProcessPendingFiles() {
  DCHECK(!pending_directory_stack_.empty());

  if ((pending_files_.empty() || canceled_) && inflight_operations_ == 0) {
    ProcessSubDirectory();
    return;
  }

  // Do not post any new tasks.
  if (canceled_)
    return;

  // Run ProcessFile in parallel (upto kMaxInflightOperations).
  scoped_refptr<base::MessageLoopProxy> current_message_loop =
      base::MessageLoopProxy::current();
  while (!pending_files_.empty() &&
         inflight_operations_ < kMaxInflightOperations) {
    ++inflight_operations_;
    current_message_loop->PostTask(
        FROM_HERE,
        base::Bind(&RecursiveOperationDelegate::ProcessFile,
                   AsWeakPtr(), pending_files_.front(),
                   base::Bind(&RecursiveOperationDelegate::DidProcessFile,
                              AsWeakPtr())));
    pending_files_.pop();
  }
}

void RecursiveOperationDelegate::DidProcessFile(
    base::File::Error error) {
  --inflight_operations_;
  if (error != base::File::FILE_OK) {
    // If an error occurs, invoke Done immediately (even if there remain
    // running operations). It is because in the callback, this instance is
    // deleted.
    Done(error);
    return;
  }

  ProcessPendingFiles();
}

void RecursiveOperationDelegate::ProcessSubDirectory() {
  DCHECK(pending_files_.empty());
  DCHECK(!pending_directory_stack_.empty());
  DCHECK_EQ(0, inflight_operations_);

  if (canceled_) {
    Done(base::File::FILE_ERROR_ABORT);
    return;
  }

  if (!pending_directory_stack_.top().empty()) {
    // There remain some sub directories. Process them first.
    ProcessNextDirectory();
    return;
  }

  // All subdirectories are processed.
  pending_directory_stack_.pop();
  if (pending_directory_stack_.empty()) {
    // All files/directories are processed.
    Done(base::File::FILE_OK);
    return;
  }

  DCHECK(!pending_directory_stack_.top().empty());
  ++inflight_operations_;
  PostProcessDirectory(
      pending_directory_stack_.top().front(),
      base::Bind(&RecursiveOperationDelegate::DidPostProcessDirectory,
                 AsWeakPtr()));
}

void RecursiveOperationDelegate::DidPostProcessDirectory(
    base::File::Error error) {
  DCHECK(pending_files_.empty());
  DCHECK(!pending_directory_stack_.empty());
  DCHECK(!pending_directory_stack_.top().empty());
  DCHECK_EQ(1, inflight_operations_);

  --inflight_operations_;
  pending_directory_stack_.top().pop();
  if (canceled_ || error != base::File::FILE_OK) {
    Done(error);
    return;
  }

  ProcessSubDirectory();
}

void RecursiveOperationDelegate::Done(base::File::Error error) {
  if (canceled_ && error == base::File::FILE_OK) {
    callback_.Run(base::File::FILE_ERROR_ABORT);
  } else {
    callback_.Run(error);
  }
}

}  // namespace fileapi
