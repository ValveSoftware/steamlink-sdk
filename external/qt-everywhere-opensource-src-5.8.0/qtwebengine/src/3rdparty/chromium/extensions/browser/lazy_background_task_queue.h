// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_LAZY_BACKGROUND_TASK_QUEUE_H_
#define EXTENSIONS_BROWSER_LAZY_BACKGROUND_TASK_QUEUE_H_

#include <stddef.h>

#include <map>
#include <string>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/scoped_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/extension_registry_observer.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
class ExtensionHost;
class ExtensionRegistry;

// This class maintains a queue of tasks that should execute when an
// extension's lazy background page is loaded. It is also in charge of loading
// the page when the first task is queued.
//
// It is the consumer's responsibility to use this class when appropriate, i.e.
// only with extensions that have not-yet-loaded lazy background pages.
class LazyBackgroundTaskQueue : public KeyedService,
                                public content::NotificationObserver,
                                public ExtensionRegistryObserver {
 public:
  typedef base::Callback<void(ExtensionHost*)> PendingTask;

  explicit LazyBackgroundTaskQueue(content::BrowserContext* browser_context);
  ~LazyBackgroundTaskQueue() override;

  // Convenience method to return the LazyBackgroundTaskQueue for a given
  // |context|.
  static LazyBackgroundTaskQueue* Get(content::BrowserContext* context);

  // Returns the number of extensions having pending tasks.
  size_t extensions_with_pending_tasks() { return pending_tasks_.size(); }

  // Returns true if the task should be added to the queue (that is, if the
  // extension has a lazy background page that isn't ready yet). If the
  // extension has a lazy background page that is being suspended this method
  // cancels that suspension.
  bool ShouldEnqueueTask(content::BrowserContext* context,
                         const Extension* extension);

  // Adds a task to the queue for a given extension. If this is the first
  // task added for the extension, its lazy background page will be loaded.
  // The task will be called either when the page is loaded, or when the
  // page fails to load for some reason (e.g. a crash or browser
  // shutdown). In the latter case, the ExtensionHost parameter is NULL.
  void AddPendingTask(
      content::BrowserContext* context,
      const std::string& extension_id,
      const PendingTask& task);

 private:
  FRIEND_TEST_ALL_PREFIXES(LazyBackgroundTaskQueueTest, ProcessPendingTasks);

  // A map between a BrowserContext/extension_id pair and the queue of tasks
  // pending the load of its background page.
  typedef std::string ExtensionID;
  typedef std::pair<content::BrowserContext*, ExtensionID> PendingTasksKey;
  typedef std::vector<PendingTask> PendingTasksList;
  typedef std::map<PendingTasksKey,
                   linked_ptr<PendingTasksList> > PendingTasksMap;

  // content::NotificationObserver interface.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // ExtensionRegistryObserver interface.
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionInfo::Reason reason) override;

  // Called when a lazy background page has finished loading, or has failed to
  // load (host is NULL in that case). All enqueued tasks are run in order.
  void ProcessPendingTasks(
      ExtensionHost* host,
      content::BrowserContext* context,
      const Extension* extension);

  content::BrowserContext* browser_context_;
  content::NotificationRegistrar registrar_;
  PendingTasksMap pending_tasks_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_LAZY_BACKGROUND_TASK_QUEUE_H_
