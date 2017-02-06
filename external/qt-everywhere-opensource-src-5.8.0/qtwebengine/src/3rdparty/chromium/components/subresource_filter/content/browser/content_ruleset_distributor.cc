// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/content_ruleset_distributor.h"

#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "components/subresource_filter/content/common/subresource_filter_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_platform_file.h"

namespace subresource_filter {

namespace {

void SendRulesetToRenderProcess(base::File* file,
                                content::RenderProcessHost* rph) {
  DCHECK(rph);
  DCHECK(file);
  DCHECK(file->IsValid());
  rph->Send(new SubresourceFilterMsg_SetRulesetForProcess(
      IPC::TakePlatformFileForTransit(file->Duplicate())));
}

// The file handle is closed when the argument goes out of scope.
void FileCloser(base::File) {}

}  // namespace

ContentRulesetDistributor::ContentRulesetDistributor() {
  // Must rely on notifications as RenderProcessHostObserver::RenderProcessReady
  // would only be called after queued IPC messages (potentially triggering a
  // navigation) had already been sent to the new renderer.
  notification_registrar_.Add(
      this, content::NOTIFICATION_RENDERER_PROCESS_CREATED,
      content::NotificationService::AllBrowserContextsAndSources());
}

ContentRulesetDistributor::~ContentRulesetDistributor() {
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&FileCloser, base::Passed(&ruleset_data_)));
}

void ContentRulesetDistributor::PublishNewVersion(base::File ruleset_data) {
  DCHECK(ruleset_data.IsValid());
  ruleset_data_ = std::move(ruleset_data);
  for (auto it = content::RenderProcessHost::AllHostsIterator(); !it.IsAtEnd();
       it.Advance()) {
    SendRulesetToRenderProcess(&ruleset_data_, it.GetCurrentValue());
  }
}

void ContentRulesetDistributor::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, content::NOTIFICATION_RENDERER_PROCESS_CREATED);
  if (!ruleset_data_.IsValid())
    return;
  SendRulesetToRenderProcess(
      &ruleset_data_,
      content::Source<content::RenderProcessHost>(source).ptr());
}

}  // namespace subresource_filter
