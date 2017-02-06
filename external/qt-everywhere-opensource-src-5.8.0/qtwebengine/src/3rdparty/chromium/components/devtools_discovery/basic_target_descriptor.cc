// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/devtools_discovery/basic_target_descriptor.h"

#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

using content::DevToolsAgentHost;

namespace devtools_discovery {

const char BasicTargetDescriptor::kTypePage[] = "page";
const char BasicTargetDescriptor::kTypeServiceWorker[] = "service_worker";
const char BasicTargetDescriptor::kTypeSharedWorker[] = "worker";
const char BasicTargetDescriptor::kTypeOther[] = "other";

namespace {

std::string GetTypeFromAgentHost(DevToolsAgentHost* agent_host) {
  switch (agent_host->GetType()) {
    case DevToolsAgentHost::TYPE_WEB_CONTENTS:
      return BasicTargetDescriptor::kTypePage;
    case DevToolsAgentHost::TYPE_SERVICE_WORKER:
      return BasicTargetDescriptor::kTypeServiceWorker;
    case DevToolsAgentHost::TYPE_SHARED_WORKER:
      return BasicTargetDescriptor::kTypeSharedWorker;
    default:
      break;
  }
  return BasicTargetDescriptor::kTypeOther;
}

}  // namespace

BasicTargetDescriptor::BasicTargetDescriptor(
    scoped_refptr<DevToolsAgentHost> agent_host)
    : agent_host_(agent_host),
      type_(GetTypeFromAgentHost(agent_host.get())),
      title_(agent_host->GetTitle()),
      url_(agent_host->GetURL()) {
  if (content::WebContents* web_contents = agent_host_->GetWebContents()) {
    content::NavigationController& controller = web_contents->GetController();
    content::NavigationEntry* entry = controller.GetLastCommittedEntry();
    if (entry != NULL && entry->GetURL().is_valid())
      favicon_url_ = entry->GetFavicon().url;
    last_activity_time_ = web_contents->GetLastActiveTime();
  }
}

BasicTargetDescriptor::~BasicTargetDescriptor() {
}

std::string BasicTargetDescriptor::GetId() const {
  return agent_host_->GetId();
}

std::string BasicTargetDescriptor::GetParentId() const {
  return parent_id_;
}

std::string BasicTargetDescriptor::GetType() const {
  return type_;
}

std::string BasicTargetDescriptor::GetTitle() const {
  return title_;
}

std::string BasicTargetDescriptor::GetDescription() const {
  return description_;
}

GURL BasicTargetDescriptor::GetURL() const {
  return url_;
}

GURL BasicTargetDescriptor::GetFaviconURL() const {
  return favicon_url_;
}

base::TimeTicks BasicTargetDescriptor::GetLastActivityTime() const {
  return last_activity_time_;
}

bool BasicTargetDescriptor::IsAttached() const {
  return agent_host_->IsAttached();
}

scoped_refptr<DevToolsAgentHost> BasicTargetDescriptor::GetAgentHost() const {
  return agent_host_;
}

bool BasicTargetDescriptor::Activate() const {
  return agent_host_->Activate();
}

bool BasicTargetDescriptor::Close() const {
  return agent_host_->Close();
}

}  // namespace devtools_discovery
