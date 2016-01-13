// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/render_process_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/test/mock_render_process.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class RenderThreadImplBrowserTest : public testing::Test {
 public:
  virtual ~RenderThreadImplBrowserTest() {}
};

class DummyListener : public IPC::Listener {
 public:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    return true;
  }
};

void CheckRenderThreadInputHandlerManager(RenderThreadImpl* thread) {
  ASSERT_TRUE(thread->input_handler_manager());
}

// Check that InputHandlerManager outlives compositor thread because it uses
// raw pointers to post tasks.
// Disabled under LeakSanitizer due to memory leaks. http://crbug.com/348994
#if defined(LEAK_SANITIZER)
#define MAYBE_InputHandlerManagerDestroyedAfterCompositorThread \
  DISABLED_InputHandlerManagerDestroyedAfterCompositorThread
#else
#define MAYBE_InputHandlerManagerDestroyedAfterCompositorThread \
  InputHandlerManagerDestroyedAfterCompositorThread
#endif
TEST_F(RenderThreadImplBrowserTest,
    MAYBE_InputHandlerManagerDestroyedAfterCompositorThread) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableThreadedCompositing);

  ContentClient content_client;
  ContentBrowserClient content_browser_client;
  ContentRendererClient content_renderer_client;
  SetContentClient(&content_client);
  SetBrowserClientForTesting(&content_browser_client);
  SetRendererClientForTesting(&content_renderer_client);
  base::MessageLoopForIO message_loop_;

  std::string channel_id = IPC::Channel::GenerateVerifiedChannelID(
      std::string());
  DummyListener dummy_listener;
  scoped_ptr<IPC::Channel> channel(
      IPC::Channel::CreateServer(channel_id, &dummy_listener));
  ASSERT_TRUE(channel->Connect());

  scoped_ptr<MockRenderProcess> mock_process(new MockRenderProcess);
  // Owned by mock_process.
  RenderThreadImpl* thread = new RenderThreadImpl(channel_id);
  thread->EnsureWebKitInitialized();

  ASSERT_TRUE(thread->input_handler_manager());

  thread->compositor_message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&CheckRenderThreadInputHandlerManager, thread));
}

}  // namespace
}  // namespace content
