// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MEMORY_PRESSURE_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_MEMORY_PRESSURE_CONTROLLER_H_

#include "base/memory/memory_pressure_listener.h"
#include "content/common/content_export.h"

namespace content {

class BrowserChildProcessHost;
class RenderProcessHost;

// Allows content embedders to forward memory pressure events from the main
// browser process to children processes. For more details see
// content/browser/memory/memory_pressure_controller_impl.h.
class CONTENT_EXPORT MemoryPressureController {
 public:

  // Sends a memory pressure notification to the specified browser child process
  // via its attached MemoryMessageFilter. May be called on any thread.
  static void SendPressureNotification(
      const BrowserChildProcessHost* child_process_host,
      base::MemoryPressureListener::MemoryPressureLevel level);

  // Sends a memory pressure notification to the specified renderer process via
  // its attached MemoryMessageFilter. May be called on any thread.
  static void SendPressureNotification(
      const RenderProcessHost* render_process_host,
      base::MemoryPressureListener::MemoryPressureLevel level);

 private:
  DISALLOW_COPY_AND_ASSIGN(MemoryPressureController);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MEMORY_PRESSURE_CONTROLLER_H_
