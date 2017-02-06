// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/memory_pressure_controller_impl.h"

#include "base/bind.h"
#include "content/browser/memory/memory_message_filter.h"
#include "content/public/browser/browser_thread.h"

namespace content {

MemoryPressureControllerImpl::MemoryPressureControllerImpl() {}

MemoryPressureControllerImpl::~MemoryPressureControllerImpl() {}

void MemoryPressureControllerImpl::OnMemoryMessageFilterAdded(
    MemoryMessageFilter* filter) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Add the message filter to the set of all memory message filters and check
  // that it wasn't there beforehand.
  const bool success =
      memory_message_filters_.insert(
                                 std::make_pair(filter->process_host(), filter))
          .second;
  DCHECK(success);

  // There's no need to send a message to the child process if memory pressure
  // notifications are not suppressed.
  if (base::MemoryPressureListener::AreNotificationsSuppressed())
    filter->SendSetPressureNotificationsSuppressed(true);
}

void MemoryPressureControllerImpl::OnMemoryMessageFilterRemoved(
    MemoryMessageFilter* filter) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Remove the message filter from the set of all memory message filters,
  // ensuring that it was there beforehand.
  auto it = memory_message_filters_.find(filter->process_host());
  DCHECK(it != memory_message_filters_.end());
  DCHECK_EQ(filter, it->second);
  memory_message_filters_.erase(it);
}

// static
MemoryPressureControllerImpl* MemoryPressureControllerImpl::GetInstance() {
  return base::Singleton<
      MemoryPressureControllerImpl,
      base::LeakySingletonTraits<MemoryPressureControllerImpl>>::get();
}

void
MemoryPressureControllerImpl::SetPressureNotificationsSuppressedInAllProcesses(
    bool suppressed) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    // Note that passing base::Unretained(this) is safe here because the
    // controller is a leaky singleton.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&MemoryPressureControllerImpl::
                       SetPressureNotificationsSuppressedInAllProcesses,
                   base::Unretained(this), suppressed));
    return;
  }

  // Enable/disable suppressing memory notifications in the browser process.
  base::MemoryPressureListener::SetNotificationsSuppressed(suppressed);

  // Enable/disable suppressing memory notifications in all child processes.
  for (const auto& filter_pair : memory_message_filters_)
    filter_pair.second->SendSetPressureNotificationsSuppressed(suppressed);
}

void MemoryPressureControllerImpl::SimulatePressureNotificationInAllProcesses(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  DCHECK_NE(level, base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);

  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    // Note that passing base::Unretained(this) is safe here because the
    // controller is a leaky singleton.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&MemoryPressureControllerImpl::
                       SimulatePressureNotificationInAllProcesses,
                   base::Unretained(this), level));
    return;
  }

  // Simulate memory pressure notification in the browser process.
  base::MemoryPressureListener::SimulatePressureNotification(level);

  // Simulate memory pressure notification in all child processes.
  for (const auto& filter_pair : memory_message_filters_)
    filter_pair.second->SendSimulatePressureNotification(level);
}

void MemoryPressureControllerImpl::SendPressureNotification(
    const BrowserChildProcessHost* child_process_host,
    base::MemoryPressureListener::MemoryPressureLevel level) {
  SendPressureNotificationImpl(child_process_host, level);
}

void MemoryPressureControllerImpl::SendPressureNotification(
    const RenderProcessHost* render_process_host,
    base::MemoryPressureListener::MemoryPressureLevel level) {
  SendPressureNotificationImpl(render_process_host, level);
}

void MemoryPressureControllerImpl::SendPressureNotificationImpl(
    const void* child_process_host,
    base::MemoryPressureListener::MemoryPressureLevel level) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    // Note that passing base::Unretained(this) is safe here because the
    // controller is a leaky singleton. It's also safe to pass an untyped
    // child process pointer as the address is only used as a key for lookup in
    // a map; at no point is it dereferenced.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&MemoryPressureControllerImpl::SendPressureNotificationImpl,
                   base::Unretained(this), child_process_host, level));
    return;
  }

  if (base::MemoryPressureListener::AreNotificationsSuppressed())
    return;

  // Find the appropriate message filter and dispatch the message.
  auto it = memory_message_filters_.find(child_process_host);
  if (it != memory_message_filters_.end())
    it->second->SendPressureNotification(level);
}

}  // namespace content
