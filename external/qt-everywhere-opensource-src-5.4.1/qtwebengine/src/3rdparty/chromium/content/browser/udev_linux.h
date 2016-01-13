// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// UdevLinux listens for device change notifications from udev and runs
// callbacks when notifications occur.
//
// UdevLinux must be created on a MessageLoop of TYPE_IO.
// UdevLinux is not thread-safe.
//
// Example usage:
//
// class UdevLinux;
//
// class Foo {
//  public:
//   Foo() {
//     std::vector<UdevLinux::UdevMonitorFilter> filters;
//     filters.push_back(content::UdevLinux::UdevMonitorFilter("block", NULL));
//     udev_.reset(new UdevLinux(filters,
//                               base::Bind(&Foo::Notify, this)));
//   }
//
//   // Called when a "block" device attaches/detaches.
//   // To hold on to |device|, call udev_device_ref(device).
//   void Notify(udev_device* device) {
//     // Do something with |device|.
//   }
//
//  private:
//   scoped_ptr<UdevLinux> udev_;
//
//   DISALLOW_COPY_AND_ASSIGN(Foo);
// };

#ifndef CONTENT_BROWSER_UDEV_LINUX_H_
#define CONTENT_BROWSER_UDEV_LINUX_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_pump_libevent.h"

extern "C" {
struct udev;
struct udev_device;
struct udev_monitor;
}

namespace content {

class UdevLinux : public base::MessagePumpLibevent::Watcher {
 public:
  typedef base::Callback<void(udev_device*)> UdevNotificationCallback;

  // subsystem and devtype parameter for
  // udev_monitor_filter_add_match_subsystem_devtype().
  struct UdevMonitorFilter {
    UdevMonitorFilter(const char* subsystem_in, const char* devtype_in)
        : subsystem(subsystem_in),
          devtype(devtype_in) {
    }
    const char* subsystem;
    const char* devtype;
  };

  // Filter incoming devices based on |filters|.
  // Calls |callback| upon device change events.
  UdevLinux(const std::vector<UdevMonitorFilter>& filters,
            const UdevNotificationCallback& callback);
  virtual ~UdevLinux();


  // Returns the udev handle to be passed into other udev_*() functions.
  udev* udev_handle();

 private:
  // base::MessagePump:Libevent::Watcher implementation.
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE;

  // libudev-related items, the main context, and the monitoring context to be
  // notified about changes to device states.
  udev* udev_;
  udev_monitor* monitor_;
  int monitor_fd_;
  base::MessagePumpLibevent::FileDescriptorWatcher monitor_watcher_;
  UdevNotificationCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(UdevLinux);
};

}  // namespace content

#endif  // CONTENT_BROWSER_UDEV_LINUX_H_
