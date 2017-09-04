// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEMORY_MEMORY_COORDINATOR_H_
#define CONTENT_BROWSER_MEMORY_MEMORY_COORDINATOR_H_

#include "base/memory/memory_coordinator_client_registry.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/process/process_handle.h"
#include "content/common/content_export.h"
#include "content/common/memory_coordinator.mojom.h"
#include "content/public/browser/memory_coordinator_delegate.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {

// NOTE: Memory coordinator is under development and not fully working.
// TODO(bashi): Add more explanations when we implement memory coordinator V0.

class MemoryCoordinatorHandleImpl;

// MemoryCoordinator is responsible for the whole memory management accross the
// browser and child proceeses. It dispatches memory events to its clients and
// child processes based on its best knowledge of the memory usage.
class CONTENT_EXPORT MemoryCoordinator {
 public:
  virtual ~MemoryCoordinator();

  // Singleton factory/accessor.
  static MemoryCoordinator* GetInstance();

  // Starts monitoring memory usage. After calling this method, memory
  // coordinator will start dispatching state changes.
  virtual void Start() {}

  // Creates a handle to the provided child process.
  void CreateHandle(int render_process_id,
                    mojom::MemoryCoordinatorHandleRequest request);

  // Returns number of children. Only used for testing.
  size_t NumChildrenForTesting();

  // Dispatches a memory state change to the provided process. Returns true if
  // the process is tracked by this coordinator and successfully dispatches,
  // returns false otherwise.
  bool SetChildMemoryState(
      int render_process_id, mojom::MemoryState memory_state);

  // Returns the memory state of the specified render process. Returns UNKNOWN
  // if the process is not tracked by this coordinator.
  mojom::MemoryState GetChildMemoryState(int render_process_id) const;

  // Records memory pressure notifications. Called by MemoryPressureMonitor.
  // TODO(bashi): Remove this when MemoryPressureMonitor is retired.
  void RecordMemoryPressure(
      base::MemoryPressureMonitor::MemoryPressureLevel level);

  // Called when ChildMemoryCoordinator calls AddChild().
  virtual void OnChildAdded(int render_process_id) {}

  virtual base::MemoryState GetCurrentMemoryState() const;
  virtual void SetCurrentMemoryStateForTesting(base::MemoryState memory_state);

 protected:
  // Constructor. Protected as this is a singleton, but accessible for
  // unittests.
  MemoryCoordinator();

  // Adds the given ChildMemoryCoordinator as a child of this coordinator.
  void AddChildForTesting(int dummy_render_process_id,
                          mojom::ChildMemoryCoordinatorPtr child);

  // Callback invoked by mojo when the child connection goes down. Exposed
  // for testing.
  void OnConnectionError(int render_process_id);

  // Returns true when a given renderer can be throttled.
  bool CanThrottleRenderer(int render_process_id);

  // Returns true when a given renderer can be suspended.
  bool CanSuspendRenderer(int render_process_id);

  // Stores information about any known child processes.
  struct ChildInfo {
    // This object must be compatible with STL containers.
    ChildInfo();
    ChildInfo(const ChildInfo& rhs);
    ~ChildInfo();

    mojom::MemoryState memory_state;
    std::unique_ptr<MemoryCoordinatorHandleImpl> handle;
  };

  // A map from process ID (RenderProcessHost::GetID()) to child process info.
  using ChildInfoMap = std::map<int, ChildInfo>;

  ChildInfoMap& children() { return children_; }

private:
  // Helper function of CreateHandle and AddChildForTesting.
  void CreateChildInfoMapEntry(
      int render_process_id,
      std::unique_ptr<MemoryCoordinatorHandleImpl> handle);

  // Tracks child processes. An entry is added when a renderer connects to
  // MemoryCoordinator and removed automatically when an underlying binding is
  // disconnected.
  ChildInfoMap children_;

  std::unique_ptr<MemoryCoordinatorDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(MemoryCoordinator);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEMORY_MEMORY_COORDINATOR_H_
