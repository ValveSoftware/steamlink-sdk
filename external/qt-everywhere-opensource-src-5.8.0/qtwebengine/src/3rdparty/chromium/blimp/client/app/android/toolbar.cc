// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/app/android/toolbar.h"

#include "base/android/jni_string.h"
#include "blimp/client/app/android/blimp_client_session_android.h"
#include "components/url_formatter/url_fixer.h"
#include "jni/Toolbar_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/gurl.h"

namespace blimp {
namespace client {

namespace {

const int kDummyTabId = 0;

}  // namespace

static jlong Init(JNIEnv* env,
                  const JavaParamRef<jobject>& jobj,
                  const JavaParamRef<jobject>& blimp_client_session) {
  BlimpClientSession* client_session =
      BlimpClientSessionAndroid::FromJavaObject(env,
                                                blimp_client_session.obj());

  return reinterpret_cast<intptr_t>(
      new Toolbar(env, jobj, client_session->GetNavigationFeature()));
}

// static
bool Toolbar::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

Toolbar::Toolbar(JNIEnv* env,
                 const base::android::JavaParamRef<jobject>& jobj,
                 NavigationFeature* navigation_feature)
    : navigation_feature_(navigation_feature) {
  java_obj_.Reset(env, jobj);

  navigation_feature_->SetDelegate(kDummyTabId, this);
}

Toolbar::~Toolbar() {
  navigation_feature_->RemoveDelegate(kDummyTabId);
}

void Toolbar::Destroy(JNIEnv* env, const JavaParamRef<jobject>& jobj) {
  delete this;
}

void Toolbar::OnUrlTextEntered(JNIEnv* env,
                               const JavaParamRef<jobject>& jobj,
                               const JavaParamRef<jstring>& text) {
  std::string url = base::android::ConvertJavaStringToUTF8(env, text);

  // Build a search query, if |url| doesn't have a '.' anywhere.
  if (url.find(".") == std::string::npos)
    url = "http://www.google.com/search?q=" + url;

  GURL fixed_url = url_formatter::FixupURL(url, std::string());
  navigation_feature_->NavigateToUrlText(kDummyTabId, fixed_url.spec());
}

void Toolbar::OnReloadPressed(JNIEnv* env, const JavaParamRef<jobject>& jobj) {
  navigation_feature_->Reload(kDummyTabId);
}

void Toolbar::OnForwardPressed(JNIEnv* env, const JavaParamRef<jobject>& jobj) {
  navigation_feature_->GoForward(kDummyTabId);
}

jboolean Toolbar::OnBackPressed(JNIEnv* env,
                                const JavaParamRef<jobject>& jobj) {
  navigation_feature_->GoBack(kDummyTabId);

  // TODO(dtrainor): Find a way to determine whether or not we're at the end of
  // our history stack.
  return true;
}

void Toolbar::OnUrlChanged(int tab_id, const GURL& url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jurl(
      base::android::ConvertUTF8ToJavaString(env, url.spec()));

  Java_Toolbar_onEngineSentUrl(env, java_obj_.obj(), jurl.obj());
}

void Toolbar::OnFaviconChanged(int tab_id, const SkBitmap& favicon) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jfavicon(gfx::ConvertToJavaBitmap(&favicon));

  Java_Toolbar_onEngineSentFavicon(env, java_obj_.obj(), jfavicon.obj());
}

void Toolbar::OnTitleChanged(int tab_id, const std::string& title) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jtitle(
      base::android::ConvertUTF8ToJavaString(env, title));

  Java_Toolbar_onEngineSentTitle(env, java_obj_.obj(), jtitle.obj());
}

void Toolbar::OnLoadingChanged(int tab_id, bool loading) {
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_Toolbar_onEngineSentLoading(env,
                                   java_obj_.obj(),
                                   static_cast<jboolean>(loading));
}

void Toolbar::OnPageLoadStatusUpdate(int tab_id, bool completed) {
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_Toolbar_onEngineSentPageLoadStatusUpdate(
      env,
      java_obj_.obj(),
      static_cast<jboolean>(completed));
}

}  // namespace client
}  // namespace blimp
