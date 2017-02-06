// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_CHILD_HISTOGRAM_MESSAGE_FILTER_H_
#define CONTENT_CHILD_CHILD_HISTOGRAM_MESSAGE_FILTER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "ipc/message_filter.h"

namespace base {
class HistogramDeltaSerialization;
class SingleThreadTaskRunner;
}  // namespace base

namespace content {

class ChildHistogramMessageFilter : public IPC::MessageFilter {
 public:
  ChildHistogramMessageFilter();

  // IPC::MessageFilter implementation.
  void OnFilterAdded(IPC::Sender* sender) override;
  void OnFilterRemoved() override;
  bool OnMessageReceived(const IPC::Message& message) override;

  void SendHistograms(int sequence_number);

 private:
  typedef std::vector<std::string> HistogramPickledList;

  ~ChildHistogramMessageFilter() override;

  // Message handlers.
  void OnSetHistogramMemory(const base::SharedMemoryHandle& memory_handle,
                            int memory_size);
  void OnGetChildHistogramData(int sequence_number);

  // Extract snapshot data and then send it off to the Browser process.
  // Send only a delta to what we have already sent.
  void UploadAllHistograms(int sequence_number);

  IPC::Sender* sender_;

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Prepares histogram deltas for transmission.
  std::unique_ptr<base::HistogramDeltaSerialization>
      histogram_delta_serialization_;

  DISALLOW_COPY_AND_ASSIGN(ChildHistogramMessageFilter);
};

}  // namespace content

#endif  // CONTENT_CHILD_CHILD_HISTOGRAM_MESSAGE_FILTER_H_
