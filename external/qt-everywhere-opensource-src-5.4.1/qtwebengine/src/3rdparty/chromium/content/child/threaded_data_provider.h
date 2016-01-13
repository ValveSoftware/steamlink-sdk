// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_THREADEDDATAPROVIDER_IMPL_H_
#define CONTENT_CHILD_THREADEDDATAPROVIDER_IMPL_H_

#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "ipc/ipc_channel.h"
#include "ipc/message_filter.h"

namespace blink {
class WebThreadedDataReceiver;
}

namespace IPC {
class SyncChannel;
}

namespace webkit_glue {
class WebThreadImpl;
}

namespace content {
class ResourceDispatcher;
class WebThreadImpl;

class ThreadedDataProvider {
 public:
  ThreadedDataProvider(
      int request_id,
      blink::WebThreadedDataReceiver* threaded_data_receiver,
      linked_ptr<base::SharedMemory> shm_buffer,
      int shm_size);
  virtual ~ThreadedDataProvider();

  void Stop();
  void OnReceivedDataOnBackgroundThread(int data_offset,
                                        int data_length,
                                        int encoded_data_length);

  void OnReceivedDataOnForegroundThread(const char* data,
                                        int data_length,
                                        int encoded_data_length);

  void OnResourceMessageFilterAddedMainThread();

 private:
  void StopOnBackgroundThread();
  void OnResourceMessageFilterAddedBackgroundThread();
  void ForwardAndACKData(const char* data, int data_length);

  scoped_refptr<IPC::MessageFilter> filter_;
  int request_id_;
  linked_ptr<base::SharedMemory> shm_buffer_;
  int shm_size_;
  scoped_ptr<base::WeakPtrFactory<ThreadedDataProvider> >
      background_thread_weak_factory_;
  base::WeakPtrFactory<ThreadedDataProvider>
      main_thread_weak_factory_;
  WebThreadImpl& background_thread_;
  IPC::SyncChannel* ipc_channel_;
  blink::WebThreadedDataReceiver* threaded_data_receiver_;
  bool resource_filter_active_;
  base::MessageLoop* main_thread_message_loop_;

  struct QueuedSharedMemoryData {
    const char* data;
    int length;
  };
  std::vector<QueuedSharedMemoryData> queued_data_;

  DISALLOW_COPY_AND_ASSIGN(ThreadedDataProvider);
};

}  // namespace content

#endif  // CONTENT_CHILD_THREADEDDATAPROVIDER_IMPL_H_
