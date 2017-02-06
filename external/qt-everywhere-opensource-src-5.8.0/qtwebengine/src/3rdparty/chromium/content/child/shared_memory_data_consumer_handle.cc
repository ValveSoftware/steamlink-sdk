// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/shared_memory_data_consumer_handle.h"

#include <algorithm>
#include <deque>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/child/fixed_received_data.h"

namespace content {

namespace {

class DelegateThreadSafeReceivedData final
    : public RequestPeer::ThreadSafeReceivedData {
 public:
  explicit DelegateThreadSafeReceivedData(
      std::unique_ptr<RequestPeer::ReceivedData> data)
      : data_(std::move(data)),
        task_runner_(base::ThreadTaskRunnerHandle::Get()) {}
  ~DelegateThreadSafeReceivedData() override {
    if (!task_runner_->BelongsToCurrentThread()) {
      // Delete the data on the original thread.
      task_runner_->DeleteSoon(FROM_HERE, data_.release());
    }
  }

  const char* payload() const override { return data_->payload(); }
  int length() const override { return data_->length(); }
  int encoded_length() const override { return data_->encoded_length(); }

 private:
  std::unique_ptr<RequestPeer::ReceivedData> data_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(DelegateThreadSafeReceivedData);
};

}  // namespace

using Result = blink::WebDataConsumerHandle::Result;

// All methods (except for ctor/dtor) must be called with |lock_| aquired
// unless otherwise stated.
class SharedMemoryDataConsumerHandle::Context final
    : public base::RefCountedThreadSafe<Context> {
 public:
  explicit Context(const base::Closure& on_reader_detached)
      : result_(Ok),
        first_offset_(0),
        client_(nullptr),
        writer_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        on_reader_detached_(on_reader_detached),
        is_on_reader_detached_valid_(!on_reader_detached_.is_null()),
        is_handle_active_(true),
        is_two_phase_read_in_progress_(false) {}

  bool IsEmpty() const {
    lock_.AssertAcquired();
    return queue_.empty();
  }
  void ClearIfNecessary() {
    lock_.AssertAcquired();
    if (!is_handle_locked() && !is_handle_active()) {
      // No one is interested in the contents.
      if (is_on_reader_detached_valid_) {
        // We post a task even in the writer thread in order to avoid a
        // reentrance problem as calling |on_reader_detached_| may manipulate
        // the context synchronously.
        writer_task_runner_->PostTask(FROM_HERE, on_reader_detached_);
      }
      Clear();
    }
  }
  void ClearQueue() {
    lock_.AssertAcquired();
    for (auto& data : queue_) {
      delete data;
    }
    queue_.clear();
    first_offset_ = 0;
  }
  RequestPeer::ThreadSafeReceivedData* Top() {
    lock_.AssertAcquired();
    return queue_.front();
  }
  void Push(std::unique_ptr<RequestPeer::ThreadSafeReceivedData> data) {
    lock_.AssertAcquired();
    queue_.push_back(data.release());
  }
  size_t first_offset() const {
    lock_.AssertAcquired();
    return first_offset_;
  }
  Result result() const {
    lock_.AssertAcquired();
    return result_;
  }
  void set_result(Result r) {
    lock_.AssertAcquired();
    result_ = r;
  }
  void AcquireReaderLock(Client* client) {
    lock_.AssertAcquired();
    DCHECK(!notification_task_runner_);
    DCHECK(!client_);
    notification_task_runner_ = base::ThreadTaskRunnerHandle::Get();
    client_ = client;
    if (client && !(IsEmpty() && result() == Ok)) {
      // We cannot notify synchronously because the user doesn't have the reader
      // yet.
      notification_task_runner_->PostTask(
          FROM_HERE, base::Bind(&Context::NotifyInternal, this, false));
    }
  }
  void ReleaseReaderLock() {
    lock_.AssertAcquired();
    DCHECK(notification_task_runner_);
    notification_task_runner_ = nullptr;
    client_ = nullptr;
  }
  void PostNotify() {
    lock_.AssertAcquired();
    auto runner = notification_task_runner_;
    if (!runner)
      return;
    // We don't re-post the task when the runner changes while waiting for
    // this task because in this case a new reader is obtained and
    // notification is already done at the reader creation time if necessary.
    runner->PostTask(FROM_HERE,
                     base::Bind(&Context::NotifyInternal, this, false));
  }
  // Must be called with |lock_| not aquired.
  void Notify() { NotifyInternal(true); }
  // This function doesn't work in the destructor if |on_reader_detached_| is
  // not null.
  void ResetOnReaderDetached() {
    lock_.AssertAcquired();
    if (on_reader_detached_.is_null()) {
      DCHECK(!is_on_reader_detached_valid_);
      return;
    }
    is_on_reader_detached_valid_ = false;
    if (writer_task_runner_->BelongsToCurrentThread()) {
      // We can reset the closure immediately.
      on_reader_detached_.Reset();
    } else {
      // We need to reset |on_reader_detached_| on the right thread because it
      // might lead to the object destruction.
      writer_task_runner_->PostTask(
          FROM_HERE, base::Bind(&Context::ResetOnReaderDetachedWithLock, this));
    }
  }
  bool is_handle_locked() const {
    lock_.AssertAcquired();
    return static_cast<bool>(notification_task_runner_);
  }
  bool IsReaderBoundToCurrentThread() const {
    lock_.AssertAcquired();
    return notification_task_runner_ &&
           notification_task_runner_->BelongsToCurrentThread();
  }
  bool is_handle_active() const {
    lock_.AssertAcquired();
    return is_handle_active_;
  }
  void set_is_handle_active(bool b) {
    lock_.AssertAcquired();
    is_handle_active_ = b;
  }
  void Consume(size_t s) {
    lock_.AssertAcquired();
    first_offset_ += s;
    auto top = Top();
    if (static_cast<size_t>(top->length()) <= first_offset_) {
      delete top;
      queue_.pop_front();
      first_offset_ = 0;
    }
  }
  bool is_two_phase_read_in_progress() const {
    lock_.AssertAcquired();
    return is_two_phase_read_in_progress_;
  }
  void set_is_two_phase_read_in_progress(bool b) {
    lock_.AssertAcquired();
    is_two_phase_read_in_progress_ = b;
  }
  // Can be called with |lock_| not aquired.
  base::Lock& lock() { return lock_; }

 private:
  // Must be called with |lock_| not aquired.
  void NotifyInternal(bool repost) {
    scoped_refptr<base::SingleThreadTaskRunner> runner;
    {
      base::AutoLock lock(lock_);
      runner = notification_task_runner_;
    }
    if (!runner)
      return;

    if (runner->BelongsToCurrentThread()) {
      // It is safe to access member variables without lock because |client_|
      // is bound to the current thread.
      if (client_)
        client_->didGetReadable();
      return;
    }
    if (repost) {
      // We don't re-post the task when the runner changes while waiting for
      // this task because in this case a new reader is obtained and
      // notification is already done at the reader creation time if necessary.
      runner->PostTask(FROM_HERE,
                       base::Bind(&Context::NotifyInternal, this, false));
    }
  }
  void Clear() {
    lock_.AssertAcquired();
    for (auto& data : queue_) {
      delete data;
    }
    queue_.clear();
    first_offset_ = 0;
    client_ = nullptr;
    // Note this doesn't work in the destructor if |on_reader_detached_| is not
    // null. We have an assert in the destructor.
    ResetOnReaderDetached();
  }
  // Must be called with |lock_| not aquired.
  void ResetOnReaderDetachedWithLock() {
    base::AutoLock lock(lock_);
    ResetOnReaderDetached();
  }

  friend class base::RefCountedThreadSafe<Context>;
  ~Context() {
    base::AutoLock lock(lock_);
    DCHECK(on_reader_detached_.is_null());

    // This is necessary because the queue stores raw pointers.
    Clear();
  }

  base::Lock lock_;
  // |result_| stores the ultimate state of this handle if it has. Otherwise,
  // |Ok| is set.
  Result result_;
  // TODO(yhirano): Use std::deque<std::unique_ptr<ThreadSafeReceivedData>>
  // once it is allowed.
  std::deque<RequestPeer::ThreadSafeReceivedData*> queue_;
  size_t first_offset_;
  Client* client_;
  scoped_refptr<base::SingleThreadTaskRunner> notification_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> writer_task_runner_;
  base::Closure on_reader_detached_;
  // We need this boolean variable to remember if |on_reader_detached_| is
  // callable because we need to reset |on_reader_detached_| only on the writer
  // thread and hence |on_reader_detached_.is_null()| is untrustworthy on
  // other threads.
  bool is_on_reader_detached_valid_;
  bool is_handle_active_;
  bool is_two_phase_read_in_progress_;

  DISALLOW_COPY_AND_ASSIGN(Context);
};

SharedMemoryDataConsumerHandle::Writer::Writer(
    const scoped_refptr<Context>& context,
    BackpressureMode mode)
    : context_(context), mode_(mode) {
}

SharedMemoryDataConsumerHandle::Writer::~Writer() {
  Close();
  base::AutoLock lock(context_->lock());
  context_->ResetOnReaderDetached();
}

void SharedMemoryDataConsumerHandle::Writer::AddData(
    std::unique_ptr<RequestPeer::ReceivedData> data) {
  if (!data->length()) {
    // We omit empty data.
    return;
  }

  bool needs_notification = false;
  {
    base::AutoLock lock(context_->lock());
    if (!context_->is_handle_active() && !context_->is_handle_locked()) {
      // No one is interested in the data.
      return;
    }

    needs_notification = context_->IsEmpty();
    std::unique_ptr<RequestPeer::ThreadSafeReceivedData> data_to_pass;
    if (mode_ == kApplyBackpressure) {
      data_to_pass =
          base::WrapUnique(new DelegateThreadSafeReceivedData(std::move(data)));
    } else {
      data_to_pass = base::WrapUnique(new FixedReceivedData(data.get()));
    }
    context_->Push(std::move(data_to_pass));
  }

  if (needs_notification) {
    // We CAN issue the notification synchronously if the associated reader
    // lives in this thread, because this function cannot be called in the
    // client's callback.
    context_->Notify();
  }
}

void SharedMemoryDataConsumerHandle::Writer::Close() {
  base::AutoLock lock(context_->lock());
  if (context_->result() == Ok) {
    context_->set_result(Done);
    context_->ResetOnReaderDetached();
    if (context_->IsEmpty()) {
      // We cannot issue the notification synchronously because this function
      // can be called in the client's callback.
      context_->PostNotify();
    }
  }
}

void SharedMemoryDataConsumerHandle::Writer::Fail() {
  base::AutoLock lock(context_->lock());
  if (context_->result() == Ok) {
    // TODO(yhirano): Use an appropriate error code other than
    // UnexpectedError.
    context_->set_result(UnexpectedError);

    if (context_->is_two_phase_read_in_progress()) {
      // If we are in two-phase read session, we cannot discard the data. We
      // will clear the queue at the end of the session.
    } else {
      context_->ClearQueue();
    }

    context_->ResetOnReaderDetached();
    // We cannot issue the notification synchronously because this function can
    // be called in the client's callback.
    context_->PostNotify();
  }
}

SharedMemoryDataConsumerHandle::ReaderImpl::ReaderImpl(
    scoped_refptr<Context> context,
    Client* client)
    : context_(context) {
  base::AutoLock lock(context_->lock());
  DCHECK(!context_->is_handle_locked());
  context_->AcquireReaderLock(client);
}

SharedMemoryDataConsumerHandle::ReaderImpl::~ReaderImpl() {
  base::AutoLock lock(context_->lock());
  context_->ReleaseReaderLock();
  context_->ClearIfNecessary();
}

Result SharedMemoryDataConsumerHandle::ReaderImpl::read(
    void* data,
    size_t size,
    Flags flags,
    size_t* read_size_to_return) {
  base::AutoLock lock(context_->lock());

  size_t total_read_size = 0;
  *read_size_to_return = 0;

  if (context_->result() == Ok && context_->is_two_phase_read_in_progress())
    context_->set_result(UnexpectedError);

  if (context_->result() != Ok && context_->result() != Done)
    return context_->result();

  while (!context_->IsEmpty() && total_read_size < size) {
    const auto& top = context_->Top();
    size_t readable = top->length() - context_->first_offset();
    size_t writable = size - total_read_size;
    size_t read_size = std::min(readable, writable);
    const char* begin = top->payload() + context_->first_offset();
    std::copy(begin, begin + read_size,
              static_cast<char*>(data) + total_read_size);
    total_read_size += read_size;
    context_->Consume(read_size);
  }
  *read_size_to_return = total_read_size;
  if (total_read_size || !context_->IsEmpty())
    return Ok;
  if (context_->result() == Done)
    return Done;
  return ShouldWait;
}

Result SharedMemoryDataConsumerHandle::ReaderImpl::beginRead(
    const void** buffer,
    Flags flags,
    size_t* available) {
  *buffer = nullptr;
  *available = 0;

  base::AutoLock lock(context_->lock());

  if (context_->result() == Ok && context_->is_two_phase_read_in_progress())
    context_->set_result(UnexpectedError);

  if (context_->result() != Ok && context_->result() != Done)
    return context_->result();

  if (context_->IsEmpty())
    return context_->result() == Done ? Done : ShouldWait;

  context_->set_is_two_phase_read_in_progress(true);
  const auto& top = context_->Top();
  *buffer = top->payload() + context_->first_offset();
  *available = top->length() - context_->first_offset();

  return Ok;
}

Result SharedMemoryDataConsumerHandle::ReaderImpl::endRead(size_t read_size) {
  base::AutoLock lock(context_->lock());

  if (!context_->is_two_phase_read_in_progress())
    return UnexpectedError;

  context_->set_is_two_phase_read_in_progress(false);
  if (context_->result() != Ok && context_->result() != Done) {
    // We have an error, so we can discard the stored data.
    context_->ClearQueue();
  } else {
    context_->Consume(read_size);
  }

  return Ok;
}

SharedMemoryDataConsumerHandle::SharedMemoryDataConsumerHandle(
    BackpressureMode mode,
    std::unique_ptr<Writer>* writer)
    : SharedMemoryDataConsumerHandle(mode, base::Closure(), writer) {}

SharedMemoryDataConsumerHandle::SharedMemoryDataConsumerHandle(
    BackpressureMode mode,
    const base::Closure& on_reader_detached,
    std::unique_ptr<Writer>* writer)
    : context_(new Context(on_reader_detached)) {
  writer->reset(new Writer(context_, mode));
}

SharedMemoryDataConsumerHandle::~SharedMemoryDataConsumerHandle() {
  base::AutoLock lock(context_->lock());
  context_->set_is_handle_active(false);
  context_->ClearIfNecessary();
}

std::unique_ptr<blink::WebDataConsumerHandle::Reader>
SharedMemoryDataConsumerHandle::ObtainReader(Client* client) {
  return base::WrapUnique(obtainReaderInternal(client));
}

SharedMemoryDataConsumerHandle::ReaderImpl*
SharedMemoryDataConsumerHandle::obtainReaderInternal(Client* client) {
  return new ReaderImpl(context_, client);
}

const char* SharedMemoryDataConsumerHandle::debugName() const {
  return "SharedMemoryDataConsumerHandle";
}

}  // namespace content
