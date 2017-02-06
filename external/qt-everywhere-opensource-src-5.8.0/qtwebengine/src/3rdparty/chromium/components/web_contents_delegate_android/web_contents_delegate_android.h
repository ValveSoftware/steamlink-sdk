// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_CONTENTS_DELEGATE_ANDROID_WEB_CONTENTS_DELEGATE_ANDROID_H_
#define COMPONENTS_WEB_CONTENTS_DELEGATE_ANDROID_WEB_CONTENTS_DELEGATE_ANDROID_H_

#include <stdint.h>

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "content/public/browser/web_contents_delegate.h"

class GURL;

namespace content {
class WebContents;
class WebContentsDelegate;
struct NativeWebKeyboardEvent;
struct OpenURLParams;
}

namespace web_contents_delegate_android {

class ValidationMessageBubbleAndroid;

enum WebContentsDelegateLogLevel {
  // Equivalent of WebCore::WebConsoleMessage::LevelDebug.
  WEB_CONTENTS_DELEGATE_LOG_LEVEL_DEBUG = 0,
  // Equivalent of WebCore::WebConsoleMessage::LevelLog.
  WEB_CONTENTS_DELEGATE_LOG_LEVEL_LOG = 1,
  // Equivalent of WebCore::WebConsoleMessage::LevelWarning.
  WEB_CONTENTS_DELEGATE_LOG_LEVEL_WARNING = 2,
  // Equivalent of WebCore::WebConsoleMessage::LevelError.
  WEB_CONTENTS_DELEGATE_LOG_LEVEL_ERROR = 3,
};


// Native underpinnings of WebContentsDelegateAndroid.java. Provides a default
// delegate for WebContents to forward calls to the java peer. The embedding
// application may subclass and override methods on either the C++ or Java side
// as required.
class WebContentsDelegateAndroid : public content::WebContentsDelegate {
 public:
  WebContentsDelegateAndroid(JNIEnv* env, jobject obj);
  ~WebContentsDelegateAndroid() override;

  // Binds this WebContentsDelegateAndroid to the passed WebContents instance,
  // such that when that WebContents is destroyed, this
  // WebContentsDelegateAndroid instance will be destroyed too.
  void SetOwnerWebContents(content::WebContents* contents);

  // Overridden from WebContentsDelegate:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  content::ColorChooser* OpenColorChooser(
      content::WebContents* source,
      SkColor color,
      const std::vector<content::ColorSuggestion>& suggestions) override;
  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags) override;
  void VisibleSSLStateChanged(const content::WebContents* source) override;
  void ActivateContents(content::WebContents* contents) override;
  void LoadingStateChanged(content::WebContents* source,
                           bool to_different_document) override;
  void LoadProgressChanged(content::WebContents* source,
                           double load_progress) override;
  void RendererUnresponsive(content::WebContents* source) override;
  void RendererResponsive(content::WebContents* source) override;
  void WebContentsCreated(content::WebContents* source_contents,
                          int opener_render_frame_id,
                          const std::string& frame_name,
                          const GURL& target_url,
                          content::WebContents* new_contents) override;
  bool ShouldCreateWebContents(
      content::WebContents* web_contents,
      int32_t route_id,
      int32_t main_frame_route_id,
      int32_t main_frame_widget_route_id,
      WindowContainerType window_container_type,
      const std::string& frame_name,
      const GURL& target_url,
      const std::string& partition_id,
      content::SessionStorageNamespace* session_storage_namespace) override;
  bool OnGoToEntryOffset(int offset) override;
  void CloseContents(content::WebContents* source) override;
  void MoveContents(content::WebContents* source,
                    const gfx::Rect& pos) override;
  bool AddMessageToConsole(content::WebContents* source,
                           int32_t level,
                           const base::string16& message,
                           int32_t line_no,
                           const base::string16& source_id) override;
  void UpdateTargetURL(content::WebContents* source, const GURL& url) override;
  void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  bool TakeFocus(content::WebContents* source, bool reverse) override;
  void ShowRepostFormWarningDialog(content::WebContents* source) override;
  void EnterFullscreenModeForTab(content::WebContents* web_contents,
                                 const GURL& origin) override;
  void ExitFullscreenModeForTab(content::WebContents* web_contents) override;
  bool IsFullscreenForTabOrPending(
      const content::WebContents* web_contents) const override;
  void ShowValidationMessage(content::WebContents* web_contents,
                             const gfx::Rect& anchor_in_root_view,
                             const base::string16& main_text,
                             const base::string16& sub_text) override;
  void HideValidationMessage(content::WebContents* web_contents) override;
  void MoveValidationMessage(content::WebContents* web_contents,
                             const gfx::Rect& anchor_in_root_view) override;
  void RequestAppBannerFromDevTools(
      content::WebContents* web_contents) override;

 protected:
  base::android::ScopedJavaLocalRef<jobject> GetJavaDelegate(JNIEnv* env) const;

 private:
  // We depend on the java side user of WebContentDelegateAndroid to hold a
  // strong reference to that object as long as they want to receive callbacks
  // on it. Using a weak ref here allows it to be correctly GCed.
  JavaObjectWeakGlobalRef weak_java_delegate_;

  std::unique_ptr<ValidationMessageBubbleAndroid> validation_message_bubble_;
};

bool RegisterWebContentsDelegateAndroid(JNIEnv* env);

}  // namespace web_contents_delegate_android

#endif  // COMPONENTS_WEB_CONTENTS_DELEGATE_ANDROID_WEB_CONTENTS_DELEGATE_ANDROID_H_
