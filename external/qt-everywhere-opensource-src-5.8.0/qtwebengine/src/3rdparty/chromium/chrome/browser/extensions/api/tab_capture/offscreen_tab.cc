// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tab_capture/offscreen_tab.h"

#include <algorithm>

#include "base/bind.h"
#include "base/macros.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_contents_sizer.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"

using content::WebContents;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(extensions::OffscreenTabsOwner);

namespace {

// Upper limit on the number of simultaneous off-screen tabs per extension
// instance.
const int kMaxOffscreenTabsPerExtension = 3;

// Time intervals used by the logic that detects when the capture of an
// offscreen tab has stopped, to automatically tear it down and free resources.
const int kMaxSecondsToWaitForCapture = 60;
const int kPollIntervalInSeconds = 1;

}  // namespace

namespace extensions {

OffscreenTabsOwner::OffscreenTabsOwner(WebContents* extension_web_contents)
    : extension_web_contents_(extension_web_contents) {
  DCHECK(extension_web_contents_);
}

OffscreenTabsOwner::~OffscreenTabsOwner() {}

// static
OffscreenTabsOwner* OffscreenTabsOwner::Get(
    content::WebContents* extension_web_contents) {
  // CreateForWebContents() really means "create if not exists."
  CreateForWebContents(extension_web_contents);
  return FromWebContents(extension_web_contents);
}

OffscreenTab* OffscreenTabsOwner::OpenNewTab(
    const GURL& start_url,
    const gfx::Size& initial_size,
    const std::string& optional_presentation_id) {
  if (tabs_.size() >= kMaxOffscreenTabsPerExtension)
    return nullptr;  // Maximum number of offscreen tabs reached.

  tabs_.push_back(new OffscreenTab(this));
  tabs_.back()->Start(start_url, initial_size, optional_presentation_id);
  return tabs_.back();
}

void OffscreenTabsOwner::DestroyTab(OffscreenTab* tab) {
  const auto it = std::find(tabs_.begin(), tabs_.end(), tab);
  if (it != tabs_.end())
    tabs_.erase(it);
}

OffscreenTab::OffscreenTab(OffscreenTabsOwner* owner)
    : owner_(owner),
      profile_(Profile::FromBrowserContext(
                   owner->extension_web_contents()->GetBrowserContext())
               ->CreateOffTheRecordProfile()),
      capture_poll_timer_(false, false),
      content_capture_was_detected_(false) {
  DCHECK(profile_);
}

OffscreenTab::~OffscreenTab() {
  DVLOG(1) << "Destroying OffscreenTab for start_url=" << start_url_.spec();
}

void OffscreenTab::Start(const GURL& start_url,
                         const gfx::Size& initial_size,
                         const std::string& optional_presentation_id) {
  DCHECK(start_time_.is_null());
  start_url_ = start_url;
  DVLOG(1) << "Starting OffscreenTab with initial size of "
           << initial_size.ToString() << " for start_url=" << start_url_.spec();

  // Create the WebContents to contain the off-screen tab's page.
  offscreen_tab_web_contents_.reset(
      WebContents::Create(WebContents::CreateParams(profile_.get())));
  offscreen_tab_web_contents_->SetDelegate(this);
  WebContentsObserver::Observe(offscreen_tab_web_contents_.get());

  // Set initial size, if specified.
  if (!initial_size.IsEmpty())
    ResizeWebContents(offscreen_tab_web_contents_.get(), initial_size);

  // Mute audio output.  When tab capture starts, the audio will be
  // automatically unmuted, but will be captured into the MediaStream.
  offscreen_tab_web_contents_->SetAudioMuted(true);

  // TODO(imcheng): If |optional_presentation_id| is not empty, register it with
  // the PresentationRouter.  http://crbug.com/513859
  if (!optional_presentation_id.empty()) {
    NOTIMPLEMENTED()
        << "Register with PresentationRouter, id=" << optional_presentation_id;
  }

  // Navigate to the initial URL.
  content::NavigationController::LoadURLParams load_params(start_url_);
  load_params.should_replace_current_entry = true;
  load_params.should_clear_history_list = true;
  offscreen_tab_web_contents_->GetController().LoadURLWithParams(load_params);

  start_time_ = base::TimeTicks::Now();
  DieIfContentCaptureEnded();
}

void OffscreenTab::CloseContents(WebContents* source) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), source);
  // Javascript in the page called window.close().
  DVLOG(1) << "OffscreenTab will die at renderer's request for start_url="
           << start_url_.spec();
  owner_->DestroyTab(this);
  // |this| is no longer valid.
}

bool OffscreenTab::ShouldSuppressDialogs(WebContents* source) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), source);
  // Suppress all because there is no possible direct user interaction with
  // dialogs.
  return true;
}

bool OffscreenTab::ShouldFocusLocationBarByDefault(WebContents* source) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), source);
  // Indicate the location bar should be focused instead of the page, even
  // though there is no location bar.  This will prevent the page from
  // automatically receiving input focus, which should never occur since there
  // is not supposed to be any direct user interaction.
  return true;
}

bool OffscreenTab::ShouldFocusPageAfterCrash() {
  // Never focus the page.  Not even after a crash.
  return false;
}

void OffscreenTab::CanDownload(const GURL& url,
                               const std::string& request_method,
                               const base::Callback<void(bool)>& callback) {
  // Offscreen tab pages are not allowed to download files.
  callback.Run(false);
}

bool OffscreenTab::HandleContextMenu(const content::ContextMenuParams& params) {
  // Context menus should never be shown.  Do nothing, but indicate the context
  // menu was shown so that default implementation in libcontent does not
  // attempt to do so on its own.
  return true;
}

bool OffscreenTab::PreHandleKeyboardEvent(
    WebContents* source,
    const content::NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), source);
  // Intercept and silence all keyboard events before they can be sent to the
  // renderer.
  *is_keyboard_shortcut = false;
  return true;
}

bool OffscreenTab::PreHandleGestureEvent(WebContents* source,
                                         const blink::WebGestureEvent& event) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), source);
  // Intercept and silence all gesture events before they can be sent to the
  // renderer.
  return true;
}

bool OffscreenTab::CanDragEnter(
    WebContents* source,
    const content::DropData& data,
    blink::WebDragOperationsMask operations_allowed) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), source);
  // Halt all drag attempts onto the page since there should be no direct user
  // interaction with it.
  return false;
}

bool OffscreenTab::ShouldCreateWebContents(
    WebContents* contents,
    int32_t route_id,
    int32_t main_frame_route_id,
    int32_t main_frame_widget_route_id,
    WindowContainerType window_container_type,
    const std::string& frame_name,
    const GURL& target_url,
    const std::string& partition_id,
    content::SessionStorageNamespace* session_storage_namespace) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), contents);
  // Disallow creating separate WebContentses.  The WebContents implementation
  // uses this to spawn new windows/tabs, which is also not allowed for
  // offscreen tabs.
  return false;
}

bool OffscreenTab::EmbedsFullscreenWidget() const {
  // OffscreenTab will manage fullscreen widgets.
  return true;
}

void OffscreenTab::EnterFullscreenModeForTab(WebContents* contents,
                                             const GURL& origin) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), contents);

  if (in_fullscreen_mode())
    return;

  non_fullscreen_size_ =
      contents->GetRenderWidgetHostView()->GetViewBounds().size();
  if (contents->GetCapturerCount() >= 0 &&
      !contents->GetPreferredSize().IsEmpty()) {
    ResizeWebContents(contents, contents->GetPreferredSize());
  }
}

void OffscreenTab::ExitFullscreenModeForTab(WebContents* contents) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), contents);

  if (!in_fullscreen_mode())
    return;

  ResizeWebContents(contents, non_fullscreen_size_);
  non_fullscreen_size_ = gfx::Size();
}

bool OffscreenTab::IsFullscreenForTabOrPending(
    const WebContents* contents) const {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), contents);
  return in_fullscreen_mode();
}

blink::WebDisplayMode OffscreenTab::GetDisplayMode(
    const WebContents* contents) const {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), contents);
  return in_fullscreen_mode() ?
      blink::WebDisplayModeFullscreen : blink::WebDisplayModeBrowser;
}

void OffscreenTab::RequestMediaAccessPermission(
      WebContents* contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), contents);

  // This method is being called to check whether an extension is permitted to
  // capture the page.  Verify that the request is being made by the extension
  // that spawned this OffscreenTab.

  // Find the extension ID associated with the extension background page's
  // WebContents.
  content::BrowserContext* const extension_browser_context =
      owner_->extension_web_contents()->GetBrowserContext();
  const extensions::Extension* const extension =
      ProcessManager::Get(extension_browser_context)->
          GetExtensionForWebContents(owner_->extension_web_contents());
  const std::string extension_id = extension ? extension->id() : "";
  LOG_IF(DFATAL, extension_id.empty())
      << "Extension that started this OffscreenTab was not found.";

  // If verified, allow any tab capture audio/video devices that were requested.
  extensions::TabCaptureRegistry* const tab_capture_registry =
      extensions::TabCaptureRegistry::Get(extension_browser_context);
  content::MediaStreamDevices devices;
  if (tab_capture_registry && tab_capture_registry->VerifyRequest(
          request.render_process_id,
          request.render_frame_id,
          extension_id)) {
    if (request.audio_type == content::MEDIA_TAB_AUDIO_CAPTURE) {
      devices.push_back(content::MediaStreamDevice(
          content::MEDIA_TAB_AUDIO_CAPTURE, std::string(), std::string()));
    }
    if (request.video_type == content::MEDIA_TAB_VIDEO_CAPTURE) {
      devices.push_back(content::MediaStreamDevice(
          content::MEDIA_TAB_VIDEO_CAPTURE, std::string(), std::string()));
    }
  }

  DVLOG(2) << "Allowing " << devices.size()
           << " capture devices for OffscreenTab content.";

  callback.Run(devices, devices.empty() ? content::MEDIA_DEVICE_INVALID_STATE
                                        : content::MEDIA_DEVICE_OK,
               std::unique_ptr<content::MediaStreamUI>(nullptr));
}

bool OffscreenTab::CheckMediaAccessPermission(
    WebContents* contents,
    const GURL& security_origin,
    content::MediaStreamType type) {
  DCHECK_EQ(offscreen_tab_web_contents_.get(), contents);
  return type == content::MEDIA_TAB_AUDIO_CAPTURE ||
      type == content::MEDIA_TAB_VIDEO_CAPTURE;
}

void OffscreenTab::DidShowFullscreenWidget() {
  if (offscreen_tab_web_contents_->GetCapturerCount() == 0 ||
      offscreen_tab_web_contents_->GetPreferredSize().IsEmpty())
    return;  // Do nothing, since no preferred size is specified.
  content::RenderWidgetHostView* const current_fs_view =
      offscreen_tab_web_contents_->GetFullscreenRenderWidgetHostView();
  if (current_fs_view)
    current_fs_view->SetSize(offscreen_tab_web_contents_->GetPreferredSize());
}

void OffscreenTab::DieIfContentCaptureEnded() {
  DCHECK(offscreen_tab_web_contents_.get());

  if (content_capture_was_detected_) {
    if (offscreen_tab_web_contents_->GetCapturerCount() == 0) {
      DVLOG(2) << "Capture of OffscreenTab content has stopped for start_url="
               << start_url_.spec();
      owner_->DestroyTab(this);
      return;  // |this| is no longer valid.
    } else {
      DVLOG(3) << "Capture of OffscreenTab content continues for start_url="
               << start_url_.spec();
    }
  } else if (offscreen_tab_web_contents_->GetCapturerCount() > 0) {
    DVLOG(2) << "Capture of OffscreenTab content has started for start_url="
             << start_url_.spec();
    content_capture_was_detected_ = true;
  } else if (base::TimeTicks::Now() - start_time_ >
                 base::TimeDelta::FromSeconds(kMaxSecondsToWaitForCapture)) {
    // More than a minute has elapsed since this OffscreenTab was started and
    // content capture still hasn't started.  As a safety precaution, assume
    // that content capture is never going to start and die to free up
    // resources.
    LOG(WARNING) << "Capture of OffscreenTab content did not start "
                    "within timeout for start_url=" << start_url_.spec();
    owner_->DestroyTab(this);
    return;  // |this| is no longer valid.
  }

  // Schedule the timer to check again in a second.
  capture_poll_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(kPollIntervalInSeconds),
      base::Bind(&OffscreenTab::DieIfContentCaptureEnded,
                 base::Unretained(this)));
}

}  // namespace extensions
