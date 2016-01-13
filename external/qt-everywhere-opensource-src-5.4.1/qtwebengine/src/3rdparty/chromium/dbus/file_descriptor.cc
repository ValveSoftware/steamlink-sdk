// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file.h"
#include "base/logging.h"
#include "dbus/file_descriptor.h"

namespace dbus {

FileDescriptor::~FileDescriptor() {
  if (owner_)
    base::File auto_closer(value_);
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
  base::File::Info info;
  bool ok = file.GetInfo(&info);
  file.TakePlatformFile();  // Prevent |value_| from being closed by |file|.
  valid_ = (ok && !info.is_directory);
}

}  // namespace dbus
