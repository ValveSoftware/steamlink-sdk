// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"

#include "components/safe_browsing_db/util.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_driver.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace subresource_filter {

// static
const char ContentSubresourceFilterDriverFactory::kWebContentsUserDataKey[] =
    "web_contents_subresource_filter_driver_factory";

// static
void ContentSubresourceFilterDriverFactory::CreateForWebContents(
    content::WebContents* web_contents) {
  if (FromWebContents(web_contents))
    return;
  web_contents->SetUserData(
      kWebContentsUserDataKey,
      new ContentSubresourceFilterDriverFactory(web_contents));
}

// static
ContentSubresourceFilterDriverFactory*
ContentSubresourceFilterDriverFactory::FromWebContents(
    content::WebContents* web_contents) {
  return static_cast<ContentSubresourceFilterDriverFactory*>(
      web_contents->GetUserData(kWebContentsUserDataKey));
}

ContentSubresourceFilterDriverFactory::ContentSubresourceFilterDriverFactory(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  content::RenderFrameHost* main_frame_host = web_contents->GetMainFrame();
  if (main_frame_host && main_frame_host->IsRenderFrameLive())
    CreateDriverForFrameHostIfNeeded(main_frame_host);
}

ContentSubresourceFilterDriverFactory::
    ~ContentSubresourceFilterDriverFactory() {}

void ContentSubresourceFilterDriverFactory::CreateDriverForFrameHostIfNeeded(
    content::RenderFrameHost* render_frame_host) {
  auto iterator_and_inserted =
      frame_drivers_.insert(std::make_pair(render_frame_host, nullptr));
  if (iterator_and_inserted.second) {
    iterator_and_inserted.first->second.reset(
        new ContentSubresourceFilterDriver(render_frame_host));
  }
}

void ContentSubresourceFilterDriverFactory::
    OnMainResourceMatchedSafeBrowsingBlacklist(
        const GURL& url,
        const std::vector<GURL>& redirect_urls,
        safe_browsing::ThreatPatternType threat_type) {
  if (threat_type != safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS)
    return;
  activate_on_origins_.insert(url.host());
  for (const auto& url : redirect_urls) {
    activate_on_origins_.insert(url.host());
  }
}

bool ContentSubresourceFilterDriverFactory::ShouldActivateForURL(
    const GURL& url) {
  return activation_set().find(url.host()) != activation_set().end();
}

ContentSubresourceFilterDriver*
ContentSubresourceFilterDriverFactory::DriverFromFrameHost(
    content::RenderFrameHost* render_frame_host) {
  auto iterator = frame_drivers_.find(render_frame_host);
  return iterator == frame_drivers_.end() ? nullptr : iterator->second.get();
}

void ContentSubresourceFilterDriverFactory::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  CreateDriverForFrameHostIfNeeded(render_frame_host);
}

void ContentSubresourceFilterDriverFactory::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  frame_drivers_.erase(render_frame_host);
}

void ContentSubresourceFilterDriverFactory::DidStartProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    bool is_error_page,
    bool is_iframe_srcdoc) {
  // TODO(melandory): Replace placeholder with actual activation logic.
  DriverFromFrameHost(render_frame_host)
      ->ActivateForProvisionalLoad(GetMaximumActivationState());
}

}  // namespace subresource_filter
