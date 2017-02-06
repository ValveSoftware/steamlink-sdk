// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/android/cast_window_android.h"

#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/browser/android/cast_window_manager.h"
#include "chromecast/browser/cast_content_window.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/renderer_preferences.h"
#include "jni/CastWindowAndroid_jni.h"
#include "ui/gfx/skia_util.h"

namespace chromecast {
namespace shell {

namespace {

// The time (in milliseconds) we wait for after a page is closed (i.e.
// after an app is stopped) before we delete the corresponding WebContents.
const int kWebContentsDestructionDelayInMs = 50;

}  // namespace

// static
bool CastWindowAndroid::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
CastWindowAndroid* CastWindowAndroid::CreateNewWindow(
    content::BrowserContext* browser_context,
    const GURL& url) {
  CastWindowAndroid* window_android = new CastWindowAndroid(browser_context);
  window_android->Initialize();

  if (!url.is_empty())
    window_android->LoadURL(url);

  content::RenderWidgetHostView* rwhv =
      window_android->web_contents_->GetRenderWidgetHostView();
  if (rwhv) {
    rwhv->SetBackgroundColor(SK_ColorBLACK);
  }

  return window_android;
}

CastWindowAndroid::CastWindowAndroid(content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      content_window_(new CastContentWindow),
      weak_factory_(this) {
}

void CastWindowAndroid::Initialize() {
  web_contents_ = content_window_->CreateWebContents(browser_context_);
  web_contents_->SetDelegate(this);
  content::WebContentsObserver::Observe(web_contents_.get());

  JNIEnv* env = base::android::AttachCurrentThread();
  window_java_.Reset(CreateCastWindowView(this));

  Java_CastWindowAndroid_initFromNativeWebContents(
      env, window_java_.obj(), web_contents_->GetJavaWebContents().obj(),
      web_contents_->GetRenderProcessHost()->GetID());

  // Enabling hole-punching also requires runtime renderer preference
  content::RendererPreferences* prefs =
      web_contents_->GetMutableRendererPrefs();
  prefs->use_video_overlay_for_embedded_encrypted_video = true;
  prefs->use_view_overlay_for_all_video = true;
  web_contents_->GetRenderViewHost()->SyncRendererPrefs();
}

CastWindowAndroid::~CastWindowAndroid() {
}

void CastWindowAndroid::Close() {
  // Close page first, which fires the window.unload event. The WebContents
  // itself will be destroyed after browser-process has received renderer
  // notification that the page is closed.
  web_contents_->ClosePage();
}

void CastWindowAndroid::Destroy() {
  // Note: if multiple windows becomes supported, this may close other devtools
  // sessions.
  content::DevToolsAgentHost::DetachAllClients();
  CloseCastWindowView(window_java_.obj());
  delete this;
}

void CastWindowAndroid::LoadURL(const GURL& url) {
  content::NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED |
      ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  web_contents_->GetController().LoadURLWithParams(params);
  web_contents_->Focus();
}

void CastWindowAndroid::AddNewContents(content::WebContents* source,
                                       content::WebContents* new_contents,
                                       WindowOpenDisposition disposition,
                                       const gfx::Rect& initial_rect,
                                       bool user_gesture,
                                       bool* was_blocked) {
  NOTIMPLEMENTED();
  if (was_blocked) {
    *was_blocked = true;
  }
}

void CastWindowAndroid::CloseContents(content::WebContents* source) {
  DCHECK_EQ(source, web_contents_.get());

  // We need to delay the deletion of web_contents_ (currently for 50ms) to
  // give (and guarantee) the renderer enough time to finish 'onunload'
  // handler (but we don't want to wait any longer than that to delay the
  // starting of next app).
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&CastWindowAndroid::Destroy, weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kWebContentsDestructionDelayInMs));
}

bool CastWindowAndroid::CanOverscrollContent() const {
  return false;
}

bool CastWindowAndroid::AddMessageToConsole(content::WebContents* source,
                                            int32_t level,
                                            const base::string16& message,
                                            int32_t line_no,
                                            const base::string16& source_id) {
  return false;
}

void CastWindowAndroid::ActivateContents(content::WebContents* contents) {
  DCHECK_EQ(contents, web_contents_.get());
  contents->GetRenderViewHost()->GetWidget()->Focus();
}

void CastWindowAndroid::RenderProcessGone(base::TerminationStatus status) {
  LOG(ERROR) << "Render process gone: status=" << status;
  Destroy();
}

}  // namespace shell
}  // namespace chromecast
