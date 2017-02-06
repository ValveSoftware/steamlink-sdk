// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_DELEGATE_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_DELEGATE_H_

#include <stdint.h>

#include <vector>

#include "base/i18n/rtl.h"
#include "build/build_config.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/common/content_export.h"
#include "content/common/frame_message_enums.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/javascript_message_type.h"
#include "content/public/common/media_stream_request.h"
#include "net/http/http_response_headers.h"

#if defined(OS_WIN)
#include "ui/gfx/native_widget_types.h"
#endif

class GURL;

namespace IPC {
class Message;
}

namespace content {
class GeolocationServiceContext;
class InterstitialPage;
class PageState;
class RenderFrameHost;
class WakeLockServiceContext;
class WebContents;
struct AXEventNotificationDetails;
struct ContextMenuParams;
struct FileChooserParams;
struct TransitionLayerData;

// An interface implemented by an object interested in knowing about the state
// of the RenderFrameHost.
class CONTENT_EXPORT RenderFrameHostDelegate {
 public:
  // This is used to give the delegate a chance to filter IPC messages.
  virtual bool OnMessageReceived(RenderFrameHost* render_frame_host,
                                 const IPC::Message& message);

  // Gets the last committed URL. See WebContents::GetLastCommittedURL for a
  // description of the semantics.
  virtual const GURL& GetMainFrameLastCommittedURL() const;

  // A message was added to to the console.
  virtual bool AddMessageToConsole(int32_t level,
                                   const base::string16& message,
                                   int32_t line_no,
                                   const base::string16& source_id);

  // Informs the delegate whenever a RenderFrameHost is created.
  virtual void RenderFrameCreated(RenderFrameHost* render_frame_host) {}

  // Informs the delegate whenever a RenderFrameHost is deleted.
  virtual void RenderFrameDeleted(RenderFrameHost* render_frame_host) {}

  // A context menu should be shown, to be built using the context information
  // provided in the supplied params.
  virtual void ShowContextMenu(RenderFrameHost* render_frame_host,
                               const ContextMenuParams& params) {}

  // A JavaScript message, confirmation or prompt should be shown.
  virtual void RunJavaScriptMessage(RenderFrameHost* render_frame_host,
                                    const base::string16& message,
                                    const base::string16& default_prompt,
                                    const GURL& frame_url,
                                    JavaScriptMessageType type,
                                    IPC::Message* reply_msg) {}

  virtual void RunBeforeUnloadConfirm(RenderFrameHost* render_frame_host,
                                      bool is_reload,
                                      IPC::Message* reply_msg) {}

  // Called when a file selection is to be done.
  virtual void RunFileChooser(RenderFrameHost* render_frame_host,
                              const FileChooserParams& params) {}

  // Another page accessed the top-level initial empty document, which means it
  // is no longer safe to display a pending URL without risking a URL spoof.
  virtual void DidAccessInitialDocument() {}

  // The frame changed its window.name property.
  virtual void DidChangeName(RenderFrameHost* render_frame_host,
                             const std::string& name) {}

  // The onload handler in the frame has completed. Only called for the top-
  // level frame.
  virtual void DocumentOnLoadCompleted(RenderFrameHost* render_frame_host) {}

  // The state for the page changed and should be updated in session history.
  virtual void UpdateStateForFrame(RenderFrameHost* render_frame_host,
                                   const PageState& page_state) {}

  // The page's title was changed and should be updated. Only called for the
  // top-level frame.
  virtual void UpdateTitle(RenderFrameHost* render_frame_host,
                           int32_t page_id,
                           const base::string16& title,
                           base::i18n::TextDirection title_direction) {}

  // The page's encoding was changed and should be updated. Only called for the
  // top-level frame.
  virtual void UpdateEncoding(RenderFrameHost* render_frame_host,
                              const std::string& encoding) {}

  // Return this object cast to a WebContents, if it is one. If the object is
  // not a WebContents, returns NULL.
  virtual WebContents* GetAsWebContents();

  // Returns this object cast to an InterstitialPage if it is one. Returns
  // nullptr otherwise.
  virtual InterstitialPage* GetAsInterstitialPage();

  // The render frame has requested access to media devices listed in
  // |request|, and the client should grant or deny that permission by
  // calling |callback|.
  virtual void RequestMediaAccessPermission(
      const MediaStreamRequest& request,
      const MediaResponseCallback& callback);

  // Checks if we have permission to access the microphone or camera. Note that
  // this does not query the user. |type| must be MEDIA_DEVICE_AUDIO_CAPTURE
  // or MEDIA_DEVICE_VIDEO_CAPTURE.
  virtual bool CheckMediaAccessPermission(const GURL& security_origin,
                                          MediaStreamType type);

  // Get the accessibility mode for the WebContents that owns this frame.
  virtual AccessibilityMode GetAccessibilityMode() const;

  // Invoked when an accessibility event is received from the renderer.
  virtual void AccessibilityEventReceived(
      const std::vector<AXEventNotificationDetails>& details) {}

  // Find a guest RenderFrameHost by its parent |render_frame_host| and
  // |browser_plugin_instance_id|.
  virtual RenderFrameHost* GetGuestByInstanceID(
      RenderFrameHost* render_frame_host,
      int browser_plugin_instance_id);

  // Gets the GeolocationServiceContext associated with this delegate.
  virtual GeolocationServiceContext* GetGeolocationServiceContext();

  // Gets the WakeLockServiceContext associated with this delegate.
  virtual WakeLockServiceContext* GetWakeLockServiceContext();

  // Notification that the frame wants to go into fullscreen mode.
  // |origin| represents the origin of the frame that requests fullscreen.
  virtual void EnterFullscreenMode(const GURL& origin) {}

  // Notification that the frame wants to go out of fullscreen mode.
  // |will_cause_resize| indicates whether the fullscreen change causes a
  // view resize. e.g. This will be false when going from tab fullscreen to
  // browser fullscreen.
  virtual void ExitFullscreenMode(bool will_cause_resize) {}

  // Let the delegate decide whether postMessage should be delivered to
  // |target_rfh| from a source frame in the given SiteInstance.  This defaults
  // to false and overrides the RenderFrameHost's decision if true.
  virtual bool ShouldRouteMessageEvent(
      RenderFrameHost* target_rfh,
      SiteInstance* source_site_instance) const;

  // Ensure that |source_rfh| has swapped-out RenderViews and
  // RenderFrameProxies for itself and for all frames on its opener chain in
  // the current frame's SiteInstance.
  //
  // TODO(alexmos): This method currently supports cross-process postMessage,
  // where we may need to create any missing proxies for the message's source
  // frame and its opener chain. It currently exists in WebContents due to a
  // special case for <webview> guests, but this logic should eventually be
  // moved down into RenderFrameProxyHost::RouteMessageEvent when <webview>
  // refactoring for --site-per-process mode is further along.  See
  // https://crbug.com/330264.
  virtual void EnsureOpenerProxiesExist(RenderFrameHost* source_rfh) {}

  // Creates a WebUI object for a frame navigating to |url|. If no WebUI
  // applies, returns null.
  virtual std::unique_ptr<WebUIImpl> CreateWebUIForRenderFrameHost(
      const GURL& url);

 protected:
  virtual ~RenderFrameHostDelegate() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_DELEGATE_H_
