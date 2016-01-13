// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/threaded_data_provider.h"

#include "content/child/child_process.h"
#include "content/child/child_thread.h"
#include "content/child/resource_dispatcher.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/webthread_impl.h"
#include "content/common/resource_messages.h"
#include "ipc/ipc_sync_channel.h"
#include "third_party/WebKit/public/platform/WebThread.h"
#include "third_party/WebKit/public/platform/WebThreadedDataReceiver.h"

namespace content {

namespace {

class DataProviderMessageFilter : public IPC::MessageFilter {
 public:
  DataProviderMessageFilter(
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop,
      base::MessageLoop* main_thread_message_loop,
      const WebThreadImpl& background_thread,
      const base::WeakPtr<ThreadedDataProvider>&
          background_thread_resource_provider,
      const base::WeakPtr<ThreadedDataProvider>&
          main_thread_resource_provider,
      int request_id);

  // IPC::ChannelProxy::MessageFilter
  virtual void OnFilterAdded(IPC::Sender* sender) OVERRIDE FINAL;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE FINAL;

 private:
  virtual ~DataProviderMessageFilter() { }

  void OnReceivedData(int request_id, int data_offset, int data_length,
                      int encoded_data_length);

  const scoped_refptr<base::MessageLoopProxy> io_message_loop_;
  base::MessageLoop* main_thread_message_loop_;
  const WebThreadImpl& background_thread_;
  // This weakptr can only be dereferenced on the background thread.
  base::WeakPtr<ThreadedDataProvider>
      background_thread_resource_provider_;
  // This weakptr can only be dereferenced on the main thread.
  base::WeakPtr<ThreadedDataProvider>
      main_thread_resource_provider_;
  int request_id_;
};

DataProviderMessageFilter::DataProviderMessageFilter(
    const scoped_refptr<base::MessageLoopProxy>& io_message_loop,
    base::MessageLoop* main_thread_message_loop,
    const WebThreadImpl& background_thread,
    const base::WeakPtr<ThreadedDataProvider>&
        background_thread_resource_provider,
    const base::WeakPtr<ThreadedDataProvider>&
        main_thread_resource_provider,
    int request_id)
    : io_message_loop_(io_message_loop),
      main_thread_message_loop_(main_thread_message_loop),
      background_thread_(background_thread),
      background_thread_resource_provider_(background_thread_resource_provider),
      main_thread_resource_provider_(main_thread_resource_provider),
      request_id_(request_id) {
  DCHECK(main_thread_message_loop != NULL);
}

void DataProviderMessageFilter::OnFilterAdded(IPC::Sender* sender) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());

  main_thread_message_loop_->PostTask(FROM_HERE,
      base::Bind(
          &ThreadedDataProvider::OnResourceMessageFilterAddedMainThread,
          main_thread_resource_provider_));
}

bool DataProviderMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());

  if (message.type() != ResourceMsg_DataReceived::ID)
    return false;

  int request_id;

  PickleIterator iter(message);
  if (!message.ReadInt(&iter, &request_id)) {
    NOTREACHED() << "malformed resource message";
    return true;
  }

  if (request_id == request_id_) {
    ResourceMsg_DataReceived::Schema::Param arg;
    if (ResourceMsg_DataReceived::Read(&message, &arg)) {
      OnReceivedData(arg.a, arg.b, arg.c, arg.d);
      return true;
    }
  }

  return false;
}

void DataProviderMessageFilter::OnReceivedData(int request_id,
                                               int data_offset,
                                               int data_length,
                                               int encoded_data_length) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  background_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &ThreadedDataProvider::OnReceivedDataOnBackgroundThread,
      background_thread_resource_provider_,
      data_offset, data_length, encoded_data_length));
}

}  // anonymous namespace

ThreadedDataProvider::ThreadedDataProvider(
    int request_id, blink::WebThreadedDataReceiver* threaded_data_receiver,
    linked_ptr<base::SharedMemory> shm_buffer, int shm_size)
    : request_id_(request_id),
      shm_buffer_(shm_buffer),
      shm_size_(shm_size),
      main_thread_weak_factory_(this),
      background_thread_(
          static_cast<WebThreadImpl&>(
              *threaded_data_receiver->backgroundThread())),
      ipc_channel_(ChildThread::current()->channel()),
      threaded_data_receiver_(threaded_data_receiver),
      resource_filter_active_(false),
      main_thread_message_loop_(ChildThread::current()->message_loop()) {
  DCHECK(ChildThread::current());
  DCHECK(ipc_channel_);
  DCHECK(threaded_data_receiver_);
  DCHECK(main_thread_message_loop_);

  background_thread_weak_factory_.reset(
      new base::WeakPtrFactory<ThreadedDataProvider>(this));

  filter_ = new DataProviderMessageFilter(
      ChildProcess::current()->io_message_loop_proxy(),
      main_thread_message_loop_,
      background_thread_,
      background_thread_weak_factory_->GetWeakPtr(),
      main_thread_weak_factory_.GetWeakPtr(),
      request_id);

  ChildThread::current()->channel()->AddFilter(filter_);
}

ThreadedDataProvider::~ThreadedDataProvider() {
  DCHECK(ChildThread::current());

  ChildThread::current()->channel()->RemoveFilter(filter_);

  delete threaded_data_receiver_;
}

void DestructOnMainThread(ThreadedDataProvider* data_provider) {
  DCHECK(ChildThread::current());

  // The ThreadedDataProvider must be destructed on the main thread to
  // be threadsafe when removing the message filter and releasing the shared
  // memory buffer.
  delete data_provider;
}

void ThreadedDataProvider::Stop() {
  DCHECK(ChildThread::current());

  // Make sure we don't get called by on the main thread anymore via weak
  // pointers we've passed to the filter.
  main_thread_weak_factory_.InvalidateWeakPtrs();

  blink::WebThread* current_background_thread =
      threaded_data_receiver_->backgroundThread();

  // We can't destroy this instance directly; we need to bounce a message over
  // to the background thread and back to make sure nothing else will access it
  // there, before we can destruct it. We also need to make sure the background
  // thread is still alive, since Blink could have shut down at this point
  // and freed the thread.
  if (current_background_thread) {
    // We should never end up with a different parser thread than from when the
    // ThreadedDataProvider gets created.
    DCHECK(current_background_thread ==
        static_cast<WebThreadImpl*>(&background_thread_));
    background_thread_.message_loop()->PostTask(FROM_HERE,
        base::Bind(&ThreadedDataProvider::StopOnBackgroundThread,
                   base::Unretained(this)));
  }
}

void ThreadedDataProvider::StopOnBackgroundThread() {
  DCHECK(background_thread_.isCurrentThread());
  DCHECK(background_thread_weak_factory_);

  // When this happens, the provider should no longer be called on the
  // background thread as it's about to be destroyed on the main thread.
  // Destructing the weak pointer factory means invalidating the weak pointers
  // which means no callbacks from the filter will happen and nothing else will
  // use this instance on the background thread.
  background_thread_weak_factory_.reset(NULL);
  main_thread_message_loop_->PostTask(FROM_HERE,
      base::Bind(&DestructOnMainThread, this));
}

void ThreadedDataProvider::OnResourceMessageFilterAddedMainThread() {
  DCHECK(ChildThread::current());
  DCHECK(background_thread_weak_factory_);

  // We bounce this message from the I/O thread via the main thread and then
  // to our background thread, following the same path as incoming data before
  // our filter gets added, to make sure there's nothing still incoming.
  background_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(
          &ThreadedDataProvider::OnResourceMessageFilterAddedBackgroundThread,
          background_thread_weak_factory_->GetWeakPtr()));
}

void ThreadedDataProvider::OnResourceMessageFilterAddedBackgroundThread() {
  DCHECK(background_thread_.isCurrentThread());
  resource_filter_active_ = true;

  // At this point we know no more data is going to arrive from the main thread,
  // so we can process any data we've received directly from the I/O thread
  // in the meantime.
  if (!queued_data_.empty()) {
    std::vector<QueuedSharedMemoryData>::iterator iter = queued_data_.begin();
    for (; iter != queued_data_.end(); ++iter) {
      ForwardAndACKData(iter->data, iter->length);
    }

    queued_data_.clear();
  }
}

void ThreadedDataProvider::OnReceivedDataOnBackgroundThread(
    int data_offset, int data_length, int encoded_data_length) {
  DCHECK(background_thread_.isCurrentThread());
  DCHECK(shm_buffer_ != NULL);

  CHECK_GE(shm_size_, data_offset + data_length);
  const char* data_ptr = static_cast<char*>(shm_buffer_->memory());
  CHECK(data_ptr);
  CHECK(data_ptr + data_offset);

  if (resource_filter_active_) {
    ForwardAndACKData(data_ptr + data_offset, data_length);
  } else {
    // There's a brief interval between the point where we know the filter
    // has been installed on the I/O thread, and when we know for sure there's
    // no more data coming in from the main thread (from before the filter
    // got added). If we get any data during that interval, we need to queue
    // it until we're certain we've processed all the main thread data to make
    // sure we forward (and ACK) everything in the right order.
    QueuedSharedMemoryData queued_data;
    queued_data.data = data_ptr + data_offset;
    queued_data.length = data_length;
    queued_data_.push_back(queued_data);
  }
}

void ThreadedDataProvider::OnReceivedDataOnForegroundThread(
    const char* data, int data_length, int encoded_data_length) {
  DCHECK(ChildThread::current());

  background_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&ThreadedDataProvider::ForwardAndACKData,
                 base::Unretained(this),
                 data, data_length));
}

void ThreadedDataProvider::ForwardAndACKData(const char* data,
                                             int data_length) {
  DCHECK(background_thread_.isCurrentThread());

  // TODO(oysteine): SiteIsolationPolicy needs to be be checked
  // here before we pass the data to the data provider
  // (or earlier on the I/O thread), otherwise once SiteIsolationPolicy does
  // actual blocking as opposed to just UMA logging this will bypass it.
  threaded_data_receiver_->acceptData(data, data_length);
  ipc_channel_->Send(new ResourceHostMsg_DataReceived_ACK(request_id_));
}

}  // namespace content
