// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_contents_delegate_android/web_contents_delegate_android.h"

#include <android/keycodes.h>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/web_contents_delegate_android/color_chooser_android.h"
#include "components/web_contents_delegate_android/validation_message_bubble_android.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/color_chooser.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "jni/WebContentsDelegateAndroid_jni.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::ScopedJavaLocalRef;
using content::ColorChooser;
using content::RenderWidgetHostView;
using content::WebContents;
using content::WebContentsDelegate;

namespace web_contents_delegate_android {

WebContentsDelegateAndroid::WebContentsDelegateAndroid(JNIEnv* env, jobject obj)
    : weak_java_delegate_(env, obj) {
}

WebContentsDelegateAndroid::~WebContentsDelegateAndroid() {
}

ScopedJavaLocalRef<jobject>
WebContentsDelegateAndroid::GetJavaDelegate(JNIEnv* env) const {
  return weak_java_delegate_.get(env);
}

// ----------------------------------------------------------------------------
// WebContentsDelegate methods
// ----------------------------------------------------------------------------

ColorChooser* WebContentsDelegateAndroid::OpenColorChooser(
      WebContents* source,
      SkColor color,
      const std::vector<content::ColorSuggestion>& suggestions)  {
  return new ColorChooserAndroid(source, color, suggestions);
}

// OpenURLFromTab() will be called when we're performing a browser-intiated
// navigation. The most common scenario for this is opening new tabs (see
// RenderViewImpl::decidePolicyForNavigation for more details).
WebContents* WebContentsDelegateAndroid::OpenURLFromTab(
    WebContents* source,
    const content::OpenURLParams& params) {
  const GURL& url = params.url;
  WindowOpenDisposition disposition = params.disposition;

  if (!source || (disposition != CURRENT_TAB &&
                  disposition != NEW_FOREGROUND_TAB &&
                  disposition != NEW_BACKGROUND_TAB &&
                  disposition != OFF_THE_RECORD)) {
    NOTIMPLEMENTED();
    return NULL;
  }

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return WebContentsDelegate::OpenURLFromTab(source, params);

  if (disposition == NEW_FOREGROUND_TAB ||
      disposition == NEW_BACKGROUND_TAB ||
      disposition == OFF_THE_RECORD) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jstring> java_url =
        ConvertUTF8ToJavaString(env, url.spec());
    ScopedJavaLocalRef<jstring> extra_headers =
            ConvertUTF8ToJavaString(env, params.extra_headers);
    ScopedJavaLocalRef<jobject> post_data;
    if (params.uses_post && params.post_data)
      post_data = params.post_data->ToJavaObject(env);
    Java_WebContentsDelegateAndroid_openNewTab(env,
                                               obj.obj(),
                                               java_url.obj(),
                                               extra_headers.obj(),
                                               post_data.obj(),
                                               disposition,
                                               params.is_renderer_initiated);
    return NULL;
  }

  // content::OpenURLParams -> content::NavigationController::LoadURLParams
  content::NavigationController::LoadURLParams load_params(url);
  load_params.referrer = params.referrer;
  load_params.frame_tree_node_id = params.frame_tree_node_id;
  load_params.redirect_chain = params.redirect_chain;
  load_params.transition_type = params.transition;
  load_params.extra_headers = params.extra_headers;
  load_params.should_replace_current_entry =
      params.should_replace_current_entry;
  load_params.is_renderer_initiated = params.is_renderer_initiated;

  if (params.uses_post) {
    load_params.load_type = content::NavigationController::LOAD_TYPE_HTTP_POST;
    load_params.post_data = params.post_data;
  }

  source->GetController().LoadURLWithParams(load_params);

  return source;
}

void WebContentsDelegateAndroid::NavigationStateChanged(
    WebContents* source, content::InvalidateTypes changed_flags) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_navigationStateChanged(
      env,
      obj.obj(),
      changed_flags);
}

void WebContentsDelegateAndroid::VisibleSSLStateChanged(
    const WebContents* source) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_visibleSSLStateChanged(
      env,
      obj.obj());
}

void WebContentsDelegateAndroid::ActivateContents(WebContents* contents) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_activateContents(env, obj.obj());
}

void WebContentsDelegateAndroid::LoadingStateChanged(WebContents* source,
    bool to_different_document) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  bool has_stopped = source == NULL || !source->IsLoading();

  if (has_stopped) {
    Java_WebContentsDelegateAndroid_onLoadStopped(env, obj.obj());
  } else {
    Java_WebContentsDelegateAndroid_onLoadStarted(
        env,
        obj.obj(),
        to_different_document);
  }
}

void WebContentsDelegateAndroid::LoadProgressChanged(WebContents* source,
                                                     double progress) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_notifyLoadProgressChanged(
      env,
      obj.obj(),
      progress);
}

void WebContentsDelegateAndroid::RendererUnresponsive(WebContents* source) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_rendererUnresponsive(env, obj.obj());
}

void WebContentsDelegateAndroid::RendererResponsive(WebContents* source) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_rendererResponsive(env, obj.obj());
}

bool WebContentsDelegateAndroid::ShouldCreateWebContents(
    WebContents* web_contents,
    int32_t route_id,
    int32_t main_frame_route_id,
    int32_t main_frame_widget_route_id,
    WindowContainerType window_container_type,
    const std::string& frame_name,
    const GURL& target_url,
    const std::string& partition_id,
    content::SessionStorageNamespace* session_storage_namespace) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return true;
  ScopedJavaLocalRef<jstring> java_url =
      ConvertUTF8ToJavaString(env, target_url.spec());
  return Java_WebContentsDelegateAndroid_shouldCreateWebContents(env, obj.obj(),
      java_url.obj());
}

bool WebContentsDelegateAndroid::OnGoToEntryOffset(int offset) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return true;
  return Java_WebContentsDelegateAndroid_onGoToEntryOffset(env, obj.obj(),
      offset);
}

void WebContentsDelegateAndroid::WebContentsCreated(
    WebContents* source_contents, int opener_render_frame_id,
    const std::string& frame_name, const GURL& target_url,
    WebContents* new_contents) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;

  ScopedJavaLocalRef<jobject> jsource_contents;
  if (source_contents)
    jsource_contents = source_contents->GetJavaWebContents();
  ScopedJavaLocalRef<jobject> jnew_contents;
  if (new_contents)
    jnew_contents = new_contents->GetJavaWebContents();

  Java_WebContentsDelegateAndroid_webContentsCreated(
      env, obj.obj(), jsource_contents.obj(), opener_render_frame_id,
      base::android::ConvertUTF8ToJavaString(env, frame_name).obj(),
      base::android::ConvertUTF8ToJavaString(env, target_url.spec()).obj(),
      jnew_contents.obj());
}

void WebContentsDelegateAndroid::CloseContents(WebContents* source) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_closeContents(env, obj.obj());
}

void WebContentsDelegateAndroid::MoveContents(WebContents* source,
                                              const gfx::Rect& pos) {
  // Do nothing.
}

bool WebContentsDelegateAndroid::AddMessageToConsole(
    WebContents* source,
    int32_t level,
    const base::string16& message,
    int32_t line_no,
    const base::string16& source_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return WebContentsDelegate::AddMessageToConsole(source, level, message,
                                                    line_no, source_id);
  ScopedJavaLocalRef<jstring> jmessage(ConvertUTF16ToJavaString(env, message));
  ScopedJavaLocalRef<jstring> jsource_id(
      ConvertUTF16ToJavaString(env, source_id));
  int jlevel = WEB_CONTENTS_DELEGATE_LOG_LEVEL_DEBUG;
  switch (level) {
    case logging::LOG_VERBOSE:
      jlevel = WEB_CONTENTS_DELEGATE_LOG_LEVEL_DEBUG;
      break;
    case logging::LOG_INFO:
      jlevel = WEB_CONTENTS_DELEGATE_LOG_LEVEL_LOG;
      break;
    case logging::LOG_WARNING:
      jlevel = WEB_CONTENTS_DELEGATE_LOG_LEVEL_WARNING;
      break;
    case logging::LOG_ERROR:
      jlevel = WEB_CONTENTS_DELEGATE_LOG_LEVEL_ERROR;
      break;
    default:
      NOTREACHED();
  }
  return Java_WebContentsDelegateAndroid_addMessageToConsole(
      env,
      GetJavaDelegate(env).obj(),
      jlevel,
      jmessage.obj(),
      line_no,
      jsource_id.obj());
}

// This is either called from TabContents::DidNavigateMainFramePostCommit() with
// an empty GURL or responding to RenderViewHost::OnMsgUpateTargetURL(). In
// Chrome, the latter is not always called, especially not during history
// navigation. So we only handle the first case and pass the source TabContents'
// url to Java to update the UI.
void WebContentsDelegateAndroid::UpdateTargetURL(WebContents* source,
                                                 const GURL& url) {
  if (!url.is_empty())
    return;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  ScopedJavaLocalRef<jstring> java_url =
      ConvertUTF8ToJavaString(env, source->GetURL().spec());
  Java_WebContentsDelegateAndroid_onUpdateUrl(env,
                                              obj.obj(),
                                              java_url.obj());
}

void WebContentsDelegateAndroid::HandleKeyboardEvent(
    WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  jobject key_event = event.os_event;
  if (key_event) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
    if (obj.is_null())
      return;
    Java_WebContentsDelegateAndroid_handleKeyboardEvent(
        env, obj.obj(), key_event);
  }
}

bool WebContentsDelegateAndroid::TakeFocus(WebContents* source, bool reverse) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return WebContentsDelegate::TakeFocus(source, reverse);
  return Java_WebContentsDelegateAndroid_takeFocus(
      env, obj.obj(), reverse);
}

void WebContentsDelegateAndroid::ShowRepostFormWarningDialog(
    WebContents* source) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_showRepostFormWarningDialog(env, obj.obj());
}

void WebContentsDelegateAndroid::EnterFullscreenModeForTab(
    WebContents* web_contents,
    const GURL& origin) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_toggleFullscreenModeForTab(env, obj.obj(),
                                                             true);
}

void WebContentsDelegateAndroid::ExitFullscreenModeForTab(
    WebContents* web_contents) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_toggleFullscreenModeForTab(env, obj.obj(),
                                                             false);
}

bool WebContentsDelegateAndroid::IsFullscreenForTabOrPending(
    const WebContents* web_contents) const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return false;
  return Java_WebContentsDelegateAndroid_isFullscreenForTabOrPending(
      env, obj.obj());
}

void WebContentsDelegateAndroid::ShowValidationMessage(
    WebContents* web_contents,
    const gfx::Rect& anchor_in_root_view,
    const base::string16& main_text,
    const base::string16& sub_text) {
  RenderWidgetHostView* rwhv = web_contents->GetRenderWidgetHostView();
  if (rwhv) {
    validation_message_bubble_.reset(
        new ValidationMessageBubbleAndroid(rwhv->GetRenderWidgetHost(),
                                           anchor_in_root_view,
                                           main_text,
                                           sub_text));
  }
}

void WebContentsDelegateAndroid::HideValidationMessage(
    WebContents* web_contents) {
  validation_message_bubble_.reset();
}

void WebContentsDelegateAndroid::MoveValidationMessage(
    WebContents* web_contents,
    const gfx::Rect& anchor_in_root_view) {
  if (!validation_message_bubble_)
    return;
  RenderWidgetHostView* rwhv = web_contents->GetRenderWidgetHostView();
  if (rwhv) {
    validation_message_bubble_->SetPositionRelativeToAnchor(
        rwhv->GetRenderWidgetHost(), anchor_in_root_view);
  }
}

void WebContentsDelegateAndroid::RequestAppBannerFromDevTools(
    content::WebContents* web_contents) {
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

// Register native methods

bool RegisterWebContentsDelegateAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace web_contents_delegate_android
