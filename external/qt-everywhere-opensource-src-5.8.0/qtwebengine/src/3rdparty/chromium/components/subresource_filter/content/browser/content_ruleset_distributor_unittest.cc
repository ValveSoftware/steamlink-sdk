// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/content_ruleset_distributor.h"

#include <stddef.h>

#include <string>
#include <tuple>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "components/subresource_filter/content/common/subresource_filter_messages.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "ipc/ipc_platform_file.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

namespace {

class NotifyingMockRenderProcessHost : public content::MockRenderProcessHost {
 public:
  explicit NotifyingMockRenderProcessHost(
      content::BrowserContext* browser_context)
      : content::MockRenderProcessHost(browser_context) {
    content::NotificationService::current()->Notify(
        content::NOTIFICATION_RENDERER_PROCESS_CREATED,
        content::Source<content::RenderProcessHost>(this),
        content::NotificationService::NoDetails());
  }
};

std::string ReadFileContents(base::File* file) {
  size_t length = base::checked_cast<size_t>(file->GetLength());
  std::string contents(length, 0);
  file->Read(0, &contents[0], base::checked_cast<int>(length));
  return contents;
}

// Extracts and takes ownership of the ruleset file handle in the IPC message.
base::File ExtractRulesetFromMessage(const IPC::Message* message) {
  std::tuple<IPC::PlatformFileForTransit> arg;
  SubresourceFilterMsg_SetRulesetForProcess::Read(message, &arg);
  return IPC::PlatformFileForTransitToFile(std::get<0>(arg));
}

}  // namespace

class SubresourceFilterContentRulesetDistributorTest : public ::testing::Test {
 public:
  SubresourceFilterContentRulesetDistributorTest()
      : existing_renderer_(&browser_context_) {}

 protected:
  void SetUp() override { ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir()); }

  content::TestBrowserContext* browser_context() { return &browser_context_; }

  base::FilePath scoped_temp_file() const {
    return scoped_temp_dir_.path().AppendASCII("data");
  }

  void AssertSetRulesetForProcessMessageWithContent(
      const IPC::Message* message,
      const std::string& expected_contents) {
    ASSERT_EQ(SubresourceFilterMsg_SetRulesetForProcess::ID, message->type());
    base::File ruleset_file = ExtractRulesetFromMessage(message);
    ASSERT_TRUE(ruleset_file.IsValid());
    ASSERT_EQ(expected_contents, ReadFileContents(&ruleset_file));
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  content::TestBrowserContext browser_context_;
  base::ScopedTempDir scoped_temp_dir_;
  NotifyingMockRenderProcessHost existing_renderer_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterContentRulesetDistributorTest);
};

TEST_F(SubresourceFilterContentRulesetDistributorTest,
       NoRuleset_NoIPCMessages) {
  NotifyingMockRenderProcessHost existing_renderer(browser_context());
  ContentRulesetDistributor distributor;
  NotifyingMockRenderProcessHost new_renderer(browser_context());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, existing_renderer.sink().message_count());
  EXPECT_EQ(0u, new_renderer.sink().message_count());
}

TEST_F(SubresourceFilterContentRulesetDistributorTest,
       PublishedRuleset_IsDistributedToExistingAndNewRenderers) {
  const char kTestFileContents[] = "foobar";
  base::WriteFile(scoped_temp_file(), kTestFileContents,
                  strlen(kTestFileContents));

  base::File file;
  file.Initialize(scoped_temp_file(),
                  base::File::FLAG_OPEN | base::File::FLAG_READ);

  NotifyingMockRenderProcessHost existing_renderer(browser_context());
  ContentRulesetDistributor distributor;
  distributor.PublishNewVersion(std::move(file));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, existing_renderer.sink().message_count());
  ASSERT_NO_FATAL_FAILURE(AssertSetRulesetForProcessMessageWithContent(
      existing_renderer.sink().GetMessageAt(0), kTestFileContents));

  NotifyingMockRenderProcessHost second_renderer(browser_context());
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, second_renderer.sink().message_count());
  ASSERT_NO_FATAL_FAILURE(AssertSetRulesetForProcessMessageWithContent(
      second_renderer.sink().GetMessageAt(0), kTestFileContents));
}

}  // namespace subresource_filter
