// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_FILE_DESCRIPTOR_H_
#define DBUS_FILE_DESCRIPTOR_H_

#include <memory>

#include "base/macros.h"
#include "dbus/dbus_export.h"

namespace dbus {

// FileDescriptor is a type used to encapsulate D-Bus file descriptors
// and to follow the RAII idiom appropiate for use with message operations
// where the descriptor might be easily leaked.  To guard against this the
// descriptor is closed when an instance is destroyed if it is owned.
// Ownership is asserted only when PutValue is used and TakeValue can be
// used to take ownership.
//
// For example, in the following
//  FileDescriptor fd;
//  if (!reader->PopString(&name) ||
//      !reader->PopFileDescriptor(&fd) ||
//      !reader->PopUint32(&flags)) {
// the descriptor in fd will be closed if the PopUint32 fails.  But
//   writer.AppendFileDescriptor(dbus::FileDescriptor(1));
// will not automatically close "1" because it is not owned.
//
// Descriptors must be validated before marshalling in a D-Bus message
// or using them after unmarshalling.  We disallow descriptors to a
// directory to reduce the security risks.  Splitting out validation
// also allows the caller to do this work on the File thread to conform
// with i/o restrictions.
class CHROME_DBUS_EXPORT FileDescriptor {
 public:
  // This provides a simple way to pass around file descriptors since they must
  // be closed on a thread that is allowed to perform I/O.
  struct Deleter {
    void CHROME_DBUS_EXPORT operator()(FileDescriptor* fd);
  };

  // Permits initialization without a value for passing to
  // dbus::MessageReader::PopFileDescriptor to fill in and from int values.
  FileDescriptor() : value_(-1), owner_(false), valid_(false) {}
  explicit FileDescriptor(int value) : value_(value), owner_(false),
      valid_(false) {}

  FileDescriptor(FileDescriptor&& other);

  virtual ~FileDescriptor();

  FileDescriptor& operator=(FileDescriptor&& other);

  // Retrieves value as an int without affecting ownership.
  int value() const;

  // Retrieves whether or not the descriptor is ok to send/receive.
  int is_valid() const { return valid_; }

  // Sets the value and assign ownership.
  void PutValue(int value) {
    value_ = value;
    owner_ = true;
    valid_ = false;
  }

  // Takes the value and ownership.
  int TakeValue();

  // Checks (and records) validity of the file descriptor.
  // We disallow directories to avoid potential sandbox escapes.
  // Note this call must be made on a thread where file i/o is allowed.
  void CheckValidity();

 private:
  void Swap(FileDescriptor* other);

  int value_;
  bool owner_;
  bool valid_;

  DISALLOW_COPY_AND_ASSIGN(FileDescriptor);
};

using ScopedFileDescriptor =
    std::unique_ptr<FileDescriptor, FileDescriptor::Deleter>;

}  // namespace dbus

#endif  // DBUS_FILE_DESCRIPTOR_H_
