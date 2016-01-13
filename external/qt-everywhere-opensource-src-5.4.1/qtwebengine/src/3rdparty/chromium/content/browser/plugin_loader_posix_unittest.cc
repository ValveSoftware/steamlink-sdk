// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/plugin_loader_posix.h"

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/browser_thread_impl.h"
#include "content/common/plugin_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace content {

class MockPluginLoaderPosix : public PluginLoaderPosix {
 public:
  MOCK_METHOD0(LoadPluginsInternal, void(void));

  size_t number_of_pending_callbacks() {
    return callbacks_.size();
  }

  std::vector<base::FilePath>* canonical_list() {
    return &canonical_list_;
  }

  size_t next_load_index() {
    return next_load_index_;
  }

  const std::vector<WebPluginInfo>& loaded_plugins() {
    return loaded_plugins_;
  }

  std::vector<WebPluginInfo>* internal_plugins() {
    return &internal_plugins_;
  }

  void RealLoadPluginsInternal() {
    PluginLoaderPosix::LoadPluginsInternal();
  }

  void TestOnPluginLoaded(uint32 index, const WebPluginInfo& plugin) {
    OnPluginLoaded(index, plugin);
  }

  void TestOnPluginLoadFailed(uint32 index, const base::FilePath& path) {
    OnPluginLoadFailed(index, path);
  }

 protected:
  virtual ~MockPluginLoaderPosix() {}
};

void VerifyCallback(int* run_count, const std::vector<WebPluginInfo>&) {
  ++(*run_count);
}

class PluginLoaderPosixTest : public testing::Test {
 public:
  PluginLoaderPosixTest()
      : plugin1_(ASCIIToUTF16("plugin1"), base::FilePath("/tmp/one.plugin"),
                 ASCIIToUTF16("1.0"), base::string16()),
        plugin2_(ASCIIToUTF16("plugin2"), base::FilePath("/tmp/two.plugin"),
                 ASCIIToUTF16("2.0"), base::string16()),
        plugin3_(ASCIIToUTF16("plugin3"), base::FilePath("/tmp/three.plugin"),
                 ASCIIToUTF16("3.0"), base::string16()),
        file_thread_(BrowserThread::FILE, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_),
        plugin_loader_(new MockPluginLoaderPosix) {
  }

  virtual void SetUp() OVERRIDE {
    PluginServiceImpl::GetInstance()->Init();
  }

  base::MessageLoop* message_loop() { return &message_loop_; }
  MockPluginLoaderPosix* plugin_loader() { return plugin_loader_.get(); }

  void AddThreePlugins() {
    plugin_loader_->canonical_list()->clear();
    plugin_loader_->canonical_list()->push_back(plugin1_.path);
    plugin_loader_->canonical_list()->push_back(plugin2_.path);
    plugin_loader_->canonical_list()->push_back(plugin3_.path);
  }

  // Data used for testing.
  WebPluginInfo plugin1_;
  WebPluginInfo plugin2_;
  WebPluginInfo plugin3_;

 private:
  // Destroys PluginService and PluginList.
  base::ShadowingAtExitManager at_exit_manager_;

  base::MessageLoopForIO message_loop_;
  BrowserThreadImpl file_thread_;
  BrowserThreadImpl io_thread_;

  scoped_refptr<MockPluginLoaderPosix> plugin_loader_;
};

TEST_F(PluginLoaderPosixTest, QueueRequests) {
  int did_callback = 0;
  PluginService::GetPluginsCallback callback =
      base::Bind(&VerifyCallback, base::Unretained(&did_callback));

  plugin_loader()->GetPlugins(callback);
  plugin_loader()->GetPlugins(callback);

  EXPECT_CALL(*plugin_loader(), LoadPluginsInternal()).Times(1);
  message_loop()->RunUntilIdle();

  EXPECT_EQ(0, did_callback);

  plugin_loader()->canonical_list()->clear();
  plugin_loader()->canonical_list()->push_back(plugin1_.path);
  plugin_loader()->TestOnPluginLoaded(0, plugin1_);
  message_loop()->RunUntilIdle();

  EXPECT_EQ(2, did_callback);
}

TEST_F(PluginLoaderPosixTest, QueueRequestsAndInvalidate) {
  int did_callback = 0;
  PluginService::GetPluginsCallback callback =
      base::Bind(&VerifyCallback, base::Unretained(&did_callback));

  plugin_loader()->GetPlugins(callback);

  EXPECT_CALL(*plugin_loader(), LoadPluginsInternal()).Times(1);
  message_loop()->RunUntilIdle();

  EXPECT_EQ(0, did_callback);
  ::testing::Mock::VerifyAndClearExpectations(plugin_loader());

  // Invalidate the plugin list, then queue up another request.
  PluginList::Singleton()->RefreshPlugins();
  plugin_loader()->GetPlugins(callback);

  EXPECT_CALL(*plugin_loader(), LoadPluginsInternal()).Times(1);

  plugin_loader()->canonical_list()->clear();
  plugin_loader()->canonical_list()->push_back(plugin1_.path);
  plugin_loader()->TestOnPluginLoaded(0, plugin1_);
  message_loop()->RunUntilIdle();

  // Only the first request should have been fulfilled.
  EXPECT_EQ(1, did_callback);

  plugin_loader()->canonical_list()->clear();
  plugin_loader()->canonical_list()->push_back(plugin1_.path);
  plugin_loader()->TestOnPluginLoaded(0, plugin1_);
  message_loop()->RunUntilIdle();

  EXPECT_EQ(2, did_callback);
}

TEST_F(PluginLoaderPosixTest, ThreeSuccessfulLoads) {
  int did_callback = 0;
  PluginService::GetPluginsCallback callback =
      base::Bind(&VerifyCallback, base::Unretained(&did_callback));

  plugin_loader()->GetPlugins(callback);

  EXPECT_CALL(*plugin_loader(), LoadPluginsInternal()).Times(1);
  message_loop()->RunUntilIdle();

  AddThreePlugins();

  EXPECT_EQ(0u, plugin_loader()->next_load_index());

  const std::vector<WebPluginInfo>& plugins(plugin_loader()->loaded_plugins());

  plugin_loader()->TestOnPluginLoaded(0, plugin1_);
  EXPECT_EQ(1u, plugin_loader()->next_load_index());
  EXPECT_EQ(1u, plugins.size());
  EXPECT_EQ(plugin1_.name, plugins[0].name);

  message_loop()->RunUntilIdle();
  EXPECT_EQ(0, did_callback);

  plugin_loader()->TestOnPluginLoaded(1, plugin2_);
  EXPECT_EQ(2u, plugin_loader()->next_load_index());
  EXPECT_EQ(2u, plugins.size());
  EXPECT_EQ(plugin2_.name, plugins[1].name);

  message_loop()->RunUntilIdle();
  EXPECT_EQ(0, did_callback);

  plugin_loader()->TestOnPluginLoaded(2, plugin3_);
  EXPECT_EQ(3u, plugins.size());
  EXPECT_EQ(plugin3_.name, plugins[2].name);

  message_loop()->RunUntilIdle();
  EXPECT_EQ(1, did_callback);
}

TEST_F(PluginLoaderPosixTest, ThreeSuccessfulLoadsThenCrash) {
  int did_callback = 0;
  PluginService::GetPluginsCallback callback =
      base::Bind(&VerifyCallback, base::Unretained(&did_callback));

  plugin_loader()->GetPlugins(callback);

  EXPECT_CALL(*plugin_loader(), LoadPluginsInternal()).Times(2);
  message_loop()->RunUntilIdle();

  AddThreePlugins();

  EXPECT_EQ(0u, plugin_loader()->next_load_index());

  const std::vector<WebPluginInfo>& plugins(plugin_loader()->loaded_plugins());

  plugin_loader()->TestOnPluginLoaded(0, plugin1_);
  EXPECT_EQ(1u, plugin_loader()->next_load_index());
  EXPECT_EQ(1u, plugins.size());
  EXPECT_EQ(plugin1_.name, plugins[0].name);

  message_loop()->RunUntilIdle();
  EXPECT_EQ(0, did_callback);

  plugin_loader()->TestOnPluginLoaded(1, plugin2_);
  EXPECT_EQ(2u, plugin_loader()->next_load_index());
  EXPECT_EQ(2u, plugins.size());
  EXPECT_EQ(plugin2_.name, plugins[1].name);

  message_loop()->RunUntilIdle();
  EXPECT_EQ(0, did_callback);

  plugin_loader()->TestOnPluginLoaded(2, plugin3_);
  EXPECT_EQ(3u, plugins.size());
  EXPECT_EQ(plugin3_.name, plugins[2].name);

  message_loop()->RunUntilIdle();
  EXPECT_EQ(1, did_callback);

  plugin_loader()->OnProcessCrashed(42);
}

TEST_F(PluginLoaderPosixTest, TwoFailures) {
  int did_callback = 0;
  PluginService::GetPluginsCallback callback =
      base::Bind(&VerifyCallback, base::Unretained(&did_callback));

  plugin_loader()->GetPlugins(callback);

  EXPECT_CALL(*plugin_loader(), LoadPluginsInternal()).Times(1);
  message_loop()->RunUntilIdle();

  AddThreePlugins();

  EXPECT_EQ(0u, plugin_loader()->next_load_index());

  const std::vector<WebPluginInfo>& plugins(plugin_loader()->loaded_plugins());

  plugin_loader()->TestOnPluginLoadFailed(0, plugin1_.path);
  EXPECT_EQ(1u, plugin_loader()->next_load_index());
  EXPECT_EQ(0u, plugins.size());

  message_loop()->RunUntilIdle();
  EXPECT_EQ(0, did_callback);

  plugin_loader()->TestOnPluginLoaded(1, plugin2_);
  EXPECT_EQ(2u, plugin_loader()->next_load_index());
  EXPECT_EQ(1u, plugins.size());
  EXPECT_EQ(plugin2_.name, plugins[0].name);

  message_loop()->RunUntilIdle();
  EXPECT_EQ(0, did_callback);

  plugin_loader()->TestOnPluginLoadFailed(2, plugin3_.path);
  EXPECT_EQ(1u, plugins.size());

  message_loop()->RunUntilIdle();
  EXPECT_EQ(1, did_callback);
}

TEST_F(PluginLoaderPosixTest, CrashedProcess) {
  int did_callback = 0;
  PluginService::GetPluginsCallback callback =
      base::Bind(&VerifyCallback, base::Unretained(&did_callback));

  plugin_loader()->GetPlugins(callback);

  EXPECT_CALL(*plugin_loader(), LoadPluginsInternal()).Times(1);
  message_loop()->RunUntilIdle();

  AddThreePlugins();

  EXPECT_EQ(0u, plugin_loader()->next_load_index());

  const std::vector<WebPluginInfo>& plugins(plugin_loader()->loaded_plugins());

  plugin_loader()->TestOnPluginLoaded(0, plugin1_);
  EXPECT_EQ(1u, plugin_loader()->next_load_index());
  EXPECT_EQ(1u, plugins.size());
  EXPECT_EQ(plugin1_.name, plugins[0].name);

  message_loop()->RunUntilIdle();
  EXPECT_EQ(0, did_callback);

  EXPECT_CALL(*plugin_loader(), LoadPluginsInternal()).Times(1);
  plugin_loader()->OnProcessCrashed(42);
  EXPECT_EQ(1u, plugin_loader()->canonical_list()->size());
  EXPECT_EQ(0u, plugin_loader()->next_load_index());
  EXPECT_EQ(plugin3_.path.value(),
            plugin_loader()->canonical_list()->at(0).value());
}

TEST_F(PluginLoaderPosixTest, InternalPlugin) {
  int did_callback = 0;
  PluginService::GetPluginsCallback callback =
      base::Bind(&VerifyCallback, base::Unretained(&did_callback));

  plugin_loader()->GetPlugins(callback);

  EXPECT_CALL(*plugin_loader(), LoadPluginsInternal()).Times(1);
  message_loop()->RunUntilIdle();

  plugin2_.path = base::FilePath("/internal/plugin.plugin");

  AddThreePlugins();

  plugin_loader()->internal_plugins()->clear();
  plugin_loader()->internal_plugins()->push_back(plugin2_);

  EXPECT_EQ(0u, plugin_loader()->next_load_index());

  const std::vector<WebPluginInfo>& plugins(plugin_loader()->loaded_plugins());

  plugin_loader()->TestOnPluginLoaded(0, plugin1_);
  EXPECT_EQ(1u, plugin_loader()->next_load_index());
  EXPECT_EQ(1u, plugins.size());
  EXPECT_EQ(plugin1_.name, plugins[0].name);

  message_loop()->RunUntilIdle();
  EXPECT_EQ(0, did_callback);

  // Internal plugins can fail to load if they're built-in with manual
  // entrypoint functions.
  plugin_loader()->TestOnPluginLoadFailed(1, plugin2_.path);
  EXPECT_EQ(2u, plugin_loader()->next_load_index());
  EXPECT_EQ(2u, plugins.size());
  EXPECT_EQ(plugin2_.name, plugins[1].name);
  EXPECT_EQ(0u, plugin_loader()->internal_plugins()->size());

  message_loop()->RunUntilIdle();
  EXPECT_EQ(0, did_callback);

  plugin_loader()->TestOnPluginLoaded(2, plugin3_);
  EXPECT_EQ(3u, plugins.size());
  EXPECT_EQ(plugin3_.name, plugins[2].name);

  message_loop()->RunUntilIdle();
  EXPECT_EQ(1, did_callback);
}

TEST_F(PluginLoaderPosixTest, AllCrashed) {
  int did_callback = 0;
  PluginService::GetPluginsCallback callback =
      base::Bind(&VerifyCallback, base::Unretained(&did_callback));

  plugin_loader()->GetPlugins(callback);

  // Spin the loop so that the canonical list of plugins can be set.
  EXPECT_CALL(*plugin_loader(), LoadPluginsInternal()).Times(1);
  message_loop()->RunUntilIdle();
  AddThreePlugins();

  EXPECT_EQ(0u, plugin_loader()->next_load_index());

  // Mock the first two calls like normal.
  testing::Expectation first =
      EXPECT_CALL(*plugin_loader(), LoadPluginsInternal()).Times(2);
  // On the last call, go through the default impl.
  EXPECT_CALL(*plugin_loader(), LoadPluginsInternal())
      .After(first)
      .WillOnce(
          testing::Invoke(plugin_loader(),
                          &MockPluginLoaderPosix::RealLoadPluginsInternal));
  plugin_loader()->OnProcessCrashed(42);
  plugin_loader()->OnProcessCrashed(42);
  plugin_loader()->OnProcessCrashed(42);

  message_loop()->RunUntilIdle();
  EXPECT_EQ(1, did_callback);

  EXPECT_EQ(0u, plugin_loader()->loaded_plugins().size());
}

}  // namespace content
