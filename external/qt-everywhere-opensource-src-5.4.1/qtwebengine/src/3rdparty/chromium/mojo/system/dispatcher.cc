// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/dispatcher.h"

#include "base/logging.h"
#include "mojo/system/constants.h"
#include "mojo/system/message_pipe_dispatcher.h"
#include "mojo/system/platform_handle_dispatcher.h"
#include "mojo/system/shared_buffer_dispatcher.h"

namespace mojo {
namespace system {

namespace test {

// TODO(vtl): Maybe this should be defined in a test-only file instead.
DispatcherTransport DispatcherTryStartTransport(
    Dispatcher* dispatcher) {
  return Dispatcher::HandleTableAccess::TryStartTransport(dispatcher);
}

}  // namespace test

// Dispatcher ------------------------------------------------------------------

// static
DispatcherTransport Dispatcher::HandleTableAccess::TryStartTransport(
    Dispatcher* dispatcher) {
  DCHECK(dispatcher);

  if (!dispatcher->lock_.Try())
    return DispatcherTransport();

  // We shouldn't race with things that close dispatchers, since closing can
  // only take place either under |handle_table_lock_| or when the handle is
  // marked as busy.
  DCHECK(!dispatcher->is_closed_);

  return DispatcherTransport(dispatcher);
}

// static
void Dispatcher::TransportDataAccess::StartSerialize(
    Dispatcher* dispatcher,
    Channel* channel,
    size_t* max_size,
    size_t* max_platform_handles) {
  DCHECK(dispatcher);
  dispatcher->StartSerialize(channel, max_size, max_platform_handles);
}

// static
bool Dispatcher::TransportDataAccess::EndSerializeAndClose(
    Dispatcher* dispatcher,
    Channel* channel,
    void* destination,
    size_t* actual_size,
    embedder::PlatformHandleVector* platform_handles) {
  DCHECK(dispatcher);
  return dispatcher->EndSerializeAndClose(channel, destination, actual_size,
                                          platform_handles);
}

// static
scoped_refptr<Dispatcher> Dispatcher::TransportDataAccess::Deserialize(
    Channel* channel,
    int32_t type,
    const void* source,
    size_t size,
    embedder::PlatformHandleVector* platform_handles) {
  switch (static_cast<int32_t>(type)) {
    case kTypeUnknown:
      DVLOG(2) << "Deserializing invalid handle";
      return scoped_refptr<Dispatcher>();
    case kTypeMessagePipe:
      return scoped_refptr<Dispatcher>(
          MessagePipeDispatcher::Deserialize(channel, source, size));
    case kTypeDataPipeProducer:
    case kTypeDataPipeConsumer:
      // TODO(vtl): Implement.
      LOG(WARNING) << "Deserialization of dispatcher type " << type
                   << " not supported";
      return scoped_refptr<Dispatcher>();
    case kTypeSharedBuffer:
      return scoped_refptr<Dispatcher>(
          SharedBufferDispatcher::Deserialize(channel, source, size,
                                              platform_handles));
    case kTypePlatformHandle:
      return scoped_refptr<Dispatcher>(
          PlatformHandleDispatcher::Deserialize(channel, source, size,
                                                platform_handles));
  }
  LOG(WARNING) << "Unknown dispatcher type " << type;
  return scoped_refptr<Dispatcher>();
}

MojoResult Dispatcher::Close() {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  CloseNoLock();
  return MOJO_RESULT_OK;
}

MojoResult Dispatcher::WriteMessage(
    const void* bytes,
    uint32_t num_bytes,
    std::vector<DispatcherTransport>* transports,
    MojoWriteMessageFlags flags) {
  DCHECK(!transports || (transports->size() > 0 &&
                         transports->size() < kMaxMessageNumHandles));

  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return WriteMessageImplNoLock(bytes, num_bytes, transports, flags);
}

MojoResult Dispatcher::ReadMessage(void* bytes,
                                   uint32_t* num_bytes,
                                   DispatcherVector* dispatchers,
                                   uint32_t* num_dispatchers,
    MojoReadMessageFlags flags) {
  DCHECK(!num_dispatchers || *num_dispatchers == 0 ||
         (dispatchers && dispatchers->empty()));

  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return ReadMessageImplNoLock(bytes, num_bytes, dispatchers, num_dispatchers,
                               flags);
}

MojoResult Dispatcher::WriteData(const void* elements,
                                 uint32_t* num_bytes,
                                 MojoWriteDataFlags flags) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return WriteDataImplNoLock(elements, num_bytes, flags);
}

MojoResult Dispatcher::BeginWriteData(void** buffer,
                                      uint32_t* buffer_num_bytes,
                                      MojoWriteDataFlags flags) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return BeginWriteDataImplNoLock(buffer, buffer_num_bytes, flags);
}

MojoResult Dispatcher::EndWriteData(uint32_t num_bytes_written) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return EndWriteDataImplNoLock(num_bytes_written);
}

MojoResult Dispatcher::ReadData(void* elements,
                                uint32_t* num_bytes,
                                MojoReadDataFlags flags) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return ReadDataImplNoLock(elements, num_bytes, flags);
}

MojoResult Dispatcher::BeginReadData(const void** buffer,
                                     uint32_t* buffer_num_bytes,
                                     MojoReadDataFlags flags) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return BeginReadDataImplNoLock(buffer, buffer_num_bytes, flags);
}

MojoResult Dispatcher::EndReadData(uint32_t num_bytes_read) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return EndReadDataImplNoLock(num_bytes_read);
}

MojoResult Dispatcher::DuplicateBufferHandle(
    const MojoDuplicateBufferHandleOptions* options,
    scoped_refptr<Dispatcher>* new_dispatcher) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return DuplicateBufferHandleImplNoLock(options, new_dispatcher);
}

MojoResult Dispatcher::MapBuffer(
    uint64_t offset,
    uint64_t num_bytes,
    MojoMapBufferFlags flags,
    scoped_ptr<RawSharedBufferMapping>* mapping) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return MapBufferImplNoLock(offset, num_bytes, flags, mapping);
}

MojoResult Dispatcher::AddWaiter(Waiter* waiter,
                                 MojoHandleSignals signals,
                                 uint32_t context) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return AddWaiterImplNoLock(waiter, signals, context);
}

void Dispatcher::RemoveWaiter(Waiter* waiter) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return;
  RemoveWaiterImplNoLock(waiter);
}

Dispatcher::Dispatcher()
    : is_closed_(false) {
}

Dispatcher::~Dispatcher() {
  // Make sure that |Close()| was called.
  DCHECK(is_closed_);
}

void Dispatcher::CancelAllWaitersNoLock() {
  lock_.AssertAcquired();
  DCHECK(is_closed_);
  // By default, waiting isn't supported. Only dispatchers that can be waited on
  // will do something nontrivial.
}

void Dispatcher::CloseImplNoLock() {
  lock_.AssertAcquired();
  DCHECK(is_closed_);
  // This may not need to do anything. Dispatchers should override this to do
  // any actual close-time cleanup necessary.
}

MojoResult Dispatcher::WriteMessageImplNoLock(
    const void* /*bytes*/,
    uint32_t /*num_bytes*/,
    std::vector<DispatcherTransport>* /*transports*/,
    MojoWriteMessageFlags /*flags*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, not supported. Only needed for message pipe dispatchers.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::ReadMessageImplNoLock(void* /*bytes*/,
                                             uint32_t* /*num_bytes*/,
                                             DispatcherVector* /*dispatchers*/,
                                             uint32_t* /*num_dispatchers*/,
                                             MojoReadMessageFlags /*flags*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, not supported. Only needed for message pipe dispatchers.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::WriteDataImplNoLock(const void* /*elements*/,
                                           uint32_t* /*num_bytes*/,
                                           MojoWriteDataFlags /*flags*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, not supported. Only needed for data pipe dispatchers.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::BeginWriteDataImplNoLock(void** /*buffer*/,
                                                uint32_t* /*buffer_num_bytes*/,
                                                MojoWriteDataFlags /*flags*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, not supported. Only needed for data pipe dispatchers.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::EndWriteDataImplNoLock(uint32_t /*num_bytes_written*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, not supported. Only needed for data pipe dispatchers.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::ReadDataImplNoLock(void* /*elements*/,
                                          uint32_t* /*num_bytes*/,
                                          MojoReadDataFlags /*flags*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, not supported. Only needed for data pipe dispatchers.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::BeginReadDataImplNoLock(const void** /*buffer*/,
                                               uint32_t* /*buffer_num_bytes*/,
                                               MojoReadDataFlags /*flags*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, not supported. Only needed for data pipe dispatchers.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::EndReadDataImplNoLock(uint32_t /*num_bytes_read*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, not supported. Only needed for data pipe dispatchers.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::DuplicateBufferHandleImplNoLock(
      const MojoDuplicateBufferHandleOptions* /*options*/,
      scoped_refptr<Dispatcher>* /*new_dispatcher*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, not supported. Only needed for buffer dispatchers.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::MapBufferImplNoLock(
    uint64_t /*offset*/,
    uint64_t /*num_bytes*/,
    MojoMapBufferFlags /*flags*/,
    scoped_ptr<RawSharedBufferMapping>* /*mapping*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, not supported. Only needed for buffer dispatchers.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::AddWaiterImplNoLock(Waiter* /*waiter*/,
                                           MojoHandleSignals /*signals*/,
                                           uint32_t /*context*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, waiting isn't supported. Only dispatchers that can be waited on
  // will do something nontrivial.
  return MOJO_RESULT_FAILED_PRECONDITION;
}

void Dispatcher::RemoveWaiterImplNoLock(Waiter* /*waiter*/) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, waiting isn't supported. Only dispatchers that can be waited on
  // will do something nontrivial.
}

void Dispatcher::StartSerializeImplNoLock(Channel* /*channel*/,
                                          size_t* max_size,
                                          size_t* max_platform_handles) {
  DCHECK(HasOneRef());  // Only one ref => no need to take the lock.
  DCHECK(!is_closed_);
  *max_size = 0;
  *max_platform_handles = 0;
}

bool Dispatcher::EndSerializeAndCloseImplNoLock(
    Channel* /*channel*/,
    void* /*destination*/,
    size_t* /*actual_size*/,
    embedder::PlatformHandleVector* /*platform_handles*/) {
  DCHECK(HasOneRef());  // Only one ref => no need to take the lock.
  DCHECK(is_closed_);
  // By default, serializing isn't supported, so just close.
  CloseImplNoLock();
  return false;
}

bool Dispatcher::IsBusyNoLock() const {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // Most dispatchers support only "atomic" operations, so they are never busy
  // (in this sense).
  return false;
}

void Dispatcher::CloseNoLock() {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);

  is_closed_ = true;
  CancelAllWaitersNoLock();
  CloseImplNoLock();
}

scoped_refptr<Dispatcher>
Dispatcher::CreateEquivalentDispatcherAndCloseNoLock() {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);

  is_closed_ = true;
  CancelAllWaitersNoLock();
  return CreateEquivalentDispatcherAndCloseImplNoLock();
}

void Dispatcher::StartSerialize(Channel* channel,
                                size_t* max_size,
                                size_t* max_platform_handles) {
  DCHECK(channel);
  DCHECK(max_size);
  DCHECK(max_platform_handles);
  DCHECK(HasOneRef());  // Only one ref => no need to take the lock.
  DCHECK(!is_closed_);
  StartSerializeImplNoLock(channel, max_size, max_platform_handles);
}

bool Dispatcher::EndSerializeAndClose(
    Channel* channel,
    void* destination,
    size_t* actual_size,
    embedder::PlatformHandleVector* platform_handles) {
  DCHECK(channel);
  DCHECK(actual_size);
  DCHECK(HasOneRef());  // Only one ref => no need to take the lock.
  DCHECK(!is_closed_);

  // Like other |...Close()| methods, we mark ourselves as closed before calling
  // the impl.
  is_closed_ = true;
  // No need to cancel waiters: we shouldn't have any (and shouldn't be in
  // |Core|'s handle table.

#if !defined(NDEBUG)
  // See the comment above |EndSerializeAndCloseImplNoLock()|. In brief: Locking
  // isn't actually needed, but we need to satisfy assertions (which we don't
  // want to remove or weaken).
  base::AutoLock locker(lock_);
#endif

  return EndSerializeAndCloseImplNoLock(channel, destination, actual_size,
                                        platform_handles);
}

// DispatcherTransport ---------------------------------------------------------

void DispatcherTransport::End() {
  DCHECK(dispatcher_);
  dispatcher_->lock_.Release();
  dispatcher_ = NULL;
}

}  // namespace system
}  // namespace mojo
