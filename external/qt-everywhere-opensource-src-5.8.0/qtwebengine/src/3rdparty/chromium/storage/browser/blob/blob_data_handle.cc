// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_data_handle.h"

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner.h"
#include "base/time/time.h"
#include "storage/browser/blob/blob_data_snapshot.h"
#include "storage/browser/blob/blob_reader.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/fileapi/file_stream_reader.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "url/gurl.h"

namespace storage {
using BlobState = BlobStorageRegistry::BlobState;

namespace {

class FileStreamReaderProviderImpl
    : public BlobReader::FileStreamReaderProvider {
 public:
  explicit FileStreamReaderProviderImpl(FileSystemContext* file_system_context)
      : file_system_context_(file_system_context) {}
  ~FileStreamReaderProviderImpl() override {}

  std::unique_ptr<FileStreamReader> CreateForLocalFile(
      base::TaskRunner* task_runner,
      const base::FilePath& file_path,
      int64_t initial_offset,
      const base::Time& expected_modification_time) override {
    return base::WrapUnique(FileStreamReader::CreateForLocalFile(
        task_runner, file_path, initial_offset, expected_modification_time));
  }

  std::unique_ptr<FileStreamReader> CreateFileStreamReader(
      const GURL& filesystem_url,
      int64_t offset,
      int64_t max_bytes_to_read,
      const base::Time& expected_modification_time) override {
    return file_system_context_->CreateFileStreamReader(
        storage::FileSystemURL(file_system_context_->CrackURL(filesystem_url)),
        offset, max_bytes_to_read, expected_modification_time);
  }

 private:
  scoped_refptr<FileSystemContext> file_system_context_;
  DISALLOW_COPY_AND_ASSIGN(FileStreamReaderProviderImpl);
};

}  // namespace

BlobDataHandle::BlobDataHandleShared::BlobDataHandleShared(
    const std::string& uuid,
    const std::string& content_type,
    const std::string& content_disposition,
    BlobStorageContext* context)
    : uuid_(uuid),
      content_type_(content_type),
      content_disposition_(content_disposition),
      context_(context->AsWeakPtr()) {
  context_->IncrementBlobRefCount(uuid);
}

std::unique_ptr<BlobReader> BlobDataHandle::CreateReader(
    FileSystemContext* file_system_context,
    base::SequencedTaskRunner* file_task_runner) const {
  return std::unique_ptr<BlobReader>(new BlobReader(
      this, std::unique_ptr<BlobReader::FileStreamReaderProvider>(
                new FileStreamReaderProviderImpl(file_system_context)),
      file_task_runner));
}

BlobDataHandle::BlobDataHandleShared::~BlobDataHandleShared() {
  if (context_.get())
    context_->DecrementBlobRefCount(uuid_);
}

BlobDataHandle::BlobDataHandle(const std::string& uuid,
                               const std::string& content_type,
                               const std::string& content_disposition,
                               BlobStorageContext* context,
                               base::SequencedTaskRunner* io_task_runner)
    : io_task_runner_(io_task_runner),
      shared_(new BlobDataHandleShared(uuid,
                                       content_type,
                                       content_disposition,
                                       context)) {
  DCHECK(io_task_runner_.get());
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
}

BlobDataHandle::BlobDataHandle(const BlobDataHandle& other) {
  io_task_runner_ = other.io_task_runner_;
  shared_ = other.shared_;
}

BlobDataHandle::~BlobDataHandle() {
  if (!io_task_runner_->RunsTasksOnCurrentThread()) {
    BlobDataHandleShared* raw = shared_.get();
    raw->AddRef();
    shared_ = nullptr;
    io_task_runner_->ReleaseSoon(FROM_HERE, raw);
  }
}

bool BlobDataHandle::IsBeingBuilt() const {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  if (!shared_->context_)
    return false;
  return shared_->context_->IsBeingBuilt(shared_->uuid_);
}

bool BlobDataHandle::IsBroken() const {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  if (!shared_->context_)
    return true;
  return shared_->context_->IsBroken(shared_->uuid_);
}

void BlobDataHandle::RunOnConstructionComplete(
    const BlobConstructedCallback& done) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  if (!shared_->context_.get()) {
    done.Run(false, IPCBlobCreationCancelCode::UNKNOWN);
    return;
  }
  shared_->context_->RunOnConstructionComplete(shared_->uuid_, done);
}

std::unique_ptr<BlobDataSnapshot> BlobDataHandle::CreateSnapshot() const {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  if (!shared_->context_.get())
    return nullptr;
  return shared_->context_->CreateSnapshot(shared_->uuid_);
}

const std::string& BlobDataHandle::uuid() const {
  return shared_->uuid_;
}

const std::string& BlobDataHandle::content_type() const {
  return shared_->content_type_;
}

const std::string& BlobDataHandle::content_disposition() const {
  return shared_->content_disposition_;
}

}  // namespace storage
