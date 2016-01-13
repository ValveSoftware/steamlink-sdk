// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_ANDROID_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/android/content_view_core_impl.h"

namespace content {

namespace aria_strings {
  extern const char kAriaLivePolite[];
  extern const char kAriaLiveAssertive[];
}

class CONTENT_EXPORT BrowserAccessibilityManagerAndroid
    : public BrowserAccessibilityManager {
 public:
  BrowserAccessibilityManagerAndroid(
      base::android::ScopedJavaLocalRef<jobject> content_view_core,
      const ui::AXTreeUpdate& initial_tree,
      BrowserAccessibilityDelegate* delegate,
      BrowserAccessibilityFactory* factory = new BrowserAccessibilityFactory());

  virtual ~BrowserAccessibilityManagerAndroid();

  static ui::AXTreeUpdate GetEmptyDocument();

  void SetContentViewCore(
      base::android::ScopedJavaLocalRef<jobject> content_view_core);

  // Implementation of BrowserAccessibilityManager.
  virtual void NotifyAccessibilityEvent(
      ui::AXEvent event_type, BrowserAccessibility* node) OVERRIDE;

  // --------------------------------------------------------------------------
  // Methods called from Java via JNI
  // --------------------------------------------------------------------------

  // Tree methods.
  jint GetRootId(JNIEnv* env, jobject obj);
  jboolean IsNodeValid(JNIEnv* env, jobject obj, jint id);
  void HitTest(JNIEnv* env, jobject obj, jint x, jint y);

  // Populate Java accessibility data structures with info about a node.
  jboolean PopulateAccessibilityNodeInfo(
      JNIEnv* env, jobject obj, jobject info, jint id);
  jboolean PopulateAccessibilityEvent(
      JNIEnv* env, jobject obj, jobject event, jint id, jint event_type);

  // Perform actions.
  void Click(JNIEnv* env, jobject obj, jint id);
  void Focus(JNIEnv* env, jobject obj, jint id);
  void Blur(JNIEnv* env, jobject obj);
  void ScrollToMakeNodeVisible(JNIEnv* env, jobject obj, int id);

  // Return the id of the next node in tree order in the direction given by
  // |forwards|, starting with |start_id|, that matches |element_type|,
  // where |element_type| is a special uppercase string from TalkBack or
  // BrailleBack indicating general categories of web content like
  // "SECTION" or "CONTROL".  Return 0 if not found.
  jint FindElementType(JNIEnv* env, jobject obj, jint start_id,
                       jstring element_type, jboolean forwards);

 protected:
  // AXTreeDelegate overrides.
  virtual void OnRootChanged(ui::AXNode* new_root) OVERRIDE;

  virtual bool UseRootScrollOffsetsWhenComputingBounds() OVERRIDE;

 private:
  // This gives BrowserAccessibilityManager::Create access to the class
  // constructor.
  friend class BrowserAccessibilityManager;

  // A weak reference to the Java BrowserAccessibilityManager object.
  // This avoids adding another reference to BrowserAccessibilityManager and
  // preventing garbage collection.
  // Premature garbage collection is prevented by the long-lived reference in
  // ContentViewCore.
  JavaObjectWeakGlobalRef java_ref_;

  // Handle a hover event from the renderer process.
  void HandleHoverEvent(BrowserAccessibility* node);

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityManagerAndroid);
};

bool RegisterBrowserAccessibilityManager(JNIEnv* env);

}

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_ANDROID_H_
