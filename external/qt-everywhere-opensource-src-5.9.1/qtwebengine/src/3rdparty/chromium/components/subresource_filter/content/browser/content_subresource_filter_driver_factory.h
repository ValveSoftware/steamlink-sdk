// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_SUBRESOURCE_FILTER_DRIVER_FACTORY_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_SUBRESOURCE_FILTER_DRIVER_FACTORY_H_

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/supports_user_data.h"
#include "components/safe_browsing_db/util.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {
class WebContents;
class RenderFrameHost;
}  // namespace content

namespace subresource_filter {

using HostSet = std::set<std::string>;

class ContentSubresourceFilterDriver;
class SubresourceFilterClient;
enum class ActivationState;

// Controls the activation of subresource filtering for each page load in a
// WebContents and manufactures the per-frame ContentSubresourceFilterDrivers.
// TODO(melandory): Once https://crbug.com/621856 is fixed this class should
// take care of passing the activation information not only to the main frame,
// but also to the subframes.
class ContentSubresourceFilterDriverFactory
    : public base::SupportsUserData::Data,
      public content::WebContentsObserver {
 public:
  static void CreateForWebContents(
      content::WebContents* web_contents,
      std::unique_ptr<SubresourceFilterClient> client);
  static ContentSubresourceFilterDriverFactory* FromWebContents(
      content::WebContents* web_contents);

  explicit ContentSubresourceFilterDriverFactory(
      content::WebContents* web_contents,
      std::unique_ptr<SubresourceFilterClient> client);
  ~ContentSubresourceFilterDriverFactory() override;

  ContentSubresourceFilterDriver* DriverFromFrameHost(
      content::RenderFrameHost* render_frame_host);

  bool IsWhitelisted(const GURL& url) const;
  bool IsBlacklisted(const GURL& url) const;

  // Whitelists the host of |url|, so that page loads with the main-frame
  // document being loaded from this host will be exempted from subresource
  // filtering for the lifetime of this WebContents.
  void AddHostOfURLToWhitelistSet(const GURL& url);

  void AddToActivationHitsSet(const GURL& url);

  // Called when Safe Browsing detects that the |url| corresponding to the load
  // of the main frame belongs to the blacklist with |threat_type|. If the
  // blacklist is the Safe Browsing Social Engineering ads landing, then |url|
  // and |redirects| are saved.
  void OnMainResourceMatchedSafeBrowsingBlacklist(
      const GURL& url,
      const std::vector<GURL>& redirect_urls,
      safe_browsing::SBThreatType threat_type,
      safe_browsing::ThreatPatternType threat_type_metadata);

  // Reloads the page and inserts the url to the whitelist.
  void OnReloadRequested();

  const HostSet& safe_browsing_blacklisted_patterns_set() const {
    return safe_browsing_blacklisted_patterns_;
  }
  ActivationState activation_state() { return activation_state_; }

 private:
  friend class ContentSubresourceFilterDriverFactoryTest;
  friend class SubresourceFilterNavigationThrottleTest;

  typedef std::map<content::RenderFrameHost*,
                   std::unique_ptr<ContentSubresourceFilterDriver>>
      FrameHostToOwnedDriverMap;

  void SetDriverForFrameHostForTesting(
      content::RenderFrameHost* render_frame_host,
      std::unique_ptr<ContentSubresourceFilterDriver> driver);

  void CreateDriverForFrameHostIfNeeded(
      content::RenderFrameHost* render_frame_host);

  void OnFirstSubresourceLoadDisallowed();

  // content::WebContentsObserver:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;

  // Checks base on the value of |urr| and current activation scope if
  // activation signal should be sent.
  bool ShouldActivateForMainFrameURL(const GURL& url) const;
  void ActivateForFrameHostIfNeeded(content::RenderFrameHost* render_frame_host,
                                    const GURL& url);

  // Internal implementation of ReadyToCommitNavigation which doesn't use
  // NavigationHandle to ease unit tests.
  void ReadyToCommitNavigationInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& url);

  bool IsHit(const GURL& url) const;

  static const char kWebContentsUserDataKey[];

  FrameHostToOwnedDriverMap frame_drivers_;
  std::unique_ptr<SubresourceFilterClient> client_;

  HostSet whitelisted_hosts_;

  // Host+path list of the URLs, where the Safe Browsing detected hit to the
  // threat list of interest. When the navigation is commited
  // |safe_browsing_blacklisted_patterns_| is used to determine whenever
  // the activation signal should be sent. All entities are deleted from the
  // list on navigation commit event.
  HostSet safe_browsing_blacklisted_patterns_;

  ActivationState activation_state_;

  DISALLOW_COPY_AND_ASSIGN(ContentSubresourceFilterDriverFactory);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_SUBRESOURCE_FILTER_DRIVER_FACTORY_H_
