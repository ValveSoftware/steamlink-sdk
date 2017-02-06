// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_COMMAND_BUFFER_SERVICE_H_
#define GPU_COMMAND_BUFFER_SERVICE_COMMAND_BUFFER_SERVICE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/command_buffer_shared.h"

namespace gpu {

class TransferBufferManagerInterface;

class GPU_EXPORT CommandBufferServiceBase : public CommandBuffer {
 public:
  // Sets the current get offset. This can be called from any thread.
  virtual void SetGetOffset(int32_t get_offset) = 0;

  // Get the transfer buffer associated with an ID. Returns a null buffer for
  // ID 0.
  virtual scoped_refptr<gpu::Buffer> GetTransferBuffer(int32_t id) = 0;

  // Allows the reader to update the current token value.
  virtual void SetToken(int32_t token) = 0;

  // Allows the reader to set the current parse error.
  virtual void SetParseError(error::Error) = 0;

  // Allows the reader to set the current context lost reason.
  // NOTE: if calling this in conjunction with SetParseError,
  // call this first.
  virtual void SetContextLostReason(error::ContextLostReason) = 0;

  // Allows the reader to obtain the current put offset.
  virtual int32_t GetPutOffset() = 0;
};

// An object that implements a shared memory command buffer and a synchronous
// API to manage the put and get pointers.
class GPU_EXPORT CommandBufferService : public CommandBufferServiceBase {
 public:
  typedef base::Callback<bool(int32_t)> GetBufferChangedCallback;
  explicit CommandBufferService(
      TransferBufferManagerInterface* transfer_buffer_manager);
  ~CommandBufferService() override;

  // CommandBuffer implementation:
  State GetLastState() override;
  int32_t GetLastToken() override;
  void Flush(int32_t put_offset) override;
  void OrderingBarrier(int32_t put_offset) override;
  void WaitForTokenInRange(int32_t start, int32_t end) override;
  void WaitForGetOffsetInRange(int32_t start, int32_t end) override;
  void SetGetBuffer(int32_t transfer_buffer_id) override;
  scoped_refptr<Buffer> CreateTransferBuffer(size_t size, int32_t* id) override;
  void DestroyTransferBuffer(int32_t id) override;

  // CommandBufferServiceBase implementation:
  void SetGetOffset(int32_t get_offset) override;
  scoped_refptr<Buffer> GetTransferBuffer(int32_t id) override;
  void SetToken(int32_t token) override;
  void SetParseError(error::Error error) override;
  void SetContextLostReason(error::ContextLostReason) override;
  int32_t GetPutOffset() override;

  // Sets a callback that is called whenever the put offset is changed. When
  // called with sync==true, the callback must not return until some progress
  // has been made (unless the command buffer is empty), i.e. the get offset
  // must have changed. It need not process the entire command buffer though.
  // This allows concurrency between the writer and the reader while giving the
  // writer a means of waiting for the reader to make some progress before
  // attempting to write more to the command buffer. Takes ownership of
  // callback.
  virtual void SetPutOffsetChangeCallback(const base::Closure& callback);
  // Sets a callback that is called whenever the get buffer is changed.
  virtual void SetGetBufferChangeCallback(
      const GetBufferChangedCallback& callback);
  virtual void SetParseErrorCallback(const base::Closure& callback);

  // Setup the shared memory that shared state should be copied into.
  void SetSharedStateBuffer(std::unique_ptr<BufferBacking> shared_state_buffer);

  // Copy the current state into the shared state transfer buffer.
  void UpdateState();

  // Registers an existing shared memory object and get an ID that can be used
  // to identify it in the command buffer.
  bool RegisterTransferBuffer(int32_t id,
                              std::unique_ptr<BufferBacking> buffer);

 private:
  int32_t ring_buffer_id_;
  scoped_refptr<Buffer> ring_buffer_;
  std::unique_ptr<BufferBacking> shared_state_buffer_;
  CommandBufferSharedState* shared_state_;
  int32_t num_entries_;
  int32_t get_offset_;
  int32_t put_offset_;
  base::Closure put_offset_change_callback_;
  GetBufferChangedCallback get_buffer_change_callback_;
  base::Closure parse_error_callback_;
  scoped_refptr<TransferBufferManagerInterface> transfer_buffer_manager_;
  int32_t token_;
  uint32_t generation_;
  error::Error error_;
  error::ContextLostReason context_lost_reason_;

  DISALLOW_COPY_AND_ASSIGN(CommandBufferService);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_COMMAND_BUFFER_SERVICE_H_
