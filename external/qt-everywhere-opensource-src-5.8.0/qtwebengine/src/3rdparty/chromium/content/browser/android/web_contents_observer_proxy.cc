// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/web_contents_observer_proxy.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/android/media_metadata_android.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "jni/WebContentsObserverProxy_jni.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;

namespace content {

// TODO(dcheng): File a bug. This class incorrectly passes just a frame ID,
// which is not sufficient to identify a frame (since frame IDs are scoped per
// render process, and so may collide).
WebContentsObserverProxy::WebContentsObserverProxy(JNIEnv* env,
                                                   jobject obj,
                                                   WebContents* web_contents)
    : WebContentsObserver(web_contents) {
  DCHECK(obj);
  java_observer_.Reset(env, obj);
}

WebContentsObserverProxy::~WebContentsObserverProxy() {
}

jlong Init(JNIEnv* env,
           const JavaParamRef<jobject>& obj,
           const JavaParamRef<jobject>& java_web_contents) {
  WebContents* web_contents =
      WebContents::FromJavaWebContents(java_web_contents);
  CHECK(web_contents);

  WebContentsObserverProxy* native_observer =
      new WebContentsObserverProxy(env, obj, web_contents);
  return reinterpret_cast<intptr_t>(native_observer);
}

void WebContentsObserverProxy::Destroy(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  delete this;
}

void WebContentsObserverProxy::WebContentsDestroyed() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  // The java side will destroy |this|
  Java_WebContentsObserverProxy_destroy(env, obj.obj());
}

void WebContentsObserverProxy::RenderViewReady() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  Java_WebContentsObserverProxy_renderViewReady(env, obj.obj());
}

void WebContentsObserverProxy::RenderProcessGone(
    base::TerminationStatus termination_status) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  jboolean was_oom_protected =
      termination_status == base::TERMINATION_STATUS_OOM_PROTECTED;
  Java_WebContentsObserverProxy_renderProcessGone(env, obj.obj(),
                                                  was_oom_protected);
}

void WebContentsObserverProxy::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  ScopedJavaLocalRef<jstring> jstring_url(
      ConvertUTF8ToJavaString(env, web_contents()->GetVisibleURL().spec()));
  Java_WebContentsObserverProxy_didFinishNavigation(
      env, obj.obj(), navigation_handle->IsInMainFrame(),
      navigation_handle->IsErrorPage(), navigation_handle->HasCommitted());
}

void WebContentsObserverProxy::DidStartLoading() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  ScopedJavaLocalRef<jstring> jstring_url(
      ConvertUTF8ToJavaString(env, web_contents()->GetVisibleURL().spec()));
  if (auto entry = web_contents()->GetController().GetPendingEntry()) {
    base_url_of_last_started_data_url_ = entry->GetBaseURLForDataURL();
  }
  Java_WebContentsObserverProxy_didStartLoading(env, obj.obj(),
                                                jstring_url.obj());
}

void WebContentsObserverProxy::DidStopLoading() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  std::string url_string = web_contents()->GetLastCommittedURL().spec();
  SetToBaseURLForDataURLIfNeeded(&url_string);
  // DidStopLoading is the last event we should get.
  base_url_of_last_started_data_url_ = GURL::EmptyGURL();
  ScopedJavaLocalRef<jstring> jstring_url(ConvertUTF8ToJavaString(
      env, url_string));
  Java_WebContentsObserverProxy_didStopLoading(env, obj.obj(),
                                               jstring_url.obj());
}

void WebContentsObserverProxy::DidFailProvisionalLoad(
    RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description,
    bool was_ignored_by_handler) {
  DidFailLoadInternal(true, !render_frame_host->GetParent(), error_code,
                      error_description, validated_url, was_ignored_by_handler);
}

void WebContentsObserverProxy::DidFailLoad(
    RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description,
    bool was_ignored_by_handler) {
  DidFailLoadInternal(false, !render_frame_host->GetParent(), error_code,
                      error_description, validated_url, was_ignored_by_handler);
}

void WebContentsObserverProxy::DidNavigateMainFrame(
    const LoadCommittedDetails& details,
    const FrameNavigateParams& params) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  ScopedJavaLocalRef<jstring> jstring_url(
      ConvertUTF8ToJavaString(env, params.url.spec()));
  ScopedJavaLocalRef<jstring> jstring_base_url(
      ConvertUTF8ToJavaString(env, params.base_url.spec()));

  // See http://crbug.com/251330 for why it's determined this way.
  url::Replacements<char> replacements;
  replacements.ClearRef();
  bool urls_same_ignoring_fragment =
      params.url.ReplaceComponents(replacements) ==
      details.previous_url.ReplaceComponents(replacements);

  // is_fragment_navigation is indicative of the intent of this variable.
  // However, there isn't sufficient information here to determine whether this
  // is actually a fragment navigation, or a history API navigation to a URL
  // that would also be valid for a fragment navigation.
  bool is_fragment_navigation =
      urls_same_ignoring_fragment && details.is_in_page;

  Java_WebContentsObserverProxy_didNavigateMainFrame(
      env, obj.obj(), jstring_url.obj(), jstring_base_url.obj(),
      details.is_navigation_to_different_page(), is_fragment_navigation,
      details.http_status_code);
}

void WebContentsObserverProxy::DidNavigateAnyFrame(
    RenderFrameHost* render_frame_host,
    const LoadCommittedDetails& details,
    const FrameNavigateParams& params) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  ScopedJavaLocalRef<jstring> jstring_url(
      ConvertUTF8ToJavaString(env, params.url.spec()));
  ScopedJavaLocalRef<jstring> jstring_base_url(
      ConvertUTF8ToJavaString(env, params.base_url.spec()));
  jboolean jboolean_is_reload = ui::PageTransitionCoreTypeIs(
      params.transition, ui::PAGE_TRANSITION_RELOAD);

  Java_WebContentsObserverProxy_didNavigateAnyFrame(
      env, obj.obj(), jstring_url.obj(), jstring_base_url.obj(),
      jboolean_is_reload);
}

void WebContentsObserverProxy::DocumentAvailableInMainFrame() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  Java_WebContentsObserverProxy_documentAvailableInMainFrame(env, obj.obj());
}

void WebContentsObserverProxy::DidStartProvisionalLoadForFrame(
    RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    bool is_error_page,
    bool is_iframe_srcdoc) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  ScopedJavaLocalRef<jstring> jstring_url(
      ConvertUTF8ToJavaString(env, validated_url.spec()));
  // TODO(dcheng): Does Java really need the parent frame ID? It doesn't appear
  // to be used at all, and it just adds complexity here.
  Java_WebContentsObserverProxy_didStartProvisionalLoadForFrame(
      env, obj.obj(), render_frame_host->GetRoutingID(),
      render_frame_host->GetParent()
          ? render_frame_host->GetParent()->GetRoutingID()
          : -1,
      !render_frame_host->GetParent(), jstring_url.obj(), is_error_page,
      is_iframe_srcdoc);
}

void WebContentsObserverProxy::DidCommitProvisionalLoadForFrame(
    RenderFrameHost* render_frame_host,
    const GURL& url,
    ui::PageTransition transition_type) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  ScopedJavaLocalRef<jstring> jstring_url(
      ConvertUTF8ToJavaString(env, url.spec()));
  Java_WebContentsObserverProxy_didCommitProvisionalLoadForFrame(
      env, obj.obj(), render_frame_host->GetRoutingID(),
      !render_frame_host->GetParent(), jstring_url.obj(), transition_type);
}

void WebContentsObserverProxy::DidFinishLoad(RenderFrameHost* render_frame_host,
                                             const GURL& validated_url) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);

  std::string url_string = validated_url.spec();
  SetToBaseURLForDataURLIfNeeded(&url_string);

  ScopedJavaLocalRef<jstring> jstring_url(
      ConvertUTF8ToJavaString(env, url_string));
  Java_WebContentsObserverProxy_didFinishLoad(
      env, obj.obj(), render_frame_host->GetRoutingID(), jstring_url.obj(),
      !render_frame_host->GetParent());
}

void WebContentsObserverProxy::DocumentLoadedInFrame(
    RenderFrameHost* render_frame_host) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  Java_WebContentsObserverProxy_documentLoadedInFrame(
      env, obj.obj(), render_frame_host->GetRoutingID(),
      !render_frame_host->GetParent());
}

void WebContentsObserverProxy::NavigationEntryCommitted(
    const LoadCommittedDetails& load_details) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  Java_WebContentsObserverProxy_navigationEntryCommitted(env, obj.obj());
}

void WebContentsObserverProxy::DidAttachInterstitialPage() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  Java_WebContentsObserverProxy_didAttachInterstitialPage(env, obj.obj());
}

void WebContentsObserverProxy::DidDetachInterstitialPage() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  Java_WebContentsObserverProxy_didDetachInterstitialPage(env, obj.obj());
}

void WebContentsObserverProxy::DidChangeThemeColor(SkColor color) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  Java_WebContentsObserverProxy_didChangeThemeColor(env, obj.obj(), color);
}

void WebContentsObserverProxy::DidFailLoadInternal(
    bool is_provisional_load,
    bool is_main_frame,
    int error_code,
    const base::string16& description,
    const GURL& url,
    bool was_ignored_by_handler) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  ScopedJavaLocalRef<jstring> jstring_error_description(
      ConvertUTF16ToJavaString(env, description));
  ScopedJavaLocalRef<jstring> jstring_url(
      ConvertUTF8ToJavaString(env, url.spec()));

  Java_WebContentsObserverProxy_didFailLoad(
      env, obj.obj(), is_provisional_load, is_main_frame, error_code,
      jstring_error_description.obj(), jstring_url.obj(),
      was_ignored_by_handler);
}

void WebContentsObserverProxy::DidFirstVisuallyNonEmptyPaint() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  Java_WebContentsObserverProxy_didFirstVisuallyNonEmptyPaint(env, obj.obj());
}

void WebContentsObserverProxy::DidStartNavigationToPendingEntry(
    const GURL& url,
    NavigationController::ReloadType reload_type) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  ScopedJavaLocalRef<jstring> jstring_url(
      ConvertUTF8ToJavaString(env, url.spec()));

  Java_WebContentsObserverProxy_didStartNavigationToPendingEntry(
      env, obj.obj(), jstring_url.obj());
}

void WebContentsObserverProxy::MediaSessionStateChanged(
    bool is_controllable,
    bool is_suspended,
    const MediaMetadata& metadata) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj(java_observer_);
  ScopedJavaLocalRef<jobject> j_metadata =
      MediaMetadataAndroid::CreateJavaObject(env, metadata);

  Java_WebContentsObserverProxy_mediaSessionStateChanged(
      env, obj.obj(), is_controllable, is_suspended, j_metadata.obj());
}

void WebContentsObserverProxy::SetToBaseURLForDataURLIfNeeded(
    std::string* url) {
  NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  // Note that GetBaseURLForDataURL is only used by the Android WebView.
  // FIXME: Should we only return valid specs and "about:blank" for invalid
  // ones? This may break apps.
  if (entry && !entry->GetBaseURLForDataURL().is_empty()) {
    *url = entry->GetBaseURLForDataURL().possibly_invalid_spec();
  } else if (!base_url_of_last_started_data_url_.is_empty()) {
    // NavigationController can lose the pending entry and recreate it without
    // a base URL if there has been a loadUrl("javascript:...") after
    // loadDataWithBaseUrl.
    *url = base_url_of_last_started_data_url_.possibly_invalid_spec();
  }
}

bool RegisterWebContentsObserverProxy(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
}  // namespace content
