// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_SCOPED_DBUS_ERROR_H_
#define DBUS_SCOPED_DBUS_ERROR_H_

#include <dbus/dbus.h>

namespace dbus {

// Utility class to ensure that DBusError is freed.
class ScopedDBusError {
 public:
  ScopedDBusError() {
    dbus_error_init(&error_);
  }

  ~ScopedDBusError() {
    dbus_error_free(&error_);
  }

  DBusError* get() { return &error_; }
  bool is_set() const { return dbus_error_is_set(&error_); }
  const char* name() { return error_.name; }
  const char* message() { return error_.message; }

 private:
  DBusError error_;
};

}  // namespace dbus

#endif  // DBUS_SCOPED_DBUS_ERROR_H_
