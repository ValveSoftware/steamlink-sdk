// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/data_pipe_producer_dispatcher.h"

#include "base/logging.h"
#include "mojo/system/data_pipe.h"
#include "mojo/system/memory.h"

namespace mojo {
namespace system {

DataPipeProducerDispatcher::DataPipeProducerDispatcher() {
}

void DataPipeProducerDispatcher::Init(scoped_refptr<DataPipe> data_pipe) {
  DCHECK(data_pipe);
  data_pipe_ = data_pipe;
}

Dispatcher::Type DataPipeProducerDispatcher::GetType() const {
  return kTypeDataPipeProducer;
}

DataPipeProducerDispatcher::~DataPipeProducerDispatcher() {
  // |Close()|/|CloseImplNoLock()| should have taken care of the pipe.
  DCHECK(!data_pipe_);
}

void DataPipeProducerDispatcher::CancelAllWaitersNoLock() {
  lock().AssertAcquired();
  data_pipe_->ProducerCancelAllWaiters();
}

void DataPipeProducerDispatcher::CloseImplNoLock() {
  lock().AssertAcquired();
  data_pipe_->ProducerClose();
  data_pipe_ = NULL;
}

scoped_refptr<Dispatcher>
DataPipeProducerDispatcher::CreateEquivalentDispatcherAndCloseImplNoLock() {
  lock().AssertAcquired();

  scoped_refptr<DataPipeProducerDispatcher> rv =
      new DataPipeProducerDispatcher();
  rv->Init(data_pipe_);
  data_pipe_ = NULL;
  return scoped_refptr<Dispatcher>(rv.get());
}

MojoResult DataPipeProducerDispatcher::WriteDataImplNoLock(
    const void* elements,
    uint32_t* num_bytes,
    MojoWriteDataFlags flags) {
  lock().AssertAcquired();

  if (!VerifyUserPointer<uint32_t>(num_bytes))
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (!VerifyUserPointerWithSize<1>(elements, *num_bytes))
    return MOJO_RESULT_INVALID_ARGUMENT;

  return data_pipe_->ProducerWriteData(
      elements, num_bytes, (flags & MOJO_WRITE_DATA_FLAG_ALL_OR_NONE));
}

MojoResult DataPipeProducerDispatcher::BeginWriteDataImplNoLock(
    void** buffer,
    uint32_t* buffer_num_bytes,
    MojoWriteDataFlags flags) {
  lock().AssertAcquired();

  if (!VerifyUserPointerWithCount<void*>(buffer, 1))
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (!VerifyUserPointer<uint32_t>(buffer_num_bytes))
    return MOJO_RESULT_INVALID_ARGUMENT;

  return data_pipe_->ProducerBeginWriteData(
      buffer, buffer_num_bytes, (flags & MOJO_WRITE_DATA_FLAG_ALL_OR_NONE));
}

MojoResult DataPipeProducerDispatcher::EndWriteDataImplNoLock(
    uint32_t num_bytes_written) {
  lock().AssertAcquired();

  return data_pipe_->ProducerEndWriteData(num_bytes_written);
}

MojoResult DataPipeProducerDispatcher::AddWaiterImplNoLock(
    Waiter* waiter,
    MojoHandleSignals signals,
    uint32_t context) {
  lock().AssertAcquired();
  return data_pipe_->ProducerAddWaiter(waiter, signals, context);
}

void DataPipeProducerDispatcher::RemoveWaiterImplNoLock(Waiter* waiter) {
  lock().AssertAcquired();
  data_pipe_->ProducerRemoveWaiter(waiter);
}

bool DataPipeProducerDispatcher::IsBusyNoLock() const {
  lock().AssertAcquired();
  return data_pipe_->ProducerIsBusy();
}

}  // namespace system
}  // namespace mojo
