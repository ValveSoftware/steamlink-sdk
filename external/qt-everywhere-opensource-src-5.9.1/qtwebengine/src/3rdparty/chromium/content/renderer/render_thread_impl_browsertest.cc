// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_thread_impl.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/memory/discardable_memory.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/output/buffer_to_texture_target_map.h"
#include "content/app/mojo/mojo_init.h"
#include "content/child/child_gpu_memory_buffer_manager.h"
#include "content/common/in_process_child_thread_params.h"
#include "content/common/resource_messages.h"
#include "content/common/service_manager/child_connection.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/public/test/test_service_manager_context.h"
#include "content/renderer/render_process_impl.h"
#include "content/test/mock_render_process.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "ipc/ipc.mojom.h"
#include "ipc/ipc_channel_mojo.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/test/scoped_ipc_support.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/scheduler/renderer/renderer_scheduler.h"
#include "ui/gfx/buffer_format_util.h"

// IPC messages for testing ----------------------------------------------------

// TODO(mdempsky): Fix properly by moving into a separate
// browsertest_message_generator.cc file.
#undef IPC_IPC_MESSAGE_MACROS_H_
#undef IPC_MESSAGE_EXTRA
#define IPC_MESSAGE_IMPL
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_templates_impl.h"

#undef IPC_MESSAGE_START
#define IPC_MESSAGE_START TestMsgStart
IPC_MESSAGE_CONTROL0(TestMsg_QuitRunLoop)

// -----------------------------------------------------------------------------

// These tests leak memory, this macro disables the test when under the
// LeakSanitizer.
#ifdef LEAK_SANITIZER
#define WILL_LEAK(NAME) DISABLED_##NAME
#else
#define WILL_LEAK(NAME) NAME
#endif

namespace content {
namespace {

// FIXME: It would be great if there was a reusable mock SingleThreadTaskRunner
class TestTaskCounter : public base::SingleThreadTaskRunner {
 public:
  TestTaskCounter() : count_(0) {}

  // SingleThreadTaskRunner implementation.
  bool PostDelayedTask(const tracked_objects::Location&,
                       const base::Closure&,
                       base::TimeDelta) override {
    base::AutoLock auto_lock(lock_);
    count_++;
    return true;
  }

  bool PostNonNestableDelayedTask(const tracked_objects::Location&,
                                  const base::Closure&,
                                  base::TimeDelta) override {
    base::AutoLock auto_lock(lock_);
    count_++;
    return true;
  }

  bool RunsTasksOnCurrentThread() const override { return true; }

  int NumTasksPosted() const {
    base::AutoLock auto_lock(lock_);
    return count_;
  }

 private:
  ~TestTaskCounter() override {}

  mutable base::Lock lock_;
  int count_;
};

#if defined(COMPILER_MSVC)
// See explanation for other RenderViewHostImpl which is the same issue.
#pragma warning(push)
#pragma warning(disable: 4250)
#endif

class RenderThreadImplForTest : public RenderThreadImpl {
 public:
  RenderThreadImplForTest(
      const InProcessChildThreadParams& params,
      std::unique_ptr<blink::scheduler::RendererScheduler> scheduler,
      scoped_refptr<base::SingleThreadTaskRunner>& test_task_counter)
      : RenderThreadImpl(params, std::move(scheduler), test_task_counter) {}

  ~RenderThreadImplForTest() override {}
};

class DummyListener : public IPC::Listener {
 public:
  ~DummyListener() override {}

  bool OnMessageReceived(const IPC::Message& message) override { return true; }
};

#if defined(COMPILER_MSVC)
#pragma warning(pop)
#endif

void QuitTask(base::MessageLoop* message_loop) {
  message_loop->QuitWhenIdle();
}

class QuitOnTestMsgFilter : public IPC::MessageFilter {
 public:
  explicit QuitOnTestMsgFilter(base::MessageLoop* message_loop)
      : message_loop_(message_loop) {}

  // IPC::MessageFilter overrides:
  bool OnMessageReceived(const IPC::Message& message) override {
    message_loop_->task_runner()->PostTask(
        FROM_HERE, base::Bind(&QuitTask, message_loop_));
    return true;
  }

  bool GetSupportedMessageClasses(
      std::vector<uint32_t>* supported_message_classes) const override {
    supported_message_classes->push_back(TestMsgStart);
    return true;
  }

 private:
  ~QuitOnTestMsgFilter() override {}

  base::MessageLoop* message_loop_;
};

class RenderThreadImplBrowserTest : public testing::Test {
 public:
  void SetUp() override {
    content_renderer_client_.reset(new ContentRendererClient());
    SetRendererClientForTesting(content_renderer_client_.get());

    browser_threads_.reset(
        new TestBrowserThreadBundle(TestBrowserThreadBundle::IO_MAINLOOP));
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
        BrowserThread::GetTaskRunnerForThread(BrowserThread::IO);

    InitializeMojo();
    ipc_support_.reset(new mojo::edk::test::ScopedIPCSupport(io_task_runner));
    shell_context_.reset(new TestServiceManagerContext);
    child_connection_.reset(new ChildConnection(
        mojom::kRendererServiceName, "test", mojo::edk::GenerateRandomToken(),
        ServiceManagerConnection::GetForProcess()->GetConnector(),
        io_task_runner));

    mojo::MessagePipe pipe;
    IPC::mojom::ChannelBootstrapPtr channel_bootstrap;
    child_connection_->GetRemoteInterfaces()->GetInterface(&channel_bootstrap);

    dummy_listener_.reset(new DummyListener);
    channel_ = IPC::ChannelProxy::Create(
        IPC::ChannelMojo::CreateServerFactory(
            channel_bootstrap.PassInterface().PassHandle(), io_task_runner),
        dummy_listener_.get(), io_task_runner);

    mock_process_.reset(new MockRenderProcess);
    test_task_counter_ = make_scoped_refptr(new TestTaskCounter());

    // RenderThreadImpl expects the browser to pass these flags.
    base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
    base::CommandLine::StringVector old_argv = cmd->argv();

    cmd->AppendSwitchASCII(switches::kNumRasterThreads, "1");
    cmd->AppendSwitchASCII(
        switches::kContentImageTextureTarget,
        cc::BufferToTextureTargetMapToString(
            cc::DefaultBufferToTextureTargetMapForTesting()));

    std::unique_ptr<blink::scheduler::RendererScheduler> renderer_scheduler =
        blink::scheduler::RendererScheduler::Create();
    scoped_refptr<base::SingleThreadTaskRunner> test_task_counter(
        test_task_counter_.get());
    thread_ = new RenderThreadImplForTest(
        InProcessChildThreadParams(io_task_runner,
                                   child_connection_->service_token()),
        std::move(renderer_scheduler), test_task_counter);
    cmd->InitFromArgv(old_argv);

    test_msg_filter_ = make_scoped_refptr(
        new QuitOnTestMsgFilter(base::MessageLoop::current()));
    thread_->AddFilter(test_msg_filter_.get());
  }

  IPC::Sender* sender() { return channel_.get(); }

  scoped_refptr<TestTaskCounter> test_task_counter_;
  TestContentClientInitializer content_client_initializer_;
  std::unique_ptr<ContentRendererClient> content_renderer_client_;

  std::unique_ptr<TestBrowserThreadBundle> browser_threads_;
  std::unique_ptr<mojo::edk::test::ScopedIPCSupport> ipc_support_;
  std::unique_ptr<TestServiceManagerContext> shell_context_;
  std::unique_ptr<ChildConnection> child_connection_;
  std::unique_ptr<DummyListener> dummy_listener_;
  std::unique_ptr<IPC::ChannelProxy> channel_;

  std::unique_ptr<MockRenderProcess> mock_process_;
  scoped_refptr<QuitOnTestMsgFilter> test_msg_filter_;
  RenderThreadImplForTest* thread_;  // Owned by mock_process_.
};

void CheckRenderThreadInputHandlerManager(RenderThreadImpl* thread) {
  ASSERT_TRUE(thread->input_handler_manager());
}

// Check that InputHandlerManager outlives compositor thread because it uses
// raw pointers to post tasks.
// Disabled under LeakSanitizer due to memory leaks. http://crbug.com/348994
TEST_F(RenderThreadImplBrowserTest,
       WILL_LEAK(InputHandlerManagerDestroyedAfterCompositorThread)) {
  ASSERT_TRUE(thread_->input_handler_manager());

  thread_->compositor_task_runner()->PostTask(
      FROM_HERE, base::Bind(&CheckRenderThreadInputHandlerManager, thread_));
}

// Disabled under LeakSanitizer due to memory leaks.
TEST_F(RenderThreadImplBrowserTest,
       WILL_LEAK(ResourceDispatchIPCTasksGoThroughScheduler)) {
  sender()->Send(new ResourceHostMsg_FollowRedirect(0));
  sender()->Send(new TestMsg_QuitRunLoop());

  base::RunLoop().Run();
  EXPECT_EQ(1, test_task_counter_->NumTasksPosted());
}

// Disabled under LeakSanitizer due to memory leaks.
TEST_F(RenderThreadImplBrowserTest,
       WILL_LEAK(NonResourceDispatchIPCTasksDontGoThroughScheduler)) {
  // NOTE other than not being a resource message, the actual message is
  // unimportant.

  sender()->Send(new TestMsg_QuitRunLoop());

  base::RunLoop().Run();

  EXPECT_EQ(0, test_task_counter_->NumTasksPosted());
}

enum NativeBufferFlag { kDisableNativeBuffers, kEnableNativeBuffers };

class RenderThreadImplGpuMemoryBufferBrowserTest
    : public ContentBrowserTest,
      public testing::WithParamInterface<
          ::testing::tuple<NativeBufferFlag, gfx::BufferFormat>> {
 public:
  RenderThreadImplGpuMemoryBufferBrowserTest() {}
  ~RenderThreadImplGpuMemoryBufferBrowserTest() override {}

  gpu::GpuMemoryBufferManager* memory_buffer_manager() {
    return memory_buffer_manager_;
  }

 private:
  void SetUpOnRenderThread() {
    memory_buffer_manager_ =
        RenderThreadImpl::current()->GetGpuMemoryBufferManager();
  }

  // Overridden from BrowserTestBase:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kSingleProcess);
    NativeBufferFlag native_buffer_flag = ::testing::get<0>(GetParam());
    switch (native_buffer_flag) {
      case kEnableNativeBuffers:
        command_line->AppendSwitch(switches::kEnableNativeGpuMemoryBuffers);
        break;
      case kDisableNativeBuffers:
        command_line->AppendSwitch(switches::kDisableNativeGpuMemoryBuffers);
        break;
    }
  }

  void SetUpOnMainThread() override {
    NavigateToURL(shell(), GURL(url::kAboutBlankURL));
    PostTaskToInProcessRendererAndWait(base::Bind(
        &RenderThreadImplGpuMemoryBufferBrowserTest::SetUpOnRenderThread,
        base::Unretained(this)));
  }

  gpu::GpuMemoryBufferManager* memory_buffer_manager_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(RenderThreadImplGpuMemoryBufferBrowserTest);
};

// https://crbug.com/652531
IN_PROC_BROWSER_TEST_P(RenderThreadImplGpuMemoryBufferBrowserTest,
                       DISABLED_Map) {
  gfx::BufferFormat format = ::testing::get<1>(GetParam());
  gfx::Size buffer_size(4, 4);

  std::unique_ptr<gfx::GpuMemoryBuffer> buffer =
      memory_buffer_manager()->AllocateGpuMemoryBuffer(
          buffer_size, format, gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
          gpu::kNullSurfaceHandle);
  ASSERT_TRUE(buffer);
  EXPECT_EQ(format, buffer->GetFormat());

  // Map buffer planes.
  ASSERT_TRUE(buffer->Map());

  // Write to buffer and check result.
  size_t num_planes = gfx::NumberOfPlanesForBufferFormat(format);
  for (size_t plane = 0; plane < num_planes; ++plane) {
    ASSERT_TRUE(buffer->memory(plane));
    ASSERT_TRUE(buffer->stride(plane));
    size_t row_size_in_bytes =
        gfx::RowSizeForBufferFormat(buffer_size.width(), format, plane);
    EXPECT_GT(row_size_in_bytes, 0u);

    std::unique_ptr<char[]> data(new char[row_size_in_bytes]);
    memset(data.get(), 0x2a + plane, row_size_in_bytes);
    size_t height = buffer_size.height() /
                    gfx::SubsamplingFactorForBufferFormat(format, plane);
    for (size_t y = 0; y < height; ++y) {
      // Copy |data| to row |y| of |plane| and verify result.
      memcpy(
          static_cast<char*>(buffer->memory(plane)) + y * buffer->stride(plane),
          data.get(), row_size_in_bytes);
      EXPECT_EQ(0, memcmp(static_cast<char*>(buffer->memory(plane)) +
                              y * buffer->stride(plane),
                          data.get(), row_size_in_bytes));
    }
  }

  buffer->Unmap();
}

INSTANTIATE_TEST_CASE_P(
    RenderThreadImplGpuMemoryBufferBrowserTests,
    RenderThreadImplGpuMemoryBufferBrowserTest,
    ::testing::Combine(::testing::Values(kDisableNativeBuffers,
                                         kEnableNativeBuffers),
                       // These formats are guaranteed to work on all platforms.
                       ::testing::Values(gfx::BufferFormat::R_8,
                                         gfx::BufferFormat::BGR_565,
                                         gfx::BufferFormat::RGBA_4444,
                                         gfx::BufferFormat::RGBA_8888,
                                         gfx::BufferFormat::BGRA_8888,
                                         gfx::BufferFormat::YVU_420)));

}  // namespace
}  // namespace content
