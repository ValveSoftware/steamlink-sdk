// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents_delegate.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/security_style_explanations.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/security_style.h"
#include "content/public/common/url_constants.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

WebContentsDelegate::WebContentsDelegate() {
}

WebContents* WebContentsDelegate::OpenURLFromTab(WebContents* source,
                                                 const OpenURLParams& params) {
  return nullptr;
}

bool WebContentsDelegate::ShouldTransferNavigation() {
  return true;
}

bool WebContentsDelegate::IsPopupOrPanel(const WebContents* source) const {
  return false;
}

bool WebContentsDelegate::CanOverscrollContent() const { return false; }

gfx::Rect WebContentsDelegate::GetRootWindowResizerRect() const {
  return gfx::Rect();
}

bool WebContentsDelegate::ShouldSuppressDialogs(WebContents* source) {
  return false;
}

bool WebContentsDelegate::ShouldPreserveAbortedURLs(WebContents* source) {
  return false;
}

bool WebContentsDelegate::AddMessageToConsole(WebContents* source,
                                              int32_t level,
                                              const base::string16& message,
                                              int32_t line_no,
                                              const base::string16& source_id) {
  return false;
}

void WebContentsDelegate::BeforeUnloadFired(WebContents* web_contents,
                                            bool proceed,
                                            bool* proceed_to_fire_unload) {
  *proceed_to_fire_unload = true;
}

bool WebContentsDelegate::ShouldFocusLocationBarByDefault(WebContents* source) {
  return false;
}

bool WebContentsDelegate::ShouldFocusPageAfterCrash() {
  return true;
}

bool WebContentsDelegate::ShouldResumeRequestsForCreatedWindow() {
  return true;
}

bool WebContentsDelegate::TakeFocus(WebContents* source, bool reverse) {
  return false;
}

void WebContentsDelegate::CanDownload(
    const GURL& url,
    const std::string& request_method,
    const base::Callback<void(bool)>& callback) {
  callback.Run(true);
}

bool WebContentsDelegate::HandleContextMenu(
    const content::ContextMenuParams& params) {
  return false;
}

void WebContentsDelegate::ViewSourceForTab(WebContents* source,
                                           const GURL& page_url) {
  // Fall back implementation based entirely on the view-source scheme.
  // It suffers from http://crbug.com/523 and that is why browser overrides
  // it with proper implementation.
  GURL url = GURL(kViewSourceScheme + std::string(":") + page_url.spec());
  OpenURLFromTab(source, OpenURLParams(url, Referrer(),
                                       NEW_FOREGROUND_TAB,
                                       ui::PAGE_TRANSITION_LINK, false));
}

void WebContentsDelegate::ViewSourceForFrame(WebContents* source,
                                             const GURL& frame_url,
                                             const PageState& page_state) {
  // Same as ViewSourceForTab, but for given subframe.
  GURL url = GURL(kViewSourceScheme + std::string(":") + frame_url.spec());
  OpenURLFromTab(source, OpenURLParams(url, Referrer(),
                                       NEW_FOREGROUND_TAB,
                                       ui::PAGE_TRANSITION_LINK, false));
}

bool WebContentsDelegate::PreHandleKeyboardEvent(
    WebContents* source,
    const NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  return false;
}

bool WebContentsDelegate::PreHandleGestureEvent(
    WebContents* source,
    const blink::WebGestureEvent& event) {
  return false;
}

bool WebContentsDelegate::CanDragEnter(
    WebContents* source,
    const DropData& data,
    blink::WebDragOperationsMask operations_allowed) {
  return true;
}

bool WebContentsDelegate::OnGoToEntryOffset(int offset) {
  return true;
}

bool WebContentsDelegate::ShouldCreateWebContents(
    WebContents* web_contents,
    int32_t route_id,
    int32_t main_frame_route_id,
    int32_t main_frame_widget_route_id,
    WindowContainerType window_container_type,
    const std::string& frame_name,
    const GURL& target_url,
    const std::string& partition_id,
    SessionStorageNamespace* session_storage_namespace) {
  return true;
}

JavaScriptDialogManager* WebContentsDelegate::GetJavaScriptDialogManager(
    WebContents* source) {
  return nullptr;
}

std::unique_ptr<BluetoothChooser> WebContentsDelegate::RunBluetoothChooser(
    RenderFrameHost* frame,
    const BluetoothChooser::EventHandler& event_handler) {
  return nullptr;
}

bool WebContentsDelegate::EmbedsFullscreenWidget() const {
  return false;
}

bool WebContentsDelegate::IsFullscreenForTabOrPending(
    const WebContents* web_contents) const {
  return false;
}

blink::WebDisplayMode WebContentsDelegate::GetDisplayMode(
    const WebContents* web_contents) const {
  return blink::WebDisplayModeBrowser;
}

content::ColorChooser* WebContentsDelegate::OpenColorChooser(
    WebContents* web_contents,
    SkColor color,
    const std::vector<ColorSuggestion>& suggestions) {
  return nullptr;
}

void WebContentsDelegate::RequestMediaAccessPermission(
    WebContents* web_contents,
    const MediaStreamRequest& request,
    const MediaResponseCallback& callback) {
  LOG(ERROR) << "WebContentsDelegate::RequestMediaAccessPermission: "
             << "Not supported.";
  callback.Run(MediaStreamDevices(), MEDIA_DEVICE_NOT_SUPPORTED,
               std::unique_ptr<MediaStreamUI>());
}

bool WebContentsDelegate::CheckMediaAccessPermission(
    WebContents* web_contents,
    const GURL& security_origin,
    MediaStreamType type) {
  LOG(ERROR) << "WebContentsDelegate::CheckMediaAccessPermission: "
             << "Not supported.";
  return false;
}

#if defined(OS_ANDROID)
void WebContentsDelegate::RequestMediaDecodePermission(
    WebContents* web_contents,
    const base::Callback<void(bool)>& callback) {
  callback.Run(false);
}
#endif

bool WebContentsDelegate::RequestPpapiBrokerPermission(
    WebContents* web_contents,
    const GURL& url,
    const base::FilePath& plugin_path,
    const base::Callback<void(bool)>& callback) {
  return false;
}

WebContentsDelegate::~WebContentsDelegate() {
  while (!attached_contents_.empty()) {
    WebContents* web_contents = *attached_contents_.begin();
    web_contents->SetDelegate(nullptr);
  }
  DCHECK(attached_contents_.empty());
}

void WebContentsDelegate::Attach(WebContents* web_contents) {
  DCHECK(attached_contents_.find(web_contents) == attached_contents_.end());
  attached_contents_.insert(web_contents);
}

void WebContentsDelegate::Detach(WebContents* web_contents) {
  DCHECK(attached_contents_.find(web_contents) != attached_contents_.end());
  attached_contents_.erase(web_contents);
}

gfx::Size WebContentsDelegate::GetSizeForNewRenderView(
   WebContents* web_contents) const {
  return gfx::Size();
}

bool WebContentsDelegate::IsNeverVisible(WebContents* web_contents) {
  return false;
}

bool WebContentsDelegate::SaveFrame(const GURL& url, const Referrer& referrer) {
  return false;
}

SecurityStyle WebContentsDelegate::GetSecurityStyle(
    WebContents* web_contents,
    SecurityStyleExplanations* security_style_explanations) {
  return content::SECURITY_STYLE_UNKNOWN;
}

void WebContentsDelegate::ShowCertificateViewerInDevTools(
    WebContents* web_contents,
    int cert_id) {
}

void WebContentsDelegate::RequestAppBannerFromDevTools(
    content::WebContents* web_contents) {
}

}  // namespace content
