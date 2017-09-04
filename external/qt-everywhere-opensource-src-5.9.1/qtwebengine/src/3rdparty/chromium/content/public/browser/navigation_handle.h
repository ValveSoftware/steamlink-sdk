// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NAVIGATION_HANDLE_H_
#define CONTENT_PUBLIC_BROWSER_NAVIGATION_HANDLE_H_

#include <memory>
#include <string>

#include "content/common/content_export.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/common/referrer.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_info.h"
#include "ui/base/page_transition_types.h"

class GURL;

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace content {
struct GlobalRequestID;
class NavigationData;
class NavigationThrottle;
class RenderFrameHost;
class SiteInstance;
class WebContents;

// A NavigationHandle tracks information related to a single navigation.
// NavigationHandles are provided to several WebContentsObserver methods to
// allow observers to track specific navigations. Observers should clear any
// references to a NavigationHandle at the time of
// WebContentsObserver::DidFinishNavigation, just before the handle is
// destroyed.
class CONTENT_EXPORT NavigationHandle {
 public:
  virtual ~NavigationHandle() {}

  // Parameters available at navigation start time -----------------------------
  //
  // These parameters are always available during the navigation. Note that
  // some may change during navigation (e.g. due to server redirects).

  // The URL the frame is navigating to. This may change during the navigation
  // when encountering a server redirect.
  // This URL may not be the same as the virtual URL returned from
  // WebContents::GetVisibleURL and WebContents::GetLastCommittedURL. For
  // example, viewing a page's source navigates to the URL of the page, but the
  // virtual URL is prefixed with "view-source:".
  virtual const GURL& GetURL() = 0;

  // Returns the SiteInstance that started the request.
  // If a frame in SiteInstance A navigates a frame in SiteInstance B to a URL
  // in SiteInstance C, then this returns B.
  virtual SiteInstance* GetStartingSiteInstance() = 0;

  // Whether the navigation is taking place in the main frame or in a subframe.
  // This remains constant over the navigation lifetime.
  virtual bool IsInMainFrame() = 0;

  // Whether the navigation is taking place in a frame that is a direct child
  // of the main frame. This remains constant over the navigation lifetime.
  virtual bool IsParentMainFrame() = 0;

  // Whether the navigation was initated by the renderer process. Examples of
  // renderer-initiated navigations include:
  //  * <a> link click
  //  * changing window.location.href
  //  * redirect via the <meta http-equiv="refresh"> tag
  //  * using window.history.pushState
  virtual bool IsRendererInitiated() = 0;

  // Whether the navigation is for an iframe with srcdoc attribute.
  virtual bool IsSrcdoc() = 0;

  // Returns the FrameTreeNode ID for the frame in which the navigation is
  // performed. This ID is browser-global and uniquely identifies a frame that
  // hosts content. The identifier is fixed at the creation of the frame and
  // stays constant for the lifetime of the frame.
  virtual int GetFrameTreeNodeId() = 0;

  // Returns the FrameTreeNode ID for the parent frame. If this navigation is
  // taking place in the main frame, the value returned is -1.
  virtual int GetParentFrameTreeNodeId() = 0;

  // The WebContents the navigation is taking place in.
  WebContents* GetWebContents();

  // The time the navigation started, recorded either in the renderer or in the
  // browser process. Corresponds to Navigation Timing API.
  virtual const base::TimeTicks& NavigationStart() = 0;

  // Whether or not the navigation was started within a context menu.
  virtual bool WasStartedFromContextMenu() const = 0;

  // Returns the URL and encoding of an INPUT field that corresponds to a
  // searchable form request.
  virtual const GURL& GetSearchableFormURL() = 0;
  virtual const std::string& GetSearchableFormEncoding() = 0;

  // Parameters available at network request start time ------------------------
  //
  // The following parameters are only available when the network request is
  // made for the navigation (or at commit time if no network request is made).
  // This corresponds to NavigationThrottle::WillSendRequest. They should not
  // be queried before that.

  // Whether the navigation is done using HTTP POST method. This may change
  // during the navigation (e.g. after encountering a server redirect).
  //
  // Note: page and frame navigations can only be done using HTTP POST or HTTP
  // GET methods (and using other, scheme-specific protocols for non-http(s) URI
  // schemes like data: or file:).  Therefore //content public API exposes only
  // |bool IsPost()| as opposed to |const std::string& GetMethod()| method.
  virtual bool IsPost() = 0;

  // Returns a sanitized version of the referrer for this request.
  virtual const Referrer& GetReferrer() = 0;

  // Whether the navigation was initiated by a user gesture. Note that this
  // will return false for browser-initiated navigations.
  // TODO(clamy): when PlzNavigate launches, this should return true for
  // browser-initiated navigations.
  virtual bool HasUserGesture() = 0;

  // Returns the page transition type.
  virtual ui::PageTransition GetPageTransition() = 0;

  // Whether the target URL cannot be handled by the browser's internal protocol
  // handlers.
  virtual bool IsExternalProtocol() = 0;

  // Navigation control flow --------------------------------------------------

  // The net error code if an error happened prior to commit. Otherwise it will
  // be net::OK.
  virtual net::Error GetNetErrorCode() = 0;

  // Returns the RenderFrameHost this navigation is taking place in. This can
  // only be accessed after a response has been delivered for processing.
  //
  // If PlzNavigate is active, the RenderFrameHost returned will be the final
  // host for the navigation. If PlzNavigate is inactive, the navigation may
  // transfer to a new host up until the point that DidFinishNavigation is
  // called.
  virtual RenderFrameHost* GetRenderFrameHost() = 0;

  // Whether the navigation happened in the same page. Examples of same page
  // navigations are:
  // * reference fragment navigations
  // * pushState/replaceState
  virtual bool IsSamePage() = 0;

  // Whether the navigation has encountered a server redirect or not.
  virtual bool WasServerRedirect() = 0;

  // Whether the navigation has committed. This returns true for either
  // successful commits or error pages that replace the previous page
  // (distinguished by |IsErrorPage|), and false for errors that leave the user
  // on the previous page.
  virtual bool HasCommitted() = 0;

  // Whether the navigation resulted in an error page.
  // Note that if an error page reloads, this will return true even though
  // GetNetErrorCode will be net::OK.
  virtual bool IsErrorPage() = 0;

  // Returns the response headers for the request, or nullptr if there aren't
  // any response headers or they have not been received yet. The response
  // headers may change during the navigation (e.g. after encountering a server
  // redirect). The headers returned should not be modified, as modifications
  // will not be reflected in the network stack.
  virtual const net::HttpResponseHeaders* GetResponseHeaders() = 0;

  // Returns the connection info for the request, the default value is
  // CONNECTION_INFO_UNKNOWN if there hasn't been a response (or redirect)
  // yet. The connection info may change during the navigation (e.g. after
  // encountering a server redirect).
  virtual net::HttpResponseInfo::ConnectionInfo GetConnectionInfo() = 0;

  // Resumes a navigation that was previously deferred by a NavigationThrottle.
  // Note: this may lead to the deletion of the NavigationHandle and its
  // associated NavigationThrottles.
  virtual void Resume() = 0;

  // Cancels a navigation that was previously deferred by a NavigationThrottle.
  // |result| should be equal to NavigationThrottle::CANCEL or
  // NavigationThrottle::CANCEL_AND_IGNORE.
  // Note: this may lead to the deletion of the NavigationHandle and its
  // associated NavigationThrottles.
  virtual void CancelDeferredNavigation(
      NavigationThrottle::ThrottleCheckResult result) = 0;

  // Returns the ID of the URLRequest associated with this navigation. Can only
  // be called from NavigationThrottle::WillProcessResponse and
  // WebContentsObserver::ReadyToCommitNavigation.
  // In the case of transfer navigations, this is the ID of the first request
  // made. The transferred request's ID will not be tracked by the
  // NavigationHandle.
  virtual const GlobalRequestID& GetGlobalRequestID() = 0;

  // Testing methods ----------------------------------------------------------
  //
  // The following methods should be used exclusively for writing unit tests.

  static std::unique_ptr<NavigationHandle> CreateNavigationHandleForTesting(
      const GURL& url,
      RenderFrameHost* render_frame_host,
      bool committed = false,
      net::Error error = net::OK);

  // Registers a NavigationThrottle for tests. The throttle can
  // modify the request, pause the request or cancel the request. This will
  // take ownership of the NavigationThrottle.
  // Note: in non-test cases, NavigationThrottles should not be added directly
  // but returned by the implementation of
  // ContentBrowserClient::CreateThrottlesForNavigation. This ensures proper
  // ordering of the throttles.
  virtual void RegisterThrottleForTesting(
      std::unique_ptr<NavigationThrottle> navigation_throttle) = 0;

  // Simulates the network request starting.
  virtual NavigationThrottle::ThrottleCheckResult
  CallWillStartRequestForTesting(bool is_post,
                                 const Referrer& sanitized_referrer,
                                 bool has_user_gesture,
                                 ui::PageTransition transition,
                                 bool is_external_protocol) = 0;

  // Simulates the network request being redirected.
  virtual NavigationThrottle::ThrottleCheckResult
  CallWillRedirectRequestForTesting(const GURL& new_url,
                                    bool new_method_is_post,
                                    const GURL& new_referrer_url,
                                    bool new_is_external_protocol) = 0;

  // Simulates the reception of the network response.
  virtual NavigationThrottle::ThrottleCheckResult
  CallWillProcessResponseForTesting(
      RenderFrameHost* render_frame_host,
      const std::string& raw_response_headers) = 0;

  // Simulates the navigation being committed.
  virtual void CallDidCommitNavigationForTesting(const GURL& url) = 0;

  // The NavigationData that the embedder returned from
  // ResourceDispatcherHostDelegate::GetNavigationData during commit. This will
  // be a clone of the NavigationData.
  virtual NavigationData* GetNavigationData() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NAVIGATION_HANDLE_H_
