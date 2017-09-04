// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MemoryPressureListener provides static APIs for handling memory pressure on
// platforms that have such signals (Android, ChromeOS, Mac and Windows). When
// such signals are received the app will try to discard buffers that aren't
// deemed essential (individual modules will implement their own policy).

#ifndef COMPONENTS_MEMORY_PRESSURE_MEMORY_PRESSURE_LISTENER_H_
#define COMPONENTS_MEMORY_PRESSURE_MEMORY_PRESSURE_LISTENER_H_

#include "base/base_export.h"
#include "base/callback.h"
#include "base/macros.h"
#include "build/build_config.h"

// Other operating systems will be added to this as they are implemented.
#if defined(OS_WIN) || defined(OS_LINUX)
#define MEMORY_PRESSURE_IS_POLLING
#endif

namespace memory_pressure {

// To start listening, create a new instance, passing a callback to a
// function that takes a MemoryPressureLevel parameter. To stop listening,
// simply delete the listener object. The implementation guarantees
// that the callback will always be called on the thread that created
// the listener.
//
// Note that even on the same thread, the callback is not guaranteed to be
// called synchronously within the system memory pressure broadcast.
// Please see notes in MemoryPressureLevel enum below: some levels are
// absolutely critical, and if not enough memory is returned to the system,
// it'll potentially kill the app, and then later the app will have to be
// cold-started.
//
// Example:
//
//   void OnMemoryPressure(MemoryPressureLevel memory_pressure_level) {
//     ...
//   }
//
//   // Start listening.
//   MemoryPressureListener* my_listener =
//       new MemoryPressureListener(base::Bind(&OnMemoryPressure));
//
//   ...
//
//   // Stop listening.
//   delete my_listener;
//
class MemoryPressureListener {
 public:
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.base
  enum MemoryPressureLevel {
    // No problems, there is enough memory to use. This event is not sent via
    // callback, but the enum is used in other places to find out the current
    // state of the system.
    MEMORY_PRESSURE_LEVEL_NONE = -1,

    // Modules are advised to free buffers that are cheap to re-allocate and not
    // immediately needed.
    MEMORY_PRESSURE_LEVEL_MODERATE = 0,

    // At this level, modules are advised to free all possible memory.  The
    // alternative is to be killed by the system, which means all memory will
    // have to be re-created, plus the cost of a cold start.
    MEMORY_PRESSURE_LEVEL_CRITICAL = 2,
  };

  typedef base::Callback<void(MemoryPressureLevel)> MemoryPressureCallback;

  explicit MemoryPressureListener(
      const MemoryPressureCallback& memory_pressure_callback);
  ~MemoryPressureListener();

  // Intended for use by the platform specific implementation.
  static void NotifyMemoryPressure(MemoryPressureLevel memory_pressure_level);

 private:
  void Notify(MemoryPressureLevel memory_pressure_level);

  MemoryPressureCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(MemoryPressureListener);
};

}  // namespace memory_pressure

#endif  // COMPONENTS_MEMORY_PRESSURE_MEMORY_PRESSURE_LISTENER_H_
