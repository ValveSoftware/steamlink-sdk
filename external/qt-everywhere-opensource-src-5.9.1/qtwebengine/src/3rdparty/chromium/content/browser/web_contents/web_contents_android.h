// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_ANDROID_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_ANDROID_H_

#include <jni.h>

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "content/browser/frame_host/navigation_controller_android.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/common/content_export.h"

namespace content {

class WebContentsImpl;

// Android wrapper around WebContents that provides safer passage from java and
// back to native and provides java with a means of communicating with its
// native counterpart.
class CONTENT_EXPORT WebContentsAndroid
    : public base::SupportsUserData::Data {
 public:
  static bool Register(JNIEnv* env);

  explicit WebContentsAndroid(WebContentsImpl* web_contents);
  ~WebContentsAndroid() override;

  WebContentsImpl* web_contents() const { return web_contents_; }

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // Methods called from Java
  base::android::ScopedJavaLocalRef<jstring> GetTitle(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj) const;
  base::android::ScopedJavaLocalRef<jstring> GetVisibleURL(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj) const;

  bool IsLoading(JNIEnv* env,
                 const base::android::JavaParamRef<jobject>& obj) const;
  bool IsLoadingToDifferentDocument(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj) const;

  void Stop(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void Cut(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void Copy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void Paste(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void Replace(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& obj,
               const base::android::JavaParamRef<jstring>& jstr);
  void SelectAll(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void Unselect(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  jint GetBackgroundColor(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jstring> GetURL(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>&) const;
  base::android::ScopedJavaLocalRef<jstring> GetLastCommittedURL(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>&) const;
  jboolean IsIncognito(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj);

  void ResumeLoadingCreatedWebContents(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  void OnHide(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void OnShow(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void SuspendAllMediaPlayers(JNIEnv* env,
                              const base::android::JavaParamRef<jobject>& jobj);
  void SetAudioMuted(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& jobj,
                     jboolean mute);

  void ShowInterstitialPage(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj,
                            const base::android::JavaParamRef<jstring>& jurl,
                            jlong delegate_ptr);
  jboolean IsShowingInterstitialPage(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean FocusLocationBarByDefault(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean IsRenderWidgetHostViewReady(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void ExitFullscreen(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj);
  void UpdateBrowserControlsState(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      bool enable_hiding,
      bool enable_showing,
      bool animate);
  void ScrollFocusedEditableNodeIntoView(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void SelectWordAroundCaret(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj);
  void AdjustSelectionByCharacterOffset(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint start_adjust,
      jint end_adjust);
  void EvaluateJavaScript(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          const base::android::JavaParamRef<jstring>& script,
                          const base::android::JavaParamRef<jobject>& callback);
  void EvaluateJavaScriptForTests(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& script,
      const base::android::JavaParamRef<jobject>& callback);

  void AddMessageToDevToolsConsole(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      jint level,
      const base::android::JavaParamRef<jstring>& message);

  void PostMessageToFrame(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jframe_name,
      const base::android::JavaParamRef<jstring>& jmessage,
      const base::android::JavaParamRef<jstring>& jtarget_origin,
      const base::android::JavaParamRef<jintArray>& jsent_ports);

  void CreateMessageChannel(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobjectArray>& ports);

  jboolean HasAccessedInitialDocument(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj);

  jint GetThemeColor(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);

  void RequestAccessibilitySnapshot(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& callback);

  base::android::ScopedJavaLocalRef<jstring> GetEncoding(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj) const;

  // Relay the access from Java layer to GetScaledContentBitmap through JNI.
  void GetContentBitmap(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        const base::android::JavaParamRef<jobject>& jcallback,
                        const base::android::JavaParamRef<jobject>& color_type,
                        jfloat scale,
                        jfloat x,
                        jfloat y,
                        jfloat width,
                        jfloat height);

  void ReloadLoFiImages(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj);

  int DownloadImage(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    const base::android::JavaParamRef<jstring>& url,
                    jboolean is_fav_icon,
                    jint max_bitmap_size,
                    jboolean bypass_cache,
                    const base::android::JavaParamRef<jobject>& jcallback);
  void DismissTextHandles(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj);

  void SetMediaSession(
      const base::android::ScopedJavaLocalRef<jobject>& j_media_session);

 private:
  RenderWidgetHostViewAndroid* GetRenderWidgetHostViewAndroid();

  void OnFinishGetContentBitmap(
      base::android::ScopedJavaGlobalRef<jobject>* obj,
      base::android::ScopedJavaGlobalRef<jobject>* callback,
      const SkBitmap& bitmap,
      ReadbackResponse response);

  void OnFinishDownloadImage(
      base::android::ScopedJavaGlobalRef<jobject>* obj,
      base::android::ScopedJavaGlobalRef<jobject>* callback,
      int id,
      int http_status_code,
      const GURL& url,
      const std::vector<SkBitmap>& bitmaps,
      const std::vector<gfx::Size>& sizes);

  WebContentsImpl* web_contents_;
  NavigationControllerAndroid navigation_controller_;
  base::android::ScopedJavaGlobalRef<jobject> obj_;

  base::WeakPtrFactory<WebContentsAndroid> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_ANDROID_H_
