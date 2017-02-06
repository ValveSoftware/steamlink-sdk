// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/lazy_background_task_queue.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/testing_pref_service_syncable.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_context.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserContext;

namespace extensions {
namespace {

// A ProcessManager that doesn't create background host pages.
class TestProcessManager : public ProcessManager {
 public:
  explicit TestProcessManager(BrowserContext* context)
      : ProcessManager(context, context, ExtensionRegistry::Get(context)),
        create_count_(0) {
    // ProcessManager constructor above assumes non-incognito.
    DCHECK(!context->IsOffTheRecord());
  }
  ~TestProcessManager() override {}

  int create_count() { return create_count_; }

  // ProcessManager overrides:
  bool CreateBackgroundHost(const Extension* extension,
                            const GURL& url) override {
    // Don't actually try to create a web contents.
    create_count_++;
    return false;
  }

 private:
  int create_count_;

  DISALLOW_COPY_AND_ASSIGN(TestProcessManager);
};

std::unique_ptr<KeyedService> CreateTestProcessManager(
    BrowserContext* context) {
  return base::WrapUnique(new TestProcessManager(context));
}

}  // namespace

// Derives from ExtensionsTest to provide content module and keyed service
// initialization.
class LazyBackgroundTaskQueueTest : public ExtensionsTest {
 public:
  LazyBackgroundTaskQueueTest()
      : notification_service_(content::NotificationService::Create()),
        task_run_count_(0) {
  }
  ~LazyBackgroundTaskQueueTest() override {}

  int task_run_count() { return task_run_count_; }

  // A simple callback for AddPendingTask.
  void RunPendingTask(ExtensionHost* host) {
    task_run_count_++;
  }

  // Creates and registers an extension without a background page.
  scoped_refptr<Extension> CreateSimpleExtension() {
    scoped_refptr<Extension> extension =
        ExtensionBuilder()
            .SetManifest(DictionaryBuilder()
                             .Set("name", "No background")
                             .Set("version", "1")
                             .Set("manifest_version", 2)
                             .Build())
            .SetID("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")
            .Build();
    ExtensionRegistry::Get(browser_context())->AddEnabled(extension);
    return extension;
  }

  // Creates and registers an extension with a lazy background page.
  scoped_refptr<Extension> CreateLazyBackgroundExtension() {
    scoped_refptr<Extension> extension =
        ExtensionBuilder()
            .SetManifest(
                DictionaryBuilder()
                    .Set("name", "Lazy background")
                    .Set("version", "1")
                    .Set("manifest_version", 2)
                    .Set("background", DictionaryBuilder()
                                           .Set("page", "background.html")
                                           .SetBoolean("persistent", false)
                                           .Build())
                    .Build())
            .SetID("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")
            .Build();
    ExtensionRegistry::Get(browser_context())->AddEnabled(extension);
    return extension;
  }

 protected:
  void SetUp() override {
    user_prefs::UserPrefs::Set(browser_context(), &testing_pref_service_);
  }

 private:
  std::unique_ptr<content::NotificationService> notification_service_;

  user_prefs::TestingPrefServiceSyncable testing_pref_service_;

  // The total number of pending tasks that have been executed.
  int task_run_count_;

  DISALLOW_COPY_AND_ASSIGN(LazyBackgroundTaskQueueTest);
};

// Tests that only extensions with background pages should have tasks queued.
TEST_F(LazyBackgroundTaskQueueTest, ShouldEnqueueTask) {
  LazyBackgroundTaskQueue queue(browser_context());

  // Build a simple extension with no background page.
  scoped_refptr<Extension> no_background = CreateSimpleExtension();
  EXPECT_FALSE(queue.ShouldEnqueueTask(browser_context(), no_background.get()));

  // Build another extension with a background page.
  scoped_refptr<Extension> with_background = CreateLazyBackgroundExtension();
  EXPECT_TRUE(
      queue.ShouldEnqueueTask(browser_context(), with_background.get()));
}

// Tests that adding tasks actually increases the pending task count, and that
// multiple extensions can have pending tasks.
TEST_F(LazyBackgroundTaskQueueTest, AddPendingTask) {
  // Get our TestProcessManager.
  TestProcessManager* process_manager = static_cast<TestProcessManager*>(
      ProcessManagerFactory::GetInstance()->SetTestingFactoryAndUse(
          browser_context(), CreateTestProcessManager));

  LazyBackgroundTaskQueue queue(browser_context());

  // Build a simple extension with no background page.
  scoped_refptr<Extension> no_background = CreateSimpleExtension();

  // Adding a pending task increases the number of extensions with tasks, but
  // doesn't run the task.
  queue.AddPendingTask(browser_context(),
                       no_background->id(),
                       base::Bind(&LazyBackgroundTaskQueueTest::RunPendingTask,
                                  base::Unretained(this)));
  EXPECT_EQ(1u, queue.extensions_with_pending_tasks());
  EXPECT_EQ(0, task_run_count());

  // Another task on the same extension doesn't increase the number of
  // extensions that have tasks and doesn't run any tasks.
  queue.AddPendingTask(browser_context(),
                       no_background->id(),
                       base::Bind(&LazyBackgroundTaskQueueTest::RunPendingTask,
                                  base::Unretained(this)));
  EXPECT_EQ(1u, queue.extensions_with_pending_tasks());
  EXPECT_EQ(0, task_run_count());

  // Adding a task on an extension with a lazy background page tries to create
  // a background host, and if that fails, runs the task immediately.
  scoped_refptr<Extension> lazy_background = CreateLazyBackgroundExtension();
  queue.AddPendingTask(browser_context(),
                       lazy_background->id(),
                       base::Bind(&LazyBackgroundTaskQueueTest::RunPendingTask,
                                  base::Unretained(this)));
  EXPECT_EQ(2u, queue.extensions_with_pending_tasks());
  // The process manager tried to create a background host.
  EXPECT_EQ(1, process_manager->create_count());
  // The task ran immediately because the creation failed.
  EXPECT_EQ(1, task_run_count());
}

// Tests that pending tasks are actually run.
TEST_F(LazyBackgroundTaskQueueTest, ProcessPendingTasks) {
  LazyBackgroundTaskQueue queue(browser_context());

  // ProcessPendingTasks is a no-op if there are no tasks.
  scoped_refptr<Extension> extension = CreateSimpleExtension();
  queue.ProcessPendingTasks(NULL, browser_context(), extension.get());
  EXPECT_EQ(0, task_run_count());

  // Schedule a task to run.
  queue.AddPendingTask(browser_context(),
                       extension->id(),
                       base::Bind(&LazyBackgroundTaskQueueTest::RunPendingTask,
                                  base::Unretained(this)));
  EXPECT_EQ(0, task_run_count());
  EXPECT_EQ(1u, queue.extensions_with_pending_tasks());

  // Trying to run tasks for an unrelated BrowserContext should do nothing.
  content::TestBrowserContext unrelated_context;
  queue.ProcessPendingTasks(NULL, &unrelated_context, extension.get());
  EXPECT_EQ(0, task_run_count());
  EXPECT_EQ(1u, queue.extensions_with_pending_tasks());

  // Processing tasks when there is one pending runs the task and removes the
  // extension from the list of extensions with pending tasks.
  queue.ProcessPendingTasks(NULL, browser_context(), extension.get());
  EXPECT_EQ(1, task_run_count());
  EXPECT_EQ(0u, queue.extensions_with_pending_tasks());
}

}  // namespace extensions
