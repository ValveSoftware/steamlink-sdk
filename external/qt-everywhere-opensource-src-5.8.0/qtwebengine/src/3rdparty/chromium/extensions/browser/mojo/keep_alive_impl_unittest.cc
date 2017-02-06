// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mojo/keep_alive_impl.h"

#include <utility>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension_builder.h"

namespace extensions {

class KeepAliveTest : public ExtensionsTest {
 public:
  KeepAliveTest()
      : notification_service_(content::NotificationService::Create()) {}
  ~KeepAliveTest() override {}

  void SetUp() override {
    ExtensionsTest::SetUp();
    message_loop_.reset(new base::MessageLoop);
    extension_ =
        ExtensionBuilder()
            .SetManifest(
                DictionaryBuilder()
                    .Set("name", "app")
                    .Set("version", "1")
                    .Set("manifest_version", 2)
                    .Set("app", DictionaryBuilder()
                                    .Set("background",
                                         DictionaryBuilder()
                                             .Set("scripts",
                                                  ListBuilder()
                                                      .Append("background.js")
                                                      .Build())
                                             .Build())
                                    .Build())
                    .Build())
            .SetID("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")
            .Build();
  }

  void TearDown() override {
    message_loop_.reset();
    ExtensionsTest::TearDown();
  }

  void WaitUntilLazyKeepAliveChanges() {
    int initial_keep_alive_count = GetKeepAliveCount();
    while (GetKeepAliveCount() == initial_keep_alive_count) {
      base::RunLoop().RunUntilIdle();
    }
  }

  void CreateKeepAlive(mojo::InterfaceRequest<KeepAlive> request) {
    KeepAliveImpl::Create(browser_context(), extension_.get(),
                          std::move(request));
  }

  const Extension* extension() { return extension_.get(); }

  int GetKeepAliveCount() {
    return ProcessManager::Get(browser_context())
        ->GetLazyKeepaliveCount(extension());
  }

 private:
  std::unique_ptr<base::MessageLoop> message_loop_;
  std::unique_ptr<content::NotificationService> notification_service_;
  scoped_refptr<const Extension> extension_;

  DISALLOW_COPY_AND_ASSIGN(KeepAliveTest);
};

TEST_F(KeepAliveTest, Basic) {
  mojo::InterfacePtr<KeepAlive> keep_alive;
  CreateKeepAlive(mojo::GetProxy(&keep_alive));
  EXPECT_EQ(1, GetKeepAliveCount());

  keep_alive.reset();
  WaitUntilLazyKeepAliveChanges();
  EXPECT_EQ(0, GetKeepAliveCount());
}

TEST_F(KeepAliveTest, TwoKeepAlives) {
  mojo::InterfacePtr<KeepAlive> keep_alive;
  CreateKeepAlive(mojo::GetProxy(&keep_alive));
  EXPECT_EQ(1, GetKeepAliveCount());

  mojo::InterfacePtr<KeepAlive> other_keep_alive;
  CreateKeepAlive(mojo::GetProxy(&other_keep_alive));
  EXPECT_EQ(2, GetKeepAliveCount());

  keep_alive.reset();
  WaitUntilLazyKeepAliveChanges();
  EXPECT_EQ(1, GetKeepAliveCount());

  other_keep_alive.reset();
  WaitUntilLazyKeepAliveChanges();
  EXPECT_EQ(0, GetKeepAliveCount());
}

TEST_F(KeepAliveTest, UnloadExtension) {
  mojo::InterfacePtr<KeepAlive> keep_alive;
  CreateKeepAlive(mojo::GetProxy(&keep_alive));
  EXPECT_EQ(1, GetKeepAliveCount());

  scoped_refptr<const Extension> other_extension =
      ExtensionBuilder()
          .SetManifest(
              DictionaryBuilder()
                  .Set("name", "app")
                  .Set("version", "1")
                  .Set("manifest_version", 2)
                  .Set("app",
                       DictionaryBuilder()
                           .Set("background",
                                DictionaryBuilder()
                                    .Set("scripts", ListBuilder()
                                                        .Append("background.js")
                                                        .Build())
                                    .Build())
                           .Build())
                  .Build())
          .SetID("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")
          .Build();

  ExtensionRegistry::Get(browser_context())
      ->TriggerOnUnloaded(other_extension.get(),
                          UnloadedExtensionInfo::REASON_DISABLE);
  EXPECT_EQ(1, GetKeepAliveCount());

  ExtensionRegistry::Get(browser_context())
      ->TriggerOnUnloaded(extension(), UnloadedExtensionInfo::REASON_DISABLE);
  // When its extension is unloaded, the KeepAliveImpl should not modify the
  // keep-alive count for its extension. However, ProcessManager resets its
  // keep-alive count for an unloaded extension.
  EXPECT_EQ(0, GetKeepAliveCount());

  // Wait for |keep_alive| to disconnect.
  base::RunLoop run_loop;
  keep_alive.set_connection_error_handler(run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(KeepAliveTest, Shutdown) {
  mojo::InterfacePtr<KeepAlive> keep_alive;
  CreateKeepAlive(mojo::GetProxy(&keep_alive));
  EXPECT_EQ(1, GetKeepAliveCount());

  ExtensionRegistry::Get(browser_context())->Shutdown();
  // After a shutdown event, the KeepAliveImpl should not access its
  // ProcessManager and so the keep-alive count should remain unchanged.
  EXPECT_EQ(1, GetKeepAliveCount());

  // Wait for |keep_alive| to disconnect.
  base::RunLoop run_loop;
  keep_alive.set_connection_error_handler(run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace extensions
