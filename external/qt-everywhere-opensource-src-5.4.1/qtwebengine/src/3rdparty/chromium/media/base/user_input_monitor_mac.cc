// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/user_input_monitor.h"

#include <ApplicationServices/ApplicationServices.h>

namespace media {
namespace {

class UserInputMonitorMac : public UserInputMonitor {
 public:
  UserInputMonitorMac();
  virtual ~UserInputMonitorMac();

  virtual size_t GetKeyPressCount() const OVERRIDE;

 private:
  virtual void StartKeyboardMonitoring() OVERRIDE;
  virtual void StopKeyboardMonitoring() OVERRIDE;
  virtual void StartMouseMonitoring() OVERRIDE;
  virtual void StopMouseMonitoring() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(UserInputMonitorMac);
};

UserInputMonitorMac::UserInputMonitorMac() {}

UserInputMonitorMac::~UserInputMonitorMac() {}

size_t UserInputMonitorMac::GetKeyPressCount() const {
  // Use |kCGEventSourceStateHIDSystemState| since we only want to count
  // hardware generated events.
  return CGEventSourceCounterForEventType(kCGEventSourceStateHIDSystemState,
                                          kCGEventKeyDown);
}

void UserInputMonitorMac::StartKeyboardMonitoring() {}

void UserInputMonitorMac::StopKeyboardMonitoring() {}

// TODO(jiayl): add the impl.
void UserInputMonitorMac::StartMouseMonitoring() { NOTIMPLEMENTED(); }

// TODO(jiayl): add the impl.
void UserInputMonitorMac::StopMouseMonitoring() { NOTIMPLEMENTED(); }

}  // namespace

scoped_ptr<UserInputMonitor> UserInputMonitor::Create(
    const scoped_refptr<base::SingleThreadTaskRunner>& input_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner) {
  return scoped_ptr<UserInputMonitor>(new UserInputMonitorMac());
}

}  // namespace media
