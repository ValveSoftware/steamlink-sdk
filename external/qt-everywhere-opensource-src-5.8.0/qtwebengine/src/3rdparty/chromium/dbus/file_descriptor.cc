// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/worker_pool.h"
#include "dbus/file_descriptor.h"

using std::swap;

namespace dbus {

void CHROME_DBUS_EXPORT FileDescriptor::Deleter::operator()(
    FileDescriptor* fd) {
  base::WorkerPool::PostTask(
      FROM_HERE, base::Bind(&base::DeletePointer<FileDescriptor>, fd), false);
}

FileDescriptor::FileDescriptor(FileDescriptor&& other) : FileDescriptor() {
  Swap(&other);
}

FileDescriptor::~FileDescriptor() {
  if (owner_)
    base::File auto_closer(value_);
}

FileDescriptor& FileDescriptor::operator=(FileDescriptor&& other) {
  Swap(&other);
  return *this;
}

int FileDescriptor::value() const {
  CHECK(valid_);
  return value_;
}

int FileDescriptor::TakeValue() {
  CHECK(valid_);  // NB: check first so owner_ is unchanged if this triggers
  owner_ = false;
  return value_;
}

void FileDescriptor::CheckValidity() {
  base::File file(value_);
  if (!file.IsValid()) {
    valid_ = false;
    return;
  }

  base::File::Info info;
  bool ok = file.GetInfo(&info);
  file.TakePlatformFile();  // Prevent |value_| from being closed by |file|.
  valid_ = (ok && !info.is_directory);
}

void FileDescriptor::Swap(FileDescriptor* other) {
  swap(value_, other->value_);
  swap(owner_, other->owner_);
  swap(valid_, other->valid_);
}

}  // namespace dbus
