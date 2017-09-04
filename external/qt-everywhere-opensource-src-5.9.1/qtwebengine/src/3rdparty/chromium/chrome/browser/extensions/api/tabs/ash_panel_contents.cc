// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/ash_panel_contents.h"

#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/api/tabs/tabs_windows_api.h"
#include "chrome/browser/extensions/api/tabs/windows_event_router.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/gfx/image/image.h"

using extensions::AppWindow;
using extensions::NativeAppWindow;

// AshPanelContents -----------------------------------------------------

AshPanelContents::AshPanelContents(AppWindow* host) : host_(host) {}

AshPanelContents::~AshPanelContents() {
}

void AshPanelContents::Initialize(content::BrowserContext* context,
                                  content::RenderFrameHost* creator_frame,
                                  const GURL& url) {
  url_ = url;

  content::WebContents::CreateParams create_params(
      context, creator_frame->GetSiteInstance());
  create_params.opener_render_process_id = creator_frame->GetProcess()->GetID();
  create_params.opener_render_frame_id = creator_frame->GetRoutingID();
  web_contents_.reset(content::WebContents::Create(create_params));

  // Needed to give the web contents a Window ID. Extension APIs expect web
  // contents to have a Window ID. Also required for FaviconDriver to correctly
  // set the window icon and title.
  SessionTabHelper::CreateForWebContents(web_contents_.get());
  SessionTabHelper::FromWebContents(web_contents_.get())->SetWindowID(
      host_->session_id());

  // Responsible for loading favicons for the Launcher, which uses different
  // logic than the FaviconDriver associated with web_contents_ (instantiated in
  // AppWindow::Init())
  launcher_favicon_loader_.reset(
      new LauncherFaviconLoader(this, web_contents_.get()));
}

void AshPanelContents::LoadContents(int32_t creator_process_id) {
  web_contents_->GetController().LoadURL(
      url_, content::Referrer(), ui::PAGE_TRANSITION_LINK,
      std::string());
}

void AshPanelContents::NativeWindowChanged(NativeAppWindow* native_app_window) {
}

void AshPanelContents::NativeWindowClosed() {
}

void AshPanelContents::DispatchWindowShownForTests() const {
}

void AshPanelContents::OnWindowReady() {}

content::WebContents* AshPanelContents::GetWebContents() const {
  return web_contents_.get();
}

extensions::WindowController* AshPanelContents::GetWindowController() const {
  return nullptr;
}

void AshPanelContents::FaviconUpdated() {
  gfx::Image new_image = gfx::Image::CreateFrom1xBitmap(
      launcher_favicon_loader_->GetFavicon());
  host_->UpdateAppIcon(new_image);
}
