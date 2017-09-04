// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/child_histogram_message_filter.h"

#include <ctype.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_delta_serialization.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/single_thread_task_runner.h"
#include "content/child/child_process.h"
#include "content/common/child_process_messages.h"
#include "ipc/ipc_sender.h"

namespace content {

ChildHistogramMessageFilter::ChildHistogramMessageFilter()
    : sender_(NULL),
      io_task_runner_(ChildProcess::current()->io_task_runner()) {
}

ChildHistogramMessageFilter::~ChildHistogramMessageFilter() {
}

void ChildHistogramMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  sender_ = channel;
}

void ChildHistogramMessageFilter::OnFilterRemoved() {
}

bool ChildHistogramMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChildHistogramMessageFilter, message)
    IPC_MESSAGE_HANDLER(ChildProcessMsg_SetHistogramMemory,
                        OnSetHistogramMemory)
    IPC_MESSAGE_HANDLER(ChildProcessMsg_GetChildNonPersistentHistogramData,
                        OnGetChildHistogramData)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ChildHistogramMessageFilter::OnSetHistogramMemory(
    const base::SharedMemoryHandle& memory_handle,
    int memory_size) {
  // This message must be received only once. Multiple calls to create a global
  // allocator will cause a CHECK() failure.
  base::GlobalHistogramAllocator::CreateWithSharedMemoryHandle(memory_handle,
                                                               memory_size);

  base::PersistentHistogramAllocator* global_allocator =
      base::GlobalHistogramAllocator::Get();
  if (global_allocator)
    global_allocator->CreateTrackingHistograms(global_allocator->Name());
}

void ChildHistogramMessageFilter::OnGetChildHistogramData(int sequence_number) {
  UploadAllHistograms(sequence_number);
}

void ChildHistogramMessageFilter::UploadAllHistograms(int sequence_number) {
  // If a persistent allocator is in use, it needs to occasionally update
  // some internal histograms. An upload is happening so this is a good time.
  base::PersistentHistogramAllocator* global_allocator =
      base::GlobalHistogramAllocator::Get();
  if (global_allocator)
    global_allocator->UpdateTrackingHistograms();

  if (!histogram_delta_serialization_) {
    histogram_delta_serialization_.reset(
        new base::HistogramDeltaSerialization("ChildProcess"));
  }

  std::vector<std::string> deltas;
  // "false" to PerpareAndSerializeDeltas() indicates to *not* include
  // histograms held in persistent storage on the assumption that they will be
  // visible to the recipient through other means.
  histogram_delta_serialization_->PrepareAndSerializeDeltas(&deltas, false);
  sender_->Send(
      new ChildProcessHostMsg_ChildHistogramData(sequence_number, deltas));

#ifndef NDEBUG
  static int count = 0;
  count++;
  LOCAL_HISTOGRAM_COUNTS("Histogram.ChildProcessHistogramSentCount", count);
#endif
}

}  // namespace content
