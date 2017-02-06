// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEMORY_MEMORY_PRESSURE_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_MEMORY_MEMORY_PRESSURE_CONTROLLER_IMPL_H_

#include <map>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/singleton.h"
#include "content/common/content_export.h"

namespace content {

class BrowserChildProcessHost;
class MemoryMessageFilter;
class RenderProcessHost;

// Controller for memory pressure IPC messages. Each child process owns and
// registers a MemoryMessageFilter, which can be used to both suppress and
// simulate memory pressure messages across processes. This controller
// coordinates suppressing and simulation of messages, as well as allows for
// messages to be forwarded to individual processes. This allows the browser
// process to control what memory pressure messages are seen in child processes.
// For more details see content/browser/memory/memory_message_filter.h and
// content/child/memory/child_memory_message_filter.h.
class CONTENT_EXPORT MemoryPressureControllerImpl {
 public:
  // This method can be called from any thread.
  static MemoryPressureControllerImpl* GetInstance();

  // These methods must be called on the IO thread.
  void OnMemoryMessageFilterAdded(MemoryMessageFilter* filter);
  void OnMemoryMessageFilterRemoved(MemoryMessageFilter* filter);

  // These methods can be called from any thread.

  // Suppresses all memory pressure messages from passing through all attached
  // MemoryMessageFilters. Any messages sent through a "suppressed" filter will
  // be ignored on the receiving end.
  void SetPressureNotificationsSuppressedInAllProcesses(bool suppressed);

  // Simulates memory pressure in all processes by invoking
  // SimulatePressureNotification on all attached MemoryMessageFilters. These
  // messages will be received even if suppression is enabled.
  void SimulatePressureNotificationInAllProcesses(
      base::MemoryPressureListener::MemoryPressureLevel level);

  // Sends a memory pressure notification to the specified browser child process
  // via its attached MemoryMessageFilter.
  void SendPressureNotification(
      const BrowserChildProcessHost* child_process_host,
      base::MemoryPressureListener::MemoryPressureLevel level);

  // Sends a memory pressure notification to the specified renderer process via
  // its attached MemoryMessageFilter.
  void SendPressureNotification(
      const RenderProcessHost* render_process_host,
      base::MemoryPressureListener::MemoryPressureLevel level);

 protected:
  virtual ~MemoryPressureControllerImpl();

 private:
  friend struct base::DefaultSingletonTraits<MemoryPressureControllerImpl>;

  MemoryPressureControllerImpl();

  // Implementation of the various SendPressureNotification methods.
  void SendPressureNotificationImpl(
      const void* child_process_host,
      base::MemoryPressureListener::MemoryPressureLevel level);

  // Map from untyped process host pointers to the associated memory message
  // filters in the browser process. Always accessed on the IO thread.
  typedef std::map<const void*, scoped_refptr<MemoryMessageFilter>>
      MemoryMessageFilterMap;
  MemoryMessageFilterMap memory_message_filters_;

  DISALLOW_COPY_AND_ASSIGN(MemoryPressureControllerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEMORY_MEMORY_PRESSURE_CONTROLLER_IMPL_H_
