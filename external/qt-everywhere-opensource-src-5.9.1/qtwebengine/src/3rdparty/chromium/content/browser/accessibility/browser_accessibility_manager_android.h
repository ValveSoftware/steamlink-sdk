// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_ANDROID_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_ANDROID_H_

#include <stdint.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/android/content_view_core_impl.h"

namespace content {

namespace aria_strings {
  extern const char kAriaLivePolite[];
  extern const char kAriaLiveAssertive[];
}

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content.browser.accessibility
enum ScrollDirection {
  FORWARD,
  BACKWARD,
  UP,
  DOWN,
  LEFT,
  RIGHT
};

// From android.view.accessibility.AccessibilityNodeInfo in Java:
enum AndroidMovementGranularity {
  ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_CHARACTER = 1,
  ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_WORD = 2,
  ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_LINE = 4
};

// From android.view.accessibility.AccessibilityEvent in Java:
enum {
  ANDROID_ACCESSIBILITY_EVENT_TEXT_CHANGED = 16,
  ANDROID_ACCESSIBILITY_EVENT_TEXT_SELECTION_CHANGED = 8192,
  ANDROID_ACCESSIBILITY_EVENT_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY = 131072
};

class BrowserAccessibilityAndroid;

class CONTENT_EXPORT BrowserAccessibilityManagerAndroid
    : public BrowserAccessibilityManager {
 public:
  BrowserAccessibilityManagerAndroid(
      base::android::ScopedJavaLocalRef<jobject> content_view_core,
      const ui::AXTreeUpdate& initial_tree,
      BrowserAccessibilityDelegate* delegate,
      BrowserAccessibilityFactory* factory = new BrowserAccessibilityFactory());

  ~BrowserAccessibilityManagerAndroid() override;

  static ui::AXTreeUpdate GetEmptyDocument();

  void SetContentViewCore(
      base::android::ScopedJavaLocalRef<jobject> content_view_core);

  // By default, the tree is pruned for a better screen reading experience,
  // including:
  //   * If the node has only static text children
  //   * If the node is focusable and has no focusable children
  //   * If the node is a heading
  // This can be turned off to generate a tree that more accurately reflects
  // the DOM and includes style changes within these nodes.
  void set_prune_tree_for_screen_reader(bool prune) {
    prune_tree_for_screen_reader_ = prune;
  }
  bool prune_tree_for_screen_reader() { return prune_tree_for_screen_reader_; }

  bool ShouldExposePasswordText();

  // BrowserAccessibilityManager overrides.
  BrowserAccessibility* GetFocus() override;
  void NotifyAccessibilityEvent(
      BrowserAccessibilityEvent::Source source,
      ui::AXEvent event_type,
      BrowserAccessibility* node) override;
  void SendLocationChangeEvents(
      const std::vector<AccessibilityHostMsg_LocationChangeParams>& params)
          override;

  // --------------------------------------------------------------------------
  // Methods called from Java via JNI
  // --------------------------------------------------------------------------

  // Global methods.
  base::android::ScopedJavaLocalRef<jstring> GetSupportedHtmlElementTypes(
      JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Tree methods.
  jint GetRootId(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  jboolean IsNodeValid(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       jint id);
  void HitTest(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& obj,
               jint x,
               jint y);

  // Methods to get information about a specific node.
  jboolean IsEditableText(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          jint id);
  jboolean IsFocused(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj,
                     jint id);
  jint GetEditableTextSelectionStart(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint id);
  jint GetEditableTextSelectionEnd(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint id);

  // Populate Java accessibility data structures with info about a node.
  jboolean PopulateAccessibilityNodeInfo(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& info,
      jint id);
  jboolean PopulateAccessibilityEvent(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& event,
      jint id,
      jint event_type);

  // Perform actions.
  void Click(JNIEnv* env,
             const base::android::JavaParamRef<jobject>& obj,
             jint id);
  void Focus(JNIEnv* env,
             const base::android::JavaParamRef<jobject>& obj,
             jint id);
  void Blur(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void ScrollToMakeNodeVisible(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj,
                               jint id);
  void SetTextFieldValue(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         jint id,
                         const base::android::JavaParamRef<jstring>& value);
  void SetSelection(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    jint id,
                    jint start,
                    jint end);
  jboolean AdjustSlider(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        jint id,
                        jboolean increment);
  void ShowContextMenu(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       jint id);

  // Return the id of the next node in tree order in the direction given by
  // |forwards|, starting with |start_id|, that matches |element_type|,
  // where |element_type| is a special uppercase string from TalkBack or
  // BrailleBack indicating general categories of web content like
  // "SECTION" or "CONTROL".  Return 0 if not found.
  jint FindElementType(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       jint start_id,
                       const base::android::JavaParamRef<jstring>& element_type,
                       jboolean forwards);

  // Respond to a ACTION_[NEXT/PREVIOUS]_AT_MOVEMENT_GRANULARITY action
  // and move the cursor/selection within the given node id. We keep track
  // of our own selection in BrowserAccessibilityManager.java for static
  // text, but if this is an editable text node, updates the selected text
  // in Blink, too, and either way calls
  // Java_BrowserAccessibilityManager_finishGranularityMove with the
  // result.
  jboolean NextAtGranularity(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj,
                             jint granularity,
                             jboolean extend_selection,
                             jint id,
                             jint cursor_index);
  jboolean PreviousAtGranularity(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint granularity,
      jboolean extend_selection,
      jint id,
      jint cursor_index);

  // Helper functions to compute the next start and end index when moving
  // forwards or backwards by character, word, or line. This part is
  // unit-tested; the Java interfaces above are just wrappers. Both of these
  // take a single cursor index as input and return the boundaries surrounding
  // the next word or line. If moving by character, the output start and
  // end index will be the same.
  bool NextAtGranularity(int32_t granularity,
                         int cursor_index,
                         BrowserAccessibilityAndroid* node,
                         int32_t* start_index,
                         int32_t* end_index);
  bool PreviousAtGranularity(int32_t granularity,
                             int cursor_index,
                             BrowserAccessibilityAndroid* node,
                             int32_t* start_index,
                             int32_t* end_index);

  // Set accessibility focus. This sends a message to the renderer to
  // asynchronously load inline text boxes for this node only, enabling more
  // accurate movement by granularities on this node.
  void SetAccessibilityFocus(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj,
                             jint id);

  // Returns true if the object is a slider.
  bool IsSlider(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& obj,
                jint id);

  // Scrolls any scrollable container by about 80% of one page in the
  // given direction.
  bool Scroll(JNIEnv* env,
              const base::android::JavaParamRef<jobject>& obj,
              jint id,
              int direction);

  JavaObjectWeakGlobalRef& java_ref() { return java_ref_; }

 protected:
  // AXTreeDelegate overrides.
  void OnAtomicUpdateFinished(
      ui::AXTree* tree,
      bool root_changed,
      const std::vector<ui::AXTreeDelegate::Change>& changes) override;

  bool UseRootScrollOffsetsWhenComputingBounds() override;

 private:
  BrowserAccessibilityAndroid* GetFromUniqueID(int32_t unique_id);

   base::android::ScopedJavaLocalRef<jobject> GetJavaRefFromRootManager();

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

  // See docs for set_prune_tree_for_screen_reader, above.
  bool prune_tree_for_screen_reader_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityManagerAndroid);
};

bool RegisterBrowserAccessibilityManager(JNIEnv* env);

}

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_ANDROID_H_
