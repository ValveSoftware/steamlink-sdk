// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_URL_RESPONSE_BODY_CONSUMER_H_
#define CONTENT_CHILD_URL_RESPONSE_BODY_CONSUMER_H_

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "content/common/content_export.h"
#include "content/common/url_loader.mojom.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/watcher.h"

namespace content {

class ResourceDispatcher;
struct ResourceRequestCompletionStatus;

// This class pulls data from a data pipe and dispatches it to the
// ResourceDispatcher. This class is used only for mojo-enabled requests.
class CONTENT_EXPORT URLResponseBodyConsumer final
    : public base::RefCounted<URLResponseBodyConsumer>,
      public base::SupportsWeakPtr<URLResponseBodyConsumer> {
 public:
  URLResponseBodyConsumer(
      int request_id,
      ResourceDispatcher* resource_dispatcher,
      mojo::ScopedDataPipeConsumerHandle handle,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Starts watching the handle.
  void Start(base::SingleThreadTaskRunner* task_runner);

  // Sets the completion status. The completion status is dispatched to the
  // ResourceDispatcher when the both following conditions hold:
  //  1) This function has been called and the completion status is set, and
  //  2) All data is read from the handle.
  void OnComplete(const ResourceRequestCompletionStatus& status);

  // Cancels watching the handle and dispatches an error to the
  // ResourceDispatcher. This function does nothing if the reading is already
  // cancelled or done.
  void Cancel();

 private:
  friend class base::RefCounted<URLResponseBodyConsumer>;
  ~URLResponseBodyConsumer();

  class ReceivedData;
  void Reclaim(uint32_t size);

  void OnReadable(MojoResult unused);
  void NotifyCompletionIfAppropriate();

  const int request_id_;
  ResourceDispatcher* resource_dispatcher_;
  mojo::ScopedDataPipeConsumerHandle handle_;
  mojo::Watcher handle_watcher_;
  ResourceRequestCompletionStatus completion_status_;

  bool has_received_completion_ = false;
  bool has_been_cancelled_ = false;
  bool has_seen_end_of_data_;

  DISALLOW_COPY_AND_ASSIGN(URLResponseBodyConsumer);
};

}  // namespace content

#endif  // CONTENT_CHILD_URL_RESPONSE_BODY_CONSUMER_H_
