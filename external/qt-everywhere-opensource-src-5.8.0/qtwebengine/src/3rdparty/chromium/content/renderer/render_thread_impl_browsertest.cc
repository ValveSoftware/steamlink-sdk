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
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "content/app/mojo/mojo_init.h"
#include "content/common/in_process_child_thread_params.h"
#include "content/common/resource_messages.h"
#include "content/common/websocket_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/render_process_impl.h"
#include "content/test/mock_render_process.h"
#include "content/test/render_thread_impl_browser_test_ipc_helper.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gtest/include/gtest/gtest.h"

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
      std::unique_ptr<scheduler::RendererScheduler> scheduler,
      scoped_refptr<base::SingleThreadTaskRunner>& test_task_counter)
      : RenderThreadImpl(params, std::move(scheduler), test_task_counter) {}

  ~RenderThreadImplForTest() override {}

  using ChildThreadImpl::OnMessageReceived;

 private:
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
    content_client_.reset(new ContentClient());
    content_browser_client_.reset(new ContentBrowserClient());
    content_renderer_client_.reset(new ContentRendererClient());
    SetContentClient(content_client_.get());
    SetBrowserClientForTesting(content_browser_client_.get());
    SetRendererClientForTesting(content_renderer_client_.get());

    test_helper_.reset(new RenderThreadImplBrowserIPCTestHelper());

    mock_process_.reset(new MockRenderProcess);
    test_task_counter_ = make_scoped_refptr(new TestTaskCounter());

    // RenderThreadImpl expects the browser to pass these flags.
    base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
    base::CommandLine::StringVector old_argv = cmd->argv();

    cmd->AppendSwitchASCII(switches::kNumRasterThreads, "1");
    std::string image_targets;
    for (size_t format = 0;
         format < static_cast<size_t>(gfx::BufferFormat::LAST) + 1; format++) {
      if (!image_targets.empty())
        image_targets += ",";
      image_targets += base::UintToString(GL_TEXTURE_2D);
    }
    cmd->AppendSwitchASCII(switches::kContentImageTextureTarget, image_targets);

    std::unique_ptr<scheduler::RendererScheduler> renderer_scheduler =
        scheduler::RendererScheduler::Create();
    InitializeMojo();
    scoped_refptr<base::SingleThreadTaskRunner> test_task_counter(
        test_task_counter_.get());
    thread_ = new RenderThreadImplForTest(
        InProcessChildThreadParams(test_helper_->GetChannelId(),
                                   test_helper_->GetIOTaskRunner(),
                                   test_helper_->GetMojoIpcToken(),
                                   test_helper_->GetMojoApplicationToken()),
        std::move(renderer_scheduler), test_task_counter);
    cmd->InitFromArgv(old_argv);

    test_msg_filter_ = make_scoped_refptr(
        new QuitOnTestMsgFilter(test_helper_->GetMessageLoop()));
    thread_->AddFilter(test_msg_filter_.get());
  }

  scoped_refptr<TestTaskCounter> test_task_counter_;
  std::unique_ptr<ContentClient> content_client_;
  std::unique_ptr<ContentBrowserClient> content_browser_client_;
  std::unique_ptr<ContentRendererClient> content_renderer_client_;
  std::unique_ptr<RenderThreadImplBrowserIPCTestHelper> test_helper_;
  std::unique_ptr<MockRenderProcess> mock_process_;
  scoped_refptr<QuitOnTestMsgFilter> test_msg_filter_;
  RenderThreadImplForTest* thread_;  // Owned by mock_process_.
  std::string channel_id_;
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
  test_helper_->Sender()->Send(new ResourceHostMsg_FollowRedirect(0));
  test_helper_->Sender()->Send(new TestMsg_QuitRunLoop());

  test_helper_->GetMessageLoop()->Run();
  EXPECT_EQ(1, test_task_counter_->NumTasksPosted());
}

// Disabled under LeakSanitizer due to memory leaks.
TEST_F(RenderThreadImplBrowserTest,
       WILL_LEAK(NonResourceDispatchIPCTasksDontGoThroughScheduler)) {
  // NOTE other than not being a resource message, the actual message is
  // unimportant.
  test_helper_->Sender()->Send(new WebSocketMsg_NotifyFailure(1, ""));
  test_helper_->Sender()->Send(new TestMsg_QuitRunLoop());

  test_helper_->GetMessageLoop()->Run();

  EXPECT_EQ(0, test_task_counter_->NumTasksPosted());
}

}  // namespace
}  // namespace content
