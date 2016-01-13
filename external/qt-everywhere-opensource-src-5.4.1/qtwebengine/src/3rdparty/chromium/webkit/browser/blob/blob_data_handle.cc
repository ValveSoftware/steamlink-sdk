// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/blob/blob_data_handle.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "webkit/browser/blob/blob_storage_context.h"
#include "webkit/common/blob/blob_data.h"

namespace webkit_blob {

BlobDataHandle::BlobDataHandleShared::BlobDataHandleShared(
    BlobData* blob_data,
    BlobStorageContext* context,
    base::SequencedTaskRunner* task_runner)
    : blob_data_(blob_data),
      context_(context->AsWeakPtr()) {
  context_->IncrementBlobRefCount(blob_data->uuid());
}

BlobData* BlobDataHandle::BlobDataHandleShared::data() const {
  return blob_data_;
}

const std::string& BlobDataHandle::BlobDataHandleShared::uuid() const {
  return blob_data_->uuid();
}

BlobDataHandle::BlobDataHandleShared::~BlobDataHandleShared() {
  if (context_.get())
    context_->DecrementBlobRefCount(blob_data_->uuid());
}

BlobDataHandle::BlobDataHandle(BlobData* blob_data,
                               BlobStorageContext* context,
                               base::SequencedTaskRunner* task_runner)
    : io_task_runner_(task_runner),
      shared_(new BlobDataHandleShared(blob_data, context, task_runner)) {
  DCHECK(io_task_runner_);
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
}

BlobDataHandle::BlobDataHandle(const BlobDataHandle& other) {
  io_task_runner_ = other.io_task_runner_;
  shared_ = other.shared_;
}

BlobDataHandle::~BlobDataHandle() {
  BlobDataHandleShared* raw = shared_.get();
  raw->AddRef();
  shared_ = 0;
  io_task_runner_->ReleaseSoon(FROM_HERE, raw);
}

BlobData* BlobDataHandle::data() const {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  return shared_->data();
}

std::string BlobDataHandle::uuid() const {
  return shared_->uuid();
}

}  // namespace webkit_blob
