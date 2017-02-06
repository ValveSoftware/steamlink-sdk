// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_ANDROID_CAST_WINDOW_ANDROID_H_
#define CHROMECAST_BROWSER_ANDROID_CAST_WINDOW_ANDROID_H_

#include <jni.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

class GURL;

namespace content {
class BrowserContext;
class SiteInstance;
class WebContents;
}  // namespace content

namespace chromecast {
namespace shell {
class CastContentWindow;

class CastWindowAndroid : public content::WebContentsDelegate,
                          public content::WebContentsObserver {
 public:
  // Creates a new window and immediately loads the given URL.
  static CastWindowAndroid* CreateNewWindow(
      content::BrowserContext* browser_context,
      const GURL& url);

  ~CastWindowAndroid() override;

  void LoadURL(const GURL& url);
  // Calls RVH::ClosePage() and waits for acknowledgement before closing/
  // deleting the window.
  void Close();
  // Destroys this window immediately.
  void Destroy();

  // Registers the JNI methods for CastWindowAndroid.
  static bool RegisterJni(JNIEnv* env);

  // content::WebContentsDelegate implementation:
  void AddNewContents(content::WebContents* source,
                      content::WebContents* new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override;
  void CloseContents(content::WebContents* source) override;
  bool CanOverscrollContent() const override;
  bool AddMessageToConsole(content::WebContents* source,
                           int32_t level,
                           const base::string16& message,
                           int32_t line_no,
                           const base::string16& source_id) override;
  void ActivateContents(content::WebContents* contents) override;

  // content::WebContentsObserver implementation:
  void RenderProcessGone(base::TerminationStatus status) override;

 private:
  explicit CastWindowAndroid(content::BrowserContext* browser_context);
  void Initialize();

  content::BrowserContext* browser_context_;
  base::android::ScopedJavaGlobalRef<jobject> window_java_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<CastContentWindow> content_window_;

  base::WeakPtrFactory<CastWindowAndroid> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastWindowAndroid);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_ANDROID_CAST_WINDOW_ANDROID_H_
