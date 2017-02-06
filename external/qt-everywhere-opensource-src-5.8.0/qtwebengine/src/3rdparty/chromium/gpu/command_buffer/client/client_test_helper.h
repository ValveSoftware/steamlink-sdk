// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper classes for implementing gpu client side unit tests.

#ifndef GPU_COMMAND_BUFFER_CLIENT_CLIENT_TEST_HELPER_H_
#define GPU_COMMAND_BUFFER_CLIENT_CLIENT_TEST_HELPER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/common/cmd_buffer_common.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class CommandBufferHelper;

class MockCommandBufferBase : public CommandBufferServiceBase {
 public:
  static const int32_t kTransferBufferBaseId = 0x123;
  static const int32_t kMaxTransferBuffers = 32;

  MockCommandBufferBase();
  ~MockCommandBufferBase() override;

  State GetLastState() override;
  int32_t GetLastToken() override;
  void WaitForTokenInRange(int32_t start, int32_t end) override;
  void WaitForGetOffsetInRange(int32_t start, int32_t end) override;
  void SetGetBuffer(int transfer_buffer_id) override;
  void SetGetOffset(int32_t get_offset) override;
  scoped_refptr<gpu::Buffer> CreateTransferBuffer(size_t size,
                                                  int32_t* id) override;
  scoped_refptr<gpu::Buffer> GetTransferBuffer(int32_t id) override;
  void SetToken(int32_t token) override;
  void SetParseError(error::Error error) override;
  void SetContextLostReason(error::ContextLostReason reason) override;
  int32_t GetPutOffset() override;

  // Get's the Id of the next transfer buffer that will be returned
  // by CreateTransferBuffer. This is useful for testing expected ids.
  int32_t GetNextFreeTransferBufferId();

  void FlushHelper(int32_t put_offset);
  void DestroyTransferBufferHelper(int32_t id);

  virtual void OnFlush() = 0;

 private:
  scoped_refptr<Buffer> transfer_buffer_buffers_[kMaxTransferBuffers];
  CommandBufferEntry* ring_buffer_;
  scoped_refptr<Buffer> ring_buffer_buffer_;
  State state_;
  int32_t put_offset_;
};

class MockClientCommandBuffer : public MockCommandBufferBase {
 public:
  MockClientCommandBuffer();
  ~MockClientCommandBuffer() override;

  // This is so we can use all the gmock functions when Flush is called.
  MOCK_METHOD0(OnFlush, void());
  MOCK_METHOD1(DestroyTransferBuffer, void(int32_t id));

  void Flush(int32_t put_offset) override;
  void OrderingBarrier(int32_t put_offset) override;

  void DelegateToFake();
};

class MockClientCommandBufferMockFlush : public MockClientCommandBuffer {
 public:
  MockClientCommandBufferMockFlush();
  virtual ~MockClientCommandBufferMockFlush();

  MOCK_METHOD1(Flush, void(int32_t put_offset));
  MOCK_METHOD1(OrderingBarrier, void(int32_t put_offset));

  void DelegateToFake();
};

class MockClientGpuControl : public GpuControl {
 public:
  MockClientGpuControl();
  virtual ~MockClientGpuControl();

  MOCK_METHOD1(SetGpuControlClient, void(GpuControlClient*));
  MOCK_METHOD0(GetCapabilities, Capabilities());
  MOCK_METHOD4(CreateImage,
               int32_t(ClientBuffer buffer,
                       size_t width,
                       size_t height,
                       unsigned internalformat));
  MOCK_METHOD1(DestroyImage, void(int32_t id));
  MOCK_METHOD4(CreateGpuMemoryBufferImage,
               int32_t(size_t width,
                       size_t height,
                       unsigned internalformat,
                       unsigned usage));
  MOCK_METHOD1(GetImageGpuMemoryBufferId, int32_t(unsigned image_id));
  MOCK_METHOD2(SignalQuery,
               void(uint32_t query, const base::Closure& callback));
  MOCK_METHOD1(CreateStreamTexture, uint32_t(uint32_t));
  MOCK_METHOD1(SetLock, void(base::Lock*));
  MOCK_METHOD0(EnsureWorkVisible, void());
  MOCK_CONST_METHOD0(GetNamespaceID, CommandBufferNamespace());
  MOCK_CONST_METHOD0(GetCommandBufferID, CommandBufferId());
  MOCK_CONST_METHOD0(GetExtraCommandBufferData, int32_t());
  MOCK_METHOD0(GenerateFenceSyncRelease, uint64_t());
  MOCK_METHOD1(IsFenceSyncRelease, bool(uint64_t release));
  MOCK_METHOD1(IsFenceSyncFlushed, bool(uint64_t release));
  MOCK_METHOD1(IsFenceSyncFlushReceived, bool(uint64_t release));
  MOCK_METHOD2(SignalSyncToken, void(const SyncToken& sync_token,
                                     const base::Closure& callback));
  MOCK_METHOD1(CanWaitUnverifiedSyncToken, bool(const SyncToken*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockClientGpuControl);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_CLIENT_TEST_HELPER_H_

