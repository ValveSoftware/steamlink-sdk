// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/lazy_background_task_queue.h"

#include "base/callback.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/lazy_background_task_queue_factory.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/view_type.h"

namespace extensions {

LazyBackgroundTaskQueue::LazyBackgroundTaskQueue(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context), extension_registry_observer_(this) {
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_HOST_DID_STOP_FIRST_LOAD,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED,
                 content::NotificationService::AllBrowserContextsAndSources());

  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context));
}

LazyBackgroundTaskQueue::~LazyBackgroundTaskQueue() {
}

// static
LazyBackgroundTaskQueue* LazyBackgroundTaskQueue::Get(
    content::BrowserContext* browser_context) {
  return LazyBackgroundTaskQueueFactory::GetForBrowserContext(browser_context);
}

bool LazyBackgroundTaskQueue::ShouldEnqueueTask(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  // Note: browser_context may not be the same as browser_context_ for incognito
  // extension tasks.
  DCHECK(extension);
  if (BackgroundInfo::HasBackgroundPage(extension)) {
    ProcessManager* pm = ProcessManager::Get(browser_context);
    ExtensionHost* background_host =
        pm->GetBackgroundHostForExtension(extension->id());
    if (!background_host || !background_host->has_loaded_once())
      return true;
    if (pm->IsBackgroundHostClosing(extension->id()))
      pm->CancelSuspend(extension);
  }

  return false;
}

void LazyBackgroundTaskQueue::AddPendingTask(
    content::BrowserContext* browser_context,
    const std::string& extension_id,
    const PendingTask& task) {
  if (ExtensionsBrowserClient::Get()->IsShuttingDown()) {
    task.Run(NULL);
    return;
  }
  PendingTasksList* tasks_list = NULL;
  PendingTasksKey key(browser_context, extension_id);
  PendingTasksMap::iterator it = pending_tasks_.find(key);
  if (it == pending_tasks_.end()) {
    tasks_list = new PendingTasksList();
    pending_tasks_[key] = linked_ptr<PendingTasksList>(tasks_list);

    const Extension* extension =
        ExtensionRegistry::Get(browser_context)->enabled_extensions().GetByID(
            extension_id);
    if (extension && BackgroundInfo::HasLazyBackgroundPage(extension)) {
      // If this is the first enqueued task, and we're not waiting for the
      // background page to unload, ensure the background page is loaded.
      ProcessManager* pm = ProcessManager::Get(browser_context);
      pm->IncrementLazyKeepaliveCount(extension);
      // Creating the background host may fail, e.g. if |profile| is incognito
      // but the extension isn't enabled in incognito mode.
      if (!pm->CreateBackgroundHost(
            extension, BackgroundInfo::GetBackgroundURL(extension))) {
        task.Run(NULL);
        return;
      }
    }
  } else {
    tasks_list = it->second.get();
  }

  tasks_list->push_back(task);
}

void LazyBackgroundTaskQueue::ProcessPendingTasks(
    ExtensionHost* host,
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (!ExtensionsBrowserClient::Get()->IsSameContext(browser_context,
                                                     browser_context_))
    return;

  PendingTasksKey key(browser_context, extension->id());
  PendingTasksMap::iterator map_it = pending_tasks_.find(key);
  if (map_it == pending_tasks_.end()) {
    if (BackgroundInfo::HasLazyBackgroundPage(extension))
      CHECK(!host);  // lazy page should not load without any pending tasks
    return;
  }

  // Swap the pending tasks to a temporary, to avoid problems if the task
  // list is modified during processing.
  PendingTasksList tasks;
  tasks.swap(*map_it->second);
  for (PendingTasksList::const_iterator it = tasks.begin();
       it != tasks.end(); ++it) {
    it->Run(host);
  }

  pending_tasks_.erase(key);

  // Balance the keepalive in AddPendingTask. Note we don't do this on a
  // failure to load, because the keepalive count is reset in that case.
  if (host && BackgroundInfo::HasLazyBackgroundPage(extension)) {
    ProcessManager::Get(browser_context)
        ->DecrementLazyKeepaliveCount(extension);
  }
}

void LazyBackgroundTaskQueue::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case extensions::NOTIFICATION_EXTENSION_HOST_DID_STOP_FIRST_LOAD: {
      // If an on-demand background page finished loading, dispatch queued up
      // events for it.
      ExtensionHost* host =
          content::Details<ExtensionHost>(details).ptr();
      if (host->extension_host_type() == VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
        CHECK(host->has_loaded_once());
        ProcessPendingTasks(host, host->browser_context(), host->extension());
      }
      break;
    }
    case extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED: {
      // Notify consumers about the load failure when the background host dies.
      // This can happen if the extension crashes. This is not strictly
      // necessary, since we also unload the extension in that case (which
      // dispatches the tasks below), but is a good extra precaution.
      content::BrowserContext* browser_context =
          content::Source<content::BrowserContext>(source).ptr();
      ExtensionHost* host =
           content::Details<ExtensionHost>(details).ptr();
      if (host->extension_host_type() == VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
        ProcessPendingTasks(NULL, browser_context, host->extension());
      }
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

void LazyBackgroundTaskQueue::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  // Notify consumers that the page failed to load.
  ProcessPendingTasks(NULL, browser_context, extension);
  // If this extension is also running in an off-the-record context, notify that
  // task queue as well.
  ExtensionsBrowserClient* browser_client = ExtensionsBrowserClient::Get();
  if (browser_client->HasOffTheRecordContext(browser_context)) {
    ProcessPendingTasks(NULL,
                        browser_client->GetOffTheRecordContext(browser_context),
                        extension);
  }
}

}  // namespace extensions
