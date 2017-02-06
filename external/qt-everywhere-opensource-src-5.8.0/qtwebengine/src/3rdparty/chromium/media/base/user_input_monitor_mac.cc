// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/user_input_monitor.h"

#include <ApplicationServices/ApplicationServices.h>
#include <stddef.h>

#include "base/macros.h"
#include "base/memory/ptr_util.h"

namespace media {
namespace {

class UserInputMonitorMac : public UserInputMonitor {
 public:
  UserInputMonitorMac();
  ~UserInputMonitorMac() override;

  size_t GetKeyPressCount() const override;

 private:
  void StartKeyboardMonitoring() override;
  void StopKeyboardMonitoring() override;
  void StartMouseMonitoring() override;
  void StopMouseMonitoring() override;

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

std::unique_ptr<UserInputMonitor> UserInputMonitor::Create(
    const scoped_refptr<base::SingleThreadTaskRunner>& input_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner) {
  return base::WrapUnique(new UserInputMonitorMac);
}

}  // namespace media
