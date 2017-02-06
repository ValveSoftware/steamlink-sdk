// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_COMMAND_EXECUTOR_H_
#define GPU_COMMAND_BUFFER_SERVICE_COMMAND_EXECUTOR_H_

#include <stdint.h>

#include <memory>
#include <queue>

#include "base/atomic_ref_count.h"
#include "base/atomicops.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"
#include "gpu/command_buffer/service/cmd_parser.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/gpu_export.h"

namespace gpu {

class PreemptionFlag : public base::RefCountedThreadSafe<PreemptionFlag> {
 public:
  PreemptionFlag() : flag_(0) {}

  bool IsSet() { return !base::AtomicRefCountIsZero(&flag_); }
  void Set() { base::AtomicRefCountInc(&flag_); }
  void Reset() { base::subtle::NoBarrier_Store(&flag_, 0); }

 private:
  base::AtomicRefCount flag_;

  ~PreemptionFlag() {}

  friend class base::RefCountedThreadSafe<PreemptionFlag>;
};

// This class schedules commands that have been flushed. They are received via
// a command buffer and forwarded to a command parser. TODO(apatrick): This
// class should not know about the decoder. Do not add additional dependencies
// on it.
class GPU_EXPORT CommandExecutor
    : NON_EXPORTED_BASE(public CommandBufferEngine),
      public base::SupportsWeakPtr<CommandExecutor> {
 public:
  CommandExecutor(CommandBufferServiceBase* command_buffer,
                  AsyncAPIInterface* handler,
                  gles2::GLES2Decoder* decoder);

  ~CommandExecutor() override;

  void PutChanged();

  void SetPreemptByFlag(scoped_refptr<PreemptionFlag> flag) {
    preemption_flag_ = flag;
  }

  // Sets whether commands should be processed by this scheduler. Setting to
  // false unschedules. Setting to true reschedules.
  void SetScheduled(bool scheduled);

  bool scheduled() const { return scheduled_; }

  // Returns whether the scheduler needs to be polled again in the future to
  // process pending queries.
  bool HasPendingQueries() const;

  // Process pending queries and return. HasPendingQueries() can be used to
  // determine if there's more pending queries after this has been called.
  void ProcessPendingQueries();

  // Implementation of CommandBufferEngine.
  scoped_refptr<Buffer> GetSharedMemoryBuffer(int32_t shm_id) override;
  void set_token(int32_t token) override;
  bool SetGetBuffer(int32_t transfer_buffer_id) override;
  bool SetGetOffset(int32_t offset) override;
  int32_t GetGetOffset() override;

  void SetCommandProcessedCallback(const base::Closure& callback);

  // Returns whether the scheduler needs to be polled again in the future to
  // process idle work.
  bool HasMoreIdleWork() const;

  // Perform some idle work and return. HasMoreIdleWork() can be used to
  // determine if there's more idle work do be done after this has been called.
  void PerformIdleWork();

  // Whether there is state that needs to be regularly polled.
  bool HasPollingWork() const;
  void PerformPollingWork();

  CommandParser* parser() const { return parser_.get(); }

 private:
  bool IsPreempted();

  // The CommandExecutor holds a weak reference to the CommandBuffer. The
  // CommandBuffer owns the CommandExecutor and holds a strong reference to it
  // through the ProcessCommands callback.
  CommandBufferServiceBase* command_buffer_;

  // The parser uses this to execute commands.
  AsyncAPIInterface* handler_;

  // Does not own decoder. TODO(apatrick): The CommandExecutor shouldn't need a
  // pointer to the decoder, it is only used to initialize the CommandParser,
  // which could be an argument to the constructor, and to determine the
  // reason for context lost.
  gles2::GLES2Decoder* decoder_;

  // TODO(apatrick): The CommandExecutor currently creates and owns the parser.
  // This should be an argument to the constructor.
  std::unique_ptr<CommandParser> parser_;

  // Whether the scheduler is currently able to process more commands.
  bool scheduled_;

  base::Closure command_processed_callback_;

  // If non-NULL and |preemption_flag_->IsSet()|, exit PutChanged early.
  scoped_refptr<PreemptionFlag> preemption_flag_;
  bool was_preempted_;

  DISALLOW_COPY_AND_ASSIGN(CommandExecutor);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_COMMAND_EXECUTOR_H_
