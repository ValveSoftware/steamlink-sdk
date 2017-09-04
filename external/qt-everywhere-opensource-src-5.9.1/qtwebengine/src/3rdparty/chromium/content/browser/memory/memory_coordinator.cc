// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/memory_coordinator.h"

#include "base/memory/memory_coordinator_client_registry.h"
#include "base/metrics/histogram_macros.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"

namespace content {

// The implementation of MemoryCoordinatorHandle. See memory_coordinator.mojom
// for the role of this class.
class MemoryCoordinatorHandleImpl : public mojom::MemoryCoordinatorHandle {
 public:
  MemoryCoordinatorHandleImpl(mojom::MemoryCoordinatorHandleRequest request,
                              MemoryCoordinator* coordinator,
                              int render_process_id);
  ~MemoryCoordinatorHandleImpl() override;

  // mojom::MemoryCoordinatorHandle:
  void AddChild(mojom::ChildMemoryCoordinatorPtr child) override;

  mojom::ChildMemoryCoordinatorPtr& child() { return child_; }
  mojo::Binding<mojom::MemoryCoordinatorHandle>& binding() { return binding_; }

 private:
  MemoryCoordinator* coordinator_;
  int render_process_id_;
  mojom::ChildMemoryCoordinatorPtr child_;
  mojo::Binding<mojom::MemoryCoordinatorHandle> binding_;

  DISALLOW_COPY_AND_ASSIGN(MemoryCoordinatorHandleImpl);
};

MemoryCoordinatorHandleImpl::MemoryCoordinatorHandleImpl(
    mojom::MemoryCoordinatorHandleRequest request,
    MemoryCoordinator* coordinator,
    int render_process_id)
    : coordinator_(coordinator),
      render_process_id_(render_process_id),
      binding_(this, std::move(request)) {
  DCHECK(coordinator_);
}

MemoryCoordinatorHandleImpl::~MemoryCoordinatorHandleImpl() {}

void MemoryCoordinatorHandleImpl::AddChild(
    mojom::ChildMemoryCoordinatorPtr child) {
  DCHECK(!child_.is_bound());
  child_ = std::move(child);
  coordinator_->OnChildAdded(render_process_id_);
}

MemoryCoordinator::MemoryCoordinator()
    : delegate_(GetContentClient()->browser()->GetMemoryCoordinatorDelegate()) {
}

MemoryCoordinator::~MemoryCoordinator() {}

void MemoryCoordinator::CreateHandle(
    int render_process_id,
    mojom::MemoryCoordinatorHandleRequest request) {
  std::unique_ptr<MemoryCoordinatorHandleImpl> handle(
      new MemoryCoordinatorHandleImpl(std::move(request), this,
                                      render_process_id));
  handle->binding().set_connection_error_handler(
      base::Bind(&MemoryCoordinator::OnConnectionError, base::Unretained(this),
                 render_process_id));
  CreateChildInfoMapEntry(render_process_id, std::move(handle));
}

size_t MemoryCoordinator::NumChildrenForTesting() {
  return children_.size();
}

bool MemoryCoordinator::SetChildMemoryState(int render_process_id,
                                            mojom::MemoryState memory_state) {
  // Can't set an invalid memory state.
  if (memory_state == mojom::MemoryState::UNKNOWN)
    return false;

  // Can't send a message to a child that doesn't exist.
  auto iter = children_.find(render_process_id);
  if (iter == children_.end())
    return false;

  // Can't send a message to a child that isn't bound.
  if (!iter->second.handle->child().is_bound())
    return false;

  // A nop doesn't need to be sent, but is considered successful.
  if (iter->second.memory_state == memory_state)
    return true;

  // Can't throttle the given renderer.
  if (memory_state == mojom::MemoryState::THROTTLED &&
      !CanThrottleRenderer(render_process_id))
    return false;

  // Can't suspend the given renderer.
  if (memory_state == mojom::MemoryState::SUSPENDED &&
      !CanSuspendRenderer(render_process_id))
    return false;

  // Update the internal state and send the message.
  iter->second.memory_state = memory_state;
  iter->second.handle->child()->OnStateChange(memory_state);
  return true;
}

mojom::MemoryState MemoryCoordinator::GetChildMemoryState(
    int render_process_id) const {
  auto iter = children_.find(render_process_id);
  if (iter == children_.end())
    return mojom::MemoryState::UNKNOWN;
  return iter->second.memory_state;
}

void MemoryCoordinator::RecordMemoryPressure(
    base::MemoryPressureMonitor::MemoryPressureLevel level) {
  int state = static_cast<int>(GetCurrentMemoryState());
  switch (level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      UMA_HISTOGRAM_ENUMERATION(
          "Memory.Coordinator.StateOnModerateNotificationReceived",
          state, base::kMemoryStateMax);
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      UMA_HISTOGRAM_ENUMERATION(
          "Memory.Coordinator.StateOnCriticalNotificationReceived",
          state, base::kMemoryStateMax);
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      NOTREACHED();
  }
}

base::MemoryState MemoryCoordinator::GetCurrentMemoryState() const {
  return base::MemoryState::UNKNOWN;
}

void MemoryCoordinator::SetCurrentMemoryStateForTesting(
    base::MemoryState memory_state) {
}

void MemoryCoordinator::AddChildForTesting(
    int dummy_render_process_id, mojom::ChildMemoryCoordinatorPtr child) {
  mojom::MemoryCoordinatorHandlePtr mch;
  auto request = mojo::GetProxy(&mch);
  std::unique_ptr<MemoryCoordinatorHandleImpl> handle(
      new MemoryCoordinatorHandleImpl(std::move(request), this,
                                      dummy_render_process_id));
  handle->AddChild(std::move(child));
  CreateChildInfoMapEntry(dummy_render_process_id, std::move(handle));
}

void MemoryCoordinator::OnConnectionError(int render_process_id) {
  children_.erase(render_process_id);
}

bool MemoryCoordinator::CanThrottleRenderer(int render_process_id) {
  // If there is no delegate (i.e. tests), renderers are always throttleable.
  // TODO(bashi): We check |delegate_| to avoid calling FromID() on a
  // wrong thread in tests. Figure out a better way to handle tests.
  if (!delegate_)
    return true;
  auto* render_process_host = RenderProcessHost::FromID(render_process_id);
  return render_process_host->IsProcessBackgrounded();
}

bool MemoryCoordinator::CanSuspendRenderer(int render_process_id) {
  // If there is no delegate (i.e. tests), renderers are always suspendable.
  if (!delegate_)
    return true;
  auto* render_process_host = RenderProcessHost::FromID(render_process_id);
  if (!render_process_host->IsProcessBackgrounded())
      return false;
  return delegate_->CanSuspendBackgroundedRenderer(render_process_id);
}

void MemoryCoordinator::CreateChildInfoMapEntry(
    int render_process_id,
    std::unique_ptr<MemoryCoordinatorHandleImpl> handle) {
  auto& child_info = children_[render_process_id];
  // Process always start with normal memory state.
  // TODO(chrisha): Consider having memory state be a startup parameter of
  // child processes, allowing them to be launched in a restricted state.
  child_info.memory_state = mojom::MemoryState::NORMAL;
  child_info.handle = std::move(handle);
}

MemoryCoordinator::ChildInfo::ChildInfo() {}

MemoryCoordinator::ChildInfo::ChildInfo(const ChildInfo& rhs) {
  // This is a nop, but exists for compatibility with STL containers.
}

MemoryCoordinator::ChildInfo::~ChildInfo() {}

}  // namespace content
