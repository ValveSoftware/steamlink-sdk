// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/core.h"

#include <vector>

#include "base/logging.h"
#include "base/time/time.h"
#include "mojo/public/c/system/macros.h"
#include "mojo/system/constants.h"
#include "mojo/system/data_pipe.h"
#include "mojo/system/data_pipe_consumer_dispatcher.h"
#include "mojo/system/data_pipe_producer_dispatcher.h"
#include "mojo/system/dispatcher.h"
#include "mojo/system/local_data_pipe.h"
#include "mojo/system/memory.h"
#include "mojo/system/message_pipe.h"
#include "mojo/system/message_pipe_dispatcher.h"
#include "mojo/system/raw_shared_buffer.h"
#include "mojo/system/shared_buffer_dispatcher.h"
#include "mojo/system/waiter.h"

namespace mojo {
namespace system {

// Implementation notes
//
// Mojo primitives are implemented by the singleton |Core| object. Most calls
// are for a "primary" handle (the first argument). |Core::GetDispatcher()| is
// used to look up a |Dispatcher| object for a given handle. That object
// implements most primitives for that object. The wait primitives are not
// attached to objects and are implemented by |Core| itself.
//
// Some objects have multiple handles associated to them, e.g., message pipes
// (which have two). In such a case, there is still a |Dispatcher| (e.g.,
// |MessagePipeDispatcher|) for each handle, with each handle having a strong
// reference to the common "secondary" object (e.g., |MessagePipe|). This
// secondary object does NOT have any references to the |Dispatcher|s (even if
// it did, it wouldn't be able to do anything with them due to lock order
// requirements -- see below).
//
// Waiting is implemented by having the thread that wants to wait call the
// |Dispatcher|s for the handles that it wants to wait on with a |Waiter|
// object; this |Waiter| object may be created on the stack of that thread or be
// kept in thread local storage for that thread (TODO(vtl): future improvement).
// The |Dispatcher| then adds the |Waiter| to a |WaiterList| that's either owned
// by that |Dispatcher| (see |SimpleDispatcher|) or by a secondary object (e.g.,
// |MessagePipe|). To signal/wake a |Waiter|, the object in question -- either a
// |SimpleDispatcher| or a secondary object -- talks to its |WaiterList|.

// Thread-safety notes
//
// Mojo primitives calls are thread-safe. We achieve this with relatively
// fine-grained locking. There is a global handle table lock. This lock should
// be held as briefly as possible (TODO(vtl): a future improvement would be to
// switch it to a reader-writer lock). Each |Dispatcher| object then has a lock
// (which subclasses can use to protect their data).
//
// The lock ordering is as follows:
//   1. global handle table lock, global mapping table lock
//   2. |Dispatcher| locks
//   3. secondary object locks
//   ...
//   INF. |Waiter| locks
//
// Notes:
//    - While holding a |Dispatcher| lock, you may not unconditionally attempt
//      to take another |Dispatcher| lock. (This has consequences on the
//      concurrency semantics of |MojoWriteMessage()| when passing handles.)
//      Doing so would lead to deadlock.
//    - Locks at the "INF" level may not have any locks taken while they are
//      held.

Core::Core() {
}

Core::~Core() {
}

MojoHandle Core::AddDispatcher(
    const scoped_refptr<Dispatcher>& dispatcher) {
  base::AutoLock locker(handle_table_lock_);
  return handle_table_.AddDispatcher(dispatcher);
}

scoped_refptr<Dispatcher> Core::GetDispatcher(MojoHandle handle) {
  if (handle == MOJO_HANDLE_INVALID)
    return NULL;

  base::AutoLock locker(handle_table_lock_);
  return handle_table_.GetDispatcher(handle);
}

MojoTimeTicks Core::GetTimeTicksNow() {
  return base::TimeTicks::Now().ToInternalValue();
}

MojoResult Core::Close(MojoHandle handle) {
  if (handle == MOJO_HANDLE_INVALID)
    return MOJO_RESULT_INVALID_ARGUMENT;

  scoped_refptr<Dispatcher> dispatcher;
  {
    base::AutoLock locker(handle_table_lock_);
    MojoResult result = handle_table_.GetAndRemoveDispatcher(handle,
                                                             &dispatcher);
    if (result != MOJO_RESULT_OK)
      return result;
  }

  // The dispatcher doesn't have a say in being closed, but gets notified of it.
  // Note: This is done outside of |handle_table_lock_|. As a result, there's a
  // race condition that the dispatcher must handle; see the comment in
  // |Dispatcher| in dispatcher.h.
  return dispatcher->Close();
}

MojoResult Core::Wait(MojoHandle handle,
                      MojoHandleSignals signals,
                      MojoDeadline deadline) {
  return WaitManyInternal(&handle, &signals, 1, deadline);
}

MojoResult Core::WaitMany(const MojoHandle* handles,
                          const MojoHandleSignals* signals,
                          uint32_t num_handles,
                          MojoDeadline deadline) {
  if (!VerifyUserPointerWithCount<MojoHandle>(handles, num_handles))
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (!VerifyUserPointerWithCount<MojoHandleSignals>(signals, num_handles))
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (num_handles < 1)
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (num_handles > kMaxWaitManyNumHandles)
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  return WaitManyInternal(handles, signals, num_handles, deadline);
}

MojoResult Core::CreateMessagePipe(const MojoCreateMessagePipeOptions* options,
                                   MojoHandle* message_pipe_handle0,
                                   MojoHandle* message_pipe_handle1) {
  MojoCreateMessagePipeOptions validated_options = {};
  // This will verify the |options| pointer.
  MojoResult result = MessagePipeDispatcher::ValidateCreateOptions(
      options, &validated_options);
  if (result != MOJO_RESULT_OK)
    return result;
  if (!VerifyUserPointer<MojoHandle>(message_pipe_handle0))
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (!VerifyUserPointer<MojoHandle>(message_pipe_handle1))
    return MOJO_RESULT_INVALID_ARGUMENT;

  scoped_refptr<MessagePipeDispatcher> dispatcher0(
      new MessagePipeDispatcher(validated_options));
  scoped_refptr<MessagePipeDispatcher> dispatcher1(
      new MessagePipeDispatcher(validated_options));

  std::pair<MojoHandle, MojoHandle> handle_pair;
  {
    base::AutoLock locker(handle_table_lock_);
    handle_pair = handle_table_.AddDispatcherPair(dispatcher0, dispatcher1);
  }
  if (handle_pair.first == MOJO_HANDLE_INVALID) {
    DCHECK_EQ(handle_pair.second, MOJO_HANDLE_INVALID);
    LOG(ERROR) << "Handle table full";
    dispatcher0->Close();
    dispatcher1->Close();
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  }

  scoped_refptr<MessagePipe> message_pipe(new MessagePipe());
  dispatcher0->Init(message_pipe, 0);
  dispatcher1->Init(message_pipe, 1);

  *message_pipe_handle0 = handle_pair.first;
  *message_pipe_handle1 = handle_pair.second;
  return MOJO_RESULT_OK;
}

// Implementation note: To properly cancel waiters and avoid other races, this
// does not transfer dispatchers from one handle to another, even when sending a
// message in-process. Instead, it must transfer the "contents" of the
// dispatcher to a new dispatcher, and then close the old dispatcher. If this
// isn't done, in the in-process case, calls on the old handle may complete
// after the the message has been received and a new handle created (and
// possibly even after calls have been made on the new handle).
MojoResult Core::WriteMessage(MojoHandle message_pipe_handle,
                              const void* bytes,
                              uint32_t num_bytes,
                              const MojoHandle* handles,
                              uint32_t num_handles,
                              MojoWriteMessageFlags flags) {
  scoped_refptr<Dispatcher> dispatcher(GetDispatcher(message_pipe_handle));
  if (!dispatcher)
    return MOJO_RESULT_INVALID_ARGUMENT;

  // Easy case: not sending any handles.
  if (num_handles == 0)
    return dispatcher->WriteMessage(bytes, num_bytes, NULL, flags);

  // We have to handle |handles| here, since we have to mark them busy in the
  // global handle table. We can't delegate this to the dispatcher, since the
  // handle table lock must be acquired before the dispatcher lock.
  //
  // (This leads to an oddity: |handles|/|num_handles| are always verified for
  // validity, even for dispatchers that don't support |WriteMessage()| and will
  // simply return failure unconditionally. It also breaks the usual
  // left-to-right verification order of arguments.)
  if (!VerifyUserPointerWithCount<MojoHandle>(handles, num_handles))
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (num_handles > kMaxMessageNumHandles)
    return MOJO_RESULT_RESOURCE_EXHAUSTED;

  // We'll need to hold on to the dispatchers so that we can pass them on to
  // |WriteMessage()| and also so that we can unlock their locks afterwards
  // without accessing the handle table. These can be dumb pointers, since their
  // entries in the handle table won't get removed (since they'll be marked as
  // busy).
  std::vector<DispatcherTransport> transports(num_handles);

  // When we pass handles, we have to try to take all their dispatchers' locks
  // and mark the handles as busy. If the call succeeds, we then remove the
  // handles from the handle table.
  {
    base::AutoLock locker(handle_table_lock_);
    MojoResult result = handle_table_.MarkBusyAndStartTransport(
        message_pipe_handle, handles, num_handles, &transports);
    if (result != MOJO_RESULT_OK)
      return result;
  }

  MojoResult rv = dispatcher->WriteMessage(bytes, num_bytes, &transports,
                                           flags);

  // We need to release the dispatcher locks before we take the handle table
  // lock.
  for (uint32_t i = 0; i < num_handles; i++)
    transports[i].End();

  {
    base::AutoLock locker(handle_table_lock_);
    if (rv == MOJO_RESULT_OK)
      handle_table_.RemoveBusyHandles(handles, num_handles);
    else
      handle_table_.RestoreBusyHandles(handles, num_handles);
  }

  return rv;
}

MojoResult Core::ReadMessage(MojoHandle message_pipe_handle,
                             void* bytes,
                             uint32_t* num_bytes,
                             MojoHandle* handles,
                             uint32_t* num_handles,
                             MojoReadMessageFlags flags) {
  scoped_refptr<Dispatcher> dispatcher(GetDispatcher(message_pipe_handle));
  if (!dispatcher)
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (num_handles) {
    if (!VerifyUserPointer<uint32_t>(num_handles))
      return MOJO_RESULT_INVALID_ARGUMENT;
    if (!VerifyUserPointerWithCount<MojoHandle>(handles, *num_handles))
      return MOJO_RESULT_INVALID_ARGUMENT;
  }

  // Easy case: won't receive any handles.
  if (!num_handles || *num_handles == 0)
    return dispatcher->ReadMessage(bytes, num_bytes, NULL, num_handles, flags);

  DispatcherVector dispatchers;
  MojoResult rv = dispatcher->ReadMessage(bytes, num_bytes,
                                          &dispatchers, num_handles,
                                          flags);
  if (!dispatchers.empty()) {
    DCHECK_EQ(rv, MOJO_RESULT_OK);
    DCHECK(num_handles);
    DCHECK_LE(dispatchers.size(), static_cast<size_t>(*num_handles));

    bool success;
    {
      base::AutoLock locker(handle_table_lock_);
      success = handle_table_.AddDispatcherVector(dispatchers, handles);
    }
    if (!success) {
      LOG(ERROR) << "Received message with " << dispatchers.size()
                 << " handles, but handle table full";
      // Close dispatchers (outside the lock).
      for (size_t i = 0; i < dispatchers.size(); i++) {
        if (dispatchers[i])
          dispatchers[i]->Close();
      }
    }
  }

  return rv;
}

MojoResult Core::CreateDataPipe(const MojoCreateDataPipeOptions* options,
                                MojoHandle* data_pipe_producer_handle,
                                MojoHandle* data_pipe_consumer_handle) {
  MojoCreateDataPipeOptions validated_options = {};
  // This will verify the |options| pointer.
  MojoResult result = DataPipe::ValidateCreateOptions(options,
                                                      &validated_options);
  if (result != MOJO_RESULT_OK)
    return result;
  if (!VerifyUserPointer<MojoHandle>(data_pipe_producer_handle))
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (!VerifyUserPointer<MojoHandle>(data_pipe_consumer_handle))
    return MOJO_RESULT_INVALID_ARGUMENT;

  scoped_refptr<DataPipeProducerDispatcher> producer_dispatcher(
      new DataPipeProducerDispatcher());
  scoped_refptr<DataPipeConsumerDispatcher> consumer_dispatcher(
      new DataPipeConsumerDispatcher());

  std::pair<MojoHandle, MojoHandle> handle_pair;
  {
    base::AutoLock locker(handle_table_lock_);
    handle_pair = handle_table_.AddDispatcherPair(producer_dispatcher,
                                                  consumer_dispatcher);
  }
  if (handle_pair.first == MOJO_HANDLE_INVALID) {
    DCHECK_EQ(handle_pair.second, MOJO_HANDLE_INVALID);
    LOG(ERROR) << "Handle table full";
    producer_dispatcher->Close();
    consumer_dispatcher->Close();
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  }
  DCHECK_NE(handle_pair.second, MOJO_HANDLE_INVALID);

  scoped_refptr<DataPipe> data_pipe(new LocalDataPipe(validated_options));
  producer_dispatcher->Init(data_pipe);
  consumer_dispatcher->Init(data_pipe);

  *data_pipe_producer_handle = handle_pair.first;
  *data_pipe_consumer_handle = handle_pair.second;
  return MOJO_RESULT_OK;
}

MojoResult Core::WriteData(MojoHandle data_pipe_producer_handle,
                           const void* elements,
                           uint32_t* num_bytes,
                           MojoWriteDataFlags flags) {
  scoped_refptr<Dispatcher> dispatcher(
      GetDispatcher(data_pipe_producer_handle));
  if (!dispatcher)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return dispatcher->WriteData(elements, num_bytes, flags);
}

MojoResult Core::BeginWriteData(MojoHandle data_pipe_producer_handle,
                                void** buffer,
                                uint32_t* buffer_num_bytes,
                                MojoWriteDataFlags flags) {
  scoped_refptr<Dispatcher> dispatcher(
      GetDispatcher(data_pipe_producer_handle));
  if (!dispatcher)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return dispatcher->BeginWriteData(buffer, buffer_num_bytes, flags);
}

MojoResult Core::EndWriteData(MojoHandle data_pipe_producer_handle,
                              uint32_t num_bytes_written) {
  scoped_refptr<Dispatcher> dispatcher(
      GetDispatcher(data_pipe_producer_handle));
  if (!dispatcher)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return dispatcher->EndWriteData(num_bytes_written);
}

MojoResult Core::ReadData(MojoHandle data_pipe_consumer_handle,
                          void* elements,
                          uint32_t* num_bytes,
                          MojoReadDataFlags flags) {
  scoped_refptr<Dispatcher> dispatcher(
      GetDispatcher(data_pipe_consumer_handle));
  if (!dispatcher)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return dispatcher->ReadData(elements, num_bytes, flags);
}

MojoResult Core::BeginReadData(MojoHandle data_pipe_consumer_handle,
                               const void** buffer,
                               uint32_t* buffer_num_bytes,
                               MojoReadDataFlags flags) {
  scoped_refptr<Dispatcher> dispatcher(
      GetDispatcher(data_pipe_consumer_handle));
  if (!dispatcher)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return dispatcher->BeginReadData(buffer, buffer_num_bytes, flags);
}

MojoResult Core::EndReadData(MojoHandle data_pipe_consumer_handle,
                             uint32_t num_bytes_read) {
  scoped_refptr<Dispatcher> dispatcher(
      GetDispatcher(data_pipe_consumer_handle));
  if (!dispatcher)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return dispatcher->EndReadData(num_bytes_read);
}

MojoResult Core::CreateSharedBuffer(
    const MojoCreateSharedBufferOptions* options,
    uint64_t num_bytes,
    MojoHandle* shared_buffer_handle) {
  MojoCreateSharedBufferOptions validated_options = {};
  // This will verify the |options| pointer.
  MojoResult result =
      SharedBufferDispatcher::ValidateCreateOptions(options,
                                                    &validated_options);
  if (result != MOJO_RESULT_OK)
    return result;
  if (!VerifyUserPointer<MojoHandle>(shared_buffer_handle))
    return MOJO_RESULT_INVALID_ARGUMENT;

  scoped_refptr<SharedBufferDispatcher> dispatcher;
  result = SharedBufferDispatcher::Create(validated_options, num_bytes,
                                          &dispatcher);
  if (result != MOJO_RESULT_OK) {
    DCHECK(!dispatcher);
    return result;
  }

  MojoHandle h = AddDispatcher(dispatcher);
  if (h == MOJO_HANDLE_INVALID) {
    LOG(ERROR) << "Handle table full";
    dispatcher->Close();
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  }

  *shared_buffer_handle = h;
  return MOJO_RESULT_OK;
}

MojoResult Core::DuplicateBufferHandle(
    MojoHandle buffer_handle,
    const MojoDuplicateBufferHandleOptions* options,
    MojoHandle* new_buffer_handle) {
  scoped_refptr<Dispatcher> dispatcher(GetDispatcher(buffer_handle));
  if (!dispatcher)
    return MOJO_RESULT_INVALID_ARGUMENT;

  // Don't verify |options| here; that's the dispatcher's job.
  if (!VerifyUserPointer<MojoHandle>(new_buffer_handle))
    return MOJO_RESULT_INVALID_ARGUMENT;

  scoped_refptr<Dispatcher> new_dispatcher;
  MojoResult result = dispatcher->DuplicateBufferHandle(options,
                                                        &new_dispatcher);
  if (result != MOJO_RESULT_OK)
    return result;

  MojoHandle new_handle = AddDispatcher(new_dispatcher);
  if (new_handle == MOJO_HANDLE_INVALID) {
    LOG(ERROR) << "Handle table full";
    dispatcher->Close();
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  }

  *new_buffer_handle = new_handle;
  return MOJO_RESULT_OK;
}

MojoResult Core::MapBuffer(MojoHandle buffer_handle,
                           uint64_t offset,
                           uint64_t num_bytes,
                           void** buffer,
                           MojoMapBufferFlags flags) {
  scoped_refptr<Dispatcher> dispatcher(GetDispatcher(buffer_handle));
  if (!dispatcher)
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (!VerifyUserPointerWithCount<void*>(buffer, 1))
    return MOJO_RESULT_INVALID_ARGUMENT;

  scoped_ptr<RawSharedBufferMapping> mapping;
  MojoResult result = dispatcher->MapBuffer(offset, num_bytes, flags, &mapping);
  if (result != MOJO_RESULT_OK)
    return result;

  DCHECK(mapping);
  void* address = mapping->base();
  {
    base::AutoLock locker(mapping_table_lock_);
    result = mapping_table_.AddMapping(mapping.Pass());
  }
  if (result != MOJO_RESULT_OK)
    return result;

  *buffer = address;
  return MOJO_RESULT_OK;
}

MojoResult Core::UnmapBuffer(void* buffer) {
  base::AutoLock locker(mapping_table_lock_);
  return mapping_table_.RemoveMapping(buffer);
}

// Note: We allow |handles| to repeat the same handle multiple times, since
// different flags may be specified.
// TODO(vtl): This incurs a performance cost in |RemoveWaiter()|. Analyze this
// more carefully and address it if necessary.
MojoResult Core::WaitManyInternal(const MojoHandle* handles,
                                  const MojoHandleSignals* signals,
                                  uint32_t num_handles,
                                  MojoDeadline deadline) {
  DCHECK_GT(num_handles, 0u);

  DispatcherVector dispatchers;
  dispatchers.reserve(num_handles);
  for (uint32_t i = 0; i < num_handles; i++) {
    scoped_refptr<Dispatcher> dispatcher = GetDispatcher(handles[i]);
    if (!dispatcher)
      return MOJO_RESULT_INVALID_ARGUMENT;
    dispatchers.push_back(dispatcher);
  }

  // TODO(vtl): Should make the waiter live (permanently) in TLS.
  Waiter waiter;
  waiter.Init();

  uint32_t i;
  MojoResult rv = MOJO_RESULT_OK;
  for (i = 0; i < num_handles; i++) {
    rv = dispatchers[i]->AddWaiter(&waiter, signals[i], i);
    if (rv != MOJO_RESULT_OK)
      break;
  }
  uint32_t num_added = i;

  if (rv == MOJO_RESULT_ALREADY_EXISTS) {
    rv = static_cast<MojoResult>(i);  // The i-th one is already "triggered".
  } else if (rv == MOJO_RESULT_OK) {
    uint32_t context = static_cast<uint32_t>(-1);
    rv = waiter.Wait(deadline, &context);
    if (rv == MOJO_RESULT_OK)
      rv = static_cast<MojoResult>(context);
  }

  // Make sure no other dispatchers try to wake |waiter| for the current
  // |Wait()|/|WaitMany()| call. (Only after doing this can |waiter| be
  // destroyed, but this would still be required if the waiter were in TLS.)
  for (i = 0; i < num_added; i++)
    dispatchers[i]->RemoveWaiter(&waiter);

  return rv;
}

}  // namespace system
}  // namespace mojo
