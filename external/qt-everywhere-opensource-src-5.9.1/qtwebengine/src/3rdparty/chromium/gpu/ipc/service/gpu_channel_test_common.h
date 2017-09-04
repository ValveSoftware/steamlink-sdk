// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "gpu/command_buffer/service/gpu_preferences.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gtest/include/gtest/gtest.h"

class GURL;

namespace base {
class TestSimpleTaskRunner;
}  // namespace base

namespace IPC {
class TestSink;
}  // namespace IPC

namespace gpu {

class SyncPointManager;

class TestGpuChannelManagerDelegate : public GpuChannelManagerDelegate {
 public:
  TestGpuChannelManagerDelegate();
  ~TestGpuChannelManagerDelegate() override;

 private:
  // GpuChannelManagerDelegate implementation:
  void SetActiveURL(const GURL& url) override;
  void DidCreateOffscreenContext(const GURL& active_url) override;
  void DidDestroyChannel(int client_id) override;
  void DidDestroyOffscreenContext(const GURL& active_url) override;
  void DidLoseContext(bool offscreen,
                      error::ContextLostReason reason,
                      const GURL& active_url) override;
  void StoreShaderToDisk(int32_t client_id,
                         const std::string& key,
                         const std::string& shader) override;
#if defined(OS_WIN)
  void SendAcceleratedSurfaceCreatedChildWindow(
      SurfaceHandle parent_window,
      SurfaceHandle child_window) override;
#endif

  DISALLOW_COPY_AND_ASSIGN(TestGpuChannelManagerDelegate);
};

class TestGpuChannelManager : public GpuChannelManager {
 public:
  TestGpuChannelManager(const GpuPreferences& gpu_preferences,
                        GpuChannelManagerDelegate* delegate,
                        base::SingleThreadTaskRunner* task_runner,
                        base::SingleThreadTaskRunner* io_task_runner,
                        SyncPointManager* sync_point_manager,
                        GpuMemoryBufferFactory* gpu_memory_buffer_factory);
  ~TestGpuChannelManager() override;

 protected:
  std::unique_ptr<GpuChannel> CreateGpuChannel(
      int client_id,
      uint64_t client_tracing_id,
      bool preempts,
      bool allow_view_command_buffers,
      bool allow_real_time_streams) override;
};

class TestGpuChannel : public GpuChannel {
 public:
  TestGpuChannel(GpuChannelManager* gpu_channel_manager,
                 SyncPointManager* sync_point_manager,
                 gl::GLShareGroup* share_group,
                 gles2::MailboxManager* mailbox_manager,
                 PreemptionFlag* preempting_flag,
                 PreemptionFlag* preempted_flag,
                 base::SingleThreadTaskRunner* task_runner,
                 base::SingleThreadTaskRunner* io_task_runner,
                 int client_id,
                 uint64_t client_tracing_id,
                 bool allow_view_command_buffers,
                 bool allow_real_time_streams);
  ~TestGpuChannel() override;

  IPC::TestSink* sink() { return &sink_; }
  base::ProcessId GetClientPID() const override;

  IPC::ChannelHandle Init(base::WaitableEvent* shutdown_event) override;

  // IPC::Sender implementation.
  bool Send(IPC::Message* msg) override;

 private:
  IPC::TestSink sink_;
};

class GpuChannelTestCommon : public testing::Test {
 public:
  GpuChannelTestCommon();
  ~GpuChannelTestCommon() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  GpuChannelManager* channel_manager() { return channel_manager_.get(); }
  TestGpuChannelManagerDelegate* channel_manager_delegate() {
    return channel_manager_delegate_.get();
  }
  base::TestSimpleTaskRunner* task_runner() { return task_runner_.get(); }

 private:
  GpuPreferences gpu_preferences_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  scoped_refptr<base::TestSimpleTaskRunner> io_task_runner_;
  std::unique_ptr<SyncPointManager> sync_point_manager_;
  std::unique_ptr<TestGpuChannelManagerDelegate> channel_manager_delegate_;
  std::unique_ptr<GpuChannelManager> channel_manager_;
};

}  // namespace gpu
