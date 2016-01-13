// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_DATA_PIPE_CONSUMER_DISPATCHER_H_
#define MOJO_SYSTEM_DATA_PIPE_CONSUMER_DISPATCHER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "mojo/system/dispatcher.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

class DataPipe;

// This is the |Dispatcher| implementation for the consumer handle for data
// pipes (created by the Mojo primitive |MojoCreateDataPipe()|). This class is
// thread-safe.
class MOJO_SYSTEM_IMPL_EXPORT DataPipeConsumerDispatcher : public Dispatcher {
 public:
  DataPipeConsumerDispatcher();

  // Must be called before any other methods.
  void Init(scoped_refptr<DataPipe> data_pipe);

  // |Dispatcher| public methods:
  virtual Type GetType() const OVERRIDE;

 private:
  virtual ~DataPipeConsumerDispatcher();

  // |Dispatcher| protected methods:
  virtual void CancelAllWaitersNoLock() OVERRIDE;
  virtual void CloseImplNoLock() OVERRIDE;
  virtual scoped_refptr<Dispatcher>
      CreateEquivalentDispatcherAndCloseImplNoLock() OVERRIDE;
  virtual MojoResult ReadDataImplNoLock(void* elements,
                                        uint32_t* num_bytes,
                                        MojoReadDataFlags flags) OVERRIDE;
  virtual MojoResult BeginReadDataImplNoLock(const void** buffer,
                                             uint32_t* buffer_num_bytes,
                                             MojoReadDataFlags flags) OVERRIDE;
  virtual MojoResult EndReadDataImplNoLock(uint32_t num_bytes_read) OVERRIDE;
  virtual MojoResult AddWaiterImplNoLock(Waiter* waiter,
                                         MojoHandleSignals signals,
                                         uint32_t context) OVERRIDE;
  virtual void RemoveWaiterImplNoLock(Waiter* waiter) OVERRIDE;
  virtual bool IsBusyNoLock() const OVERRIDE;

  // Protected by |lock()|:
  scoped_refptr<DataPipe> data_pipe_;  // This will be null if closed.

  DISALLOW_COPY_AND_ASSIGN(DataPipeConsumerDispatcher);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_DATA_PIPE_CONSUMER_DISPATCHER_H_
