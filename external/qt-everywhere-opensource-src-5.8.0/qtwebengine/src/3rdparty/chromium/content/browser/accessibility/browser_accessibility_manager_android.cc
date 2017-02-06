// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_android.h"

#include <stddef.h>

#include <cmath>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/i18n/char_iterator.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/accessibility/browser_accessibility_android.h"
#include "content/browser/accessibility/one_shot_accessibility_tree_search.h"
#include "content/common/accessibility_messages.h"
#include "jni/BrowserAccessibilityManager_jni.h"
#include "ui/accessibility/ax_text_utils.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace content {

namespace {

using SearchKeyToPredicateMap =
    base::hash_map<base::string16, AccessibilityMatchPredicate>;
base::LazyInstance<SearchKeyToPredicateMap> g_search_key_to_predicate_map =
    LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<base::string16> g_all_search_keys =
    LAZY_INSTANCE_INITIALIZER;

bool SectionPredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  switch (node->GetRole()) {
    case ui::AX_ROLE_ARTICLE:
    case ui::AX_ROLE_APPLICATION:
    case ui::AX_ROLE_BANNER:
    case ui::AX_ROLE_COMPLEMENTARY:
    case ui::AX_ROLE_CONTENT_INFO:
    case ui::AX_ROLE_HEADING:
    case ui::AX_ROLE_MAIN:
    case ui::AX_ROLE_NAVIGATION:
    case ui::AX_ROLE_SEARCH:
    case ui::AX_ROLE_REGION:
      return true;
    default:
      return false;
  }
}

void AddToPredicateMap(const char* search_key_ascii,
                       AccessibilityMatchPredicate predicate) {
  base::string16 search_key_utf16 = base::ASCIIToUTF16(search_key_ascii);
  g_search_key_to_predicate_map.Get()[search_key_utf16] = predicate;
  if (!g_all_search_keys.Get().empty())
    g_all_search_keys.Get() += base::ASCIIToUTF16(",");
  g_all_search_keys.Get() += search_key_utf16;
}

// These are special unofficial strings sent from TalkBack/BrailleBack
// to jump to certain categories of web elements.
void InitSearchKeyToPredicateMapIfNeeded() {
  if (!g_search_key_to_predicate_map.Get().empty())
    return;

  AddToPredicateMap("ARTICLE", AccessibilityArticlePredicate);
  AddToPredicateMap("BUTTON", AccessibilityButtonPredicate);
  AddToPredicateMap("CHECKBOX", AccessibilityCheckboxPredicate);
  AddToPredicateMap("COMBOBOX", AccessibilityComboboxPredicate);
  AddToPredicateMap("CONTROL", AccessibilityControlPredicate);
  AddToPredicateMap("FOCUSABLE", AccessibilityFocusablePredicate);
  AddToPredicateMap("FRAME", AccessibilityFramePredicate);
  AddToPredicateMap("GRAPHIC", AccessibilityGraphicPredicate);
  AddToPredicateMap("H1", AccessibilityH1Predicate);
  AddToPredicateMap("H2", AccessibilityH2Predicate);
  AddToPredicateMap("H3", AccessibilityH3Predicate);
  AddToPredicateMap("H4", AccessibilityH4Predicate);
  AddToPredicateMap("H5", AccessibilityH5Predicate);
  AddToPredicateMap("H6", AccessibilityH6Predicate);
  AddToPredicateMap("HEADING", AccessibilityHeadingPredicate);
  AddToPredicateMap("LANDMARK", AccessibilityLandmarkPredicate);
  AddToPredicateMap("LINK", AccessibilityLinkPredicate);
  AddToPredicateMap("LIST", AccessibilityListPredicate);
  AddToPredicateMap("LIST_ITEM", AccessibilityListItemPredicate);
  AddToPredicateMap("MAIN", AccessibilityMainPredicate);
  AddToPredicateMap("MEDIA", AccessibilityMediaPredicate);
  AddToPredicateMap("RADIO", AccessibilityRadioButtonPredicate);
  AddToPredicateMap("SECTION", SectionPredicate);
  AddToPredicateMap("TABLE", AccessibilityTablePredicate);
  AddToPredicateMap("TEXT_FIELD", AccessibilityTextfieldPredicate);
  AddToPredicateMap("UNVISITED_LINK", AccessibilityUnvisitedLinkPredicate);
  AddToPredicateMap("VISITED_LINK", AccessibilityVisitedLinkPredicate);
}

AccessibilityMatchPredicate PredicateForSearchKey(
    const base::string16& element_type) {
  InitSearchKeyToPredicateMapIfNeeded();
  const auto& iter = g_search_key_to_predicate_map.Get().find(element_type);
  if (iter != g_search_key_to_predicate_map.Get().end())
    return iter->second;

  // If we don't recognize the selector, return any element that's clickable.
  // We mark all focusable nodes and leaf nodes as clickable because it's
  // impossible to know whether a web node has a click handler or not, so
  // to be safe we have to allow accessibility services to click on nearly
  // anything that could possibly respond to a click.
  return [](BrowserAccessibility* start, BrowserAccessibility* node) {
    return static_cast<BrowserAccessibilityAndroid*>(node)->IsClickable();
  };
}

}  // anonymous namespace

namespace aria_strings {
  const char kAriaLivePolite[] = "polite";
  const char kAriaLiveAssertive[] = "assertive";
}

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory) {
  return new BrowserAccessibilityManagerAndroid(
      ScopedJavaLocalRef<jobject>(), initial_tree, delegate, factory);
}

BrowserAccessibilityManagerAndroid*
BrowserAccessibilityManager::ToBrowserAccessibilityManagerAndroid() {
  return static_cast<BrowserAccessibilityManagerAndroid*>(this);
}

BrowserAccessibilityManagerAndroid::BrowserAccessibilityManagerAndroid(
    ScopedJavaLocalRef<jobject> content_view_core,
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory)
    : BrowserAccessibilityManager(delegate, factory),
      prune_tree_for_screen_reader_(true) {
  Initialize(initial_tree);
  SetContentViewCore(content_view_core);
}

BrowserAccessibilityManagerAndroid::~BrowserAccessibilityManagerAndroid() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaRefFromRootManager();
  if (obj.is_null())
    return;

  Java_BrowserAccessibilityManager_onNativeObjectDestroyed(
      env, obj.obj(),reinterpret_cast<intptr_t>(this));
}

// static
ui::AXTreeUpdate
    BrowserAccessibilityManagerAndroid::GetEmptyDocument() {
  ui::AXNodeData empty_document;
  empty_document.id = 0;
  empty_document.role = ui::AX_ROLE_ROOT_WEB_AREA;
  empty_document.state = 1 << ui::AX_STATE_READ_ONLY;

  ui::AXTreeUpdate update;
  update.root_id = empty_document.id;
  update.nodes.push_back(empty_document);
  return update;
}

void BrowserAccessibilityManagerAndroid::SetContentViewCore(
    ScopedJavaLocalRef<jobject> content_view_core) {
  if (content_view_core.is_null())
    return;

  JNIEnv* env = AttachCurrentThread();
  java_ref_ = JavaObjectWeakGlobalRef(
      env, Java_BrowserAccessibilityManager_create(
          env, reinterpret_cast<intptr_t>(this),
          content_view_core.obj()).obj());
}

bool BrowserAccessibilityManagerAndroid::ShouldExposePasswordText() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaRefFromRootManager();
  if (obj.is_null())
    return false;

  return Java_BrowserAccessibilityManager_shouldExposePasswordText(
      env, obj.obj());
}

BrowserAccessibility* BrowserAccessibilityManagerAndroid::GetFocus() {
  BrowserAccessibility* focus = BrowserAccessibilityManager::GetFocus();
  return GetActiveDescendant(focus);
}

void BrowserAccessibilityManagerAndroid::NotifyAccessibilityEvent(
    BrowserAccessibilityEvent::Source source,
    ui::AXEvent event_type,
    BrowserAccessibility* node) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaRefFromRootManager();
  if (obj.is_null())
    return;

  BrowserAccessibilityAndroid* android_node =
      static_cast<BrowserAccessibilityAndroid*>(node);

  if (event_type == ui::AX_EVENT_HIDE)
    return;

  if (event_type == ui::AX_EVENT_TREE_CHANGED)
    return;

  // Layout changes are handled in OnLocationChanges and
  // SendLocationChangeEvents.
  if (event_type == ui::AX_EVENT_LAYOUT_COMPLETE)
    return;

  if (event_type == ui::AX_EVENT_HOVER) {
    HandleHoverEvent(node);
    return;
  }

  // Always send AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED to notify
  // the Android system that the accessibility hierarchy rooted at this
  // node has changed.
  Java_BrowserAccessibilityManager_handleContentChanged(
      env, obj.obj(), node->unique_id());

  // Ignore load complete events on iframes.
  if (event_type == ui::AX_EVENT_LOAD_COMPLETE &&
      node->manager() != GetRootManager()) {
    return;
  }

  switch (event_type) {
    case ui::AX_EVENT_LOAD_COMPLETE:
      Java_BrowserAccessibilityManager_handlePageLoaded(
          env, obj.obj(), GetFocus()->unique_id());
      break;
    case ui::AX_EVENT_FOCUS:
      Java_BrowserAccessibilityManager_handleFocusChanged(
          env, obj.obj(), node->unique_id());
      break;
    case ui::AX_EVENT_CHECKED_STATE_CHANGED:
      Java_BrowserAccessibilityManager_handleCheckStateChanged(
          env, obj.obj(), node->unique_id());
      break;
    case ui::AX_EVENT_CLICKED:
      Java_BrowserAccessibilityManager_handleClicked(env, obj.obj(),
                                                     node->unique_id());
      break;
    case ui::AX_EVENT_SCROLL_POSITION_CHANGED:
      Java_BrowserAccessibilityManager_handleScrollPositionChanged(
          env, obj.obj(), node->unique_id());
      break;
    case ui::AX_EVENT_SCROLLED_TO_ANCHOR:
      Java_BrowserAccessibilityManager_handleScrolledToAnchor(
          env, obj.obj(), node->unique_id());
      break;
    case ui::AX_EVENT_ALERT:
      // An alert is a special case of live region. Fall through to the
      // next case to handle it.
    case ui::AX_EVENT_SHOW: {
      // This event is fired when an object appears in a live region.
      // Speak its text.
      Java_BrowserAccessibilityManager_announceLiveRegionText(
          env, obj.obj(),
          base::android::ConvertUTF16ToJavaString(
              env, android_node->GetText()).obj());
      break;
    }
    case ui::AX_EVENT_TEXT_SELECTION_CHANGED:
      Java_BrowserAccessibilityManager_handleTextSelectionChanged(
          env, obj.obj(), node->unique_id());
      break;
    case ui::AX_EVENT_TEXT_CHANGED:
    case ui::AX_EVENT_VALUE_CHANGED:
      if (android_node->IsEditableText() && GetFocus() == node) {
        Java_BrowserAccessibilityManager_handleEditableTextChanged(
            env, obj.obj(), node->unique_id());
      } else if (android_node->IsSlider()) {
        Java_BrowserAccessibilityManager_handleSliderChanged(
            env, obj.obj(), node->unique_id());
      }
      break;
    default:
      // There are some notifications that aren't meaningful on Android.
      // It's okay to skip them.
      break;
  }
}

void BrowserAccessibilityManagerAndroid::SendLocationChangeEvents(
      const std::vector<AccessibilityHostMsg_LocationChangeParams>& params) {
  // Android is not very efficient at handling notifications, and location
  // changes in particular are frequent and not time-critical. If a lot of
  // nodes changed location, just send a single notification after a short
  // delay (to batch them), rather than lots of individual notifications.
  if (params.size() > 3) {
    ScopedJavaLocalRef<jobject> obj = GetJavaRefFromRootManager();
    JNIEnv* env = AttachCurrentThread();
    if (obj.is_null())
      return;
    Java_BrowserAccessibilityManager_sendDelayedWindowContentChangedEvent(
        env, obj.obj());
    return;
  }

  BrowserAccessibilityManager::SendLocationChangeEvents(params);
}

base::android::ScopedJavaLocalRef<jstring>
BrowserAccessibilityManagerAndroid::GetSupportedHtmlElementTypes(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  InitSearchKeyToPredicateMapIfNeeded();
  return base::android::ConvertUTF16ToJavaString(env, g_all_search_keys.Get());
}

jint BrowserAccessibilityManagerAndroid::GetRootId(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (GetRoot())
    return static_cast<jint>(GetRoot()->unique_id());
  else
    return -1;
}

jboolean BrowserAccessibilityManagerAndroid::IsNodeValid(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint id) {
  return GetFromUniqueID(id) != NULL;
}

void BrowserAccessibilityManagerAndroid::HitTest(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint x,
    jint y) {
  if (delegate())
    delegate()->AccessibilityHitTest(gfx::Point(x, y));
}

jboolean BrowserAccessibilityManagerAndroid::IsEditableText(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint id) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (!node)
    return false;

  return node->IsEditableText();
}

jint BrowserAccessibilityManagerAndroid::GetEditableTextSelectionStart(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint id) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (!node)
    return false;

  return node->GetIntAttribute(ui::AX_ATTR_TEXT_SEL_START);
}

jint BrowserAccessibilityManagerAndroid::GetEditableTextSelectionEnd(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint id) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (!node)
    return false;

  return node->GetIntAttribute(ui::AX_ATTR_TEXT_SEL_END);
}

jboolean BrowserAccessibilityManagerAndroid::PopulateAccessibilityNodeInfo(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& info,
    jint id) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (!node)
    return false;

  if (node->GetParent()) {
    Java_BrowserAccessibilityManager_setAccessibilityNodeInfoParent(
        env, obj, info, node->GetParent()->unique_id());
  }
  for (unsigned i = 0; i < node->PlatformChildCount(); ++i) {
    Java_BrowserAccessibilityManager_addAccessibilityNodeInfoChild(
        env, obj, info, node->PlatformGetChild(i)->unique_id());
  }
  Java_BrowserAccessibilityManager_setAccessibilityNodeInfoBooleanAttributes(
      env, obj, info,
      id,
      node->IsCheckable(),
      node->IsChecked(),
      node->IsClickable(),
      node->IsEnabled(),
      node->IsFocusable(),
      node->IsFocused(),
      node->IsPassword(),
      node->IsScrollable(),
      node->IsSelected(),
      node->IsVisibleToUser());
  Java_BrowserAccessibilityManager_addAccessibilityNodeInfoActions(
      env, obj, info,
      id,
      node->CanScrollForward(),
      node->CanScrollBackward(),
      node->CanScrollUp(),
      node->CanScrollDown(),
      node->CanScrollLeft(),
      node->CanScrollRight(),
      node->IsClickable(),
      node->IsEditableText(),
      node->IsEnabled(),
      node->IsFocusable(),
      node->IsFocused());
  Java_BrowserAccessibilityManager_setAccessibilityNodeInfoClassName(
      env, obj, info,
      base::android::ConvertUTF8ToJavaString(env, node->GetClassName()).obj());
  Java_BrowserAccessibilityManager_setAccessibilityNodeInfoText(
      env, obj, info,
      base::android::ConvertUTF16ToJavaString(env, node->GetText()).obj(),
      node->IsLink(),
      node->IsEditableText());
  base::string16 element_id;
  if (node->GetHtmlAttribute("id", &element_id)) {
    Java_BrowserAccessibilityManager_setAccessibilityNodeInfoViewIdResourceName(
        env, obj, info,
        base::android::ConvertUTF16ToJavaString(env, element_id).obj());
  }

  gfx::Rect absolute_rect = node->GetLocalBoundsRect();
  gfx::Rect parent_relative_rect = absolute_rect;
  if (node->GetParent()) {
    gfx::Rect parent_rect = node->GetParent()->GetLocalBoundsRect();
    parent_relative_rect.Offset(-parent_rect.OffsetFromOrigin());
  }
  bool is_root = node->GetParent() == NULL;
  Java_BrowserAccessibilityManager_setAccessibilityNodeInfoLocation(
      env, obj, info,
      id,
      absolute_rect.x(), absolute_rect.y(),
      parent_relative_rect.x(), parent_relative_rect.y(),
      absolute_rect.width(), absolute_rect.height(),
      is_root);

  Java_BrowserAccessibilityManager_setAccessibilityNodeInfoKitKatAttributes(
      env, obj, info,
      is_root,
      node->IsEditableText(),
      base::android::ConvertUTF16ToJavaString(
          env, node->GetRoleDescription()).obj());

  Java_BrowserAccessibilityManager_setAccessibilityNodeInfoLollipopAttributes(
      env, obj, info,
      node->CanOpenPopup(),
      node->IsContentInvalid(),
      node->IsDismissable(),
      node->IsMultiLine(),
      node->AndroidInputType(),
      node->AndroidLiveRegionType());
  if (node->IsCollection()) {
    Java_BrowserAccessibilityManager_setAccessibilityNodeInfoCollectionInfo(
        env, obj, info,
        node->RowCount(),
        node->ColumnCount(),
        node->IsHierarchical());
  }
  if (node->IsCollectionItem() || node->IsHeading()) {
    Java_BrowserAccessibilityManager_setAccessibilityNodeInfoCollectionItemInfo(
        env, obj, info,
        node->RowIndex(),
        node->RowSpan(),
        node->ColumnIndex(),
        node->ColumnSpan(),
        node->IsHeading());
  }
  if (node->IsRangeType()) {
    Java_BrowserAccessibilityManager_setAccessibilityNodeInfoRangeInfo(
        env, obj, info,
        node->AndroidRangeType(),
        node->RangeMin(),
        node->RangeMax(),
        node->RangeCurrentValue());
  }

  return true;
}

jboolean BrowserAccessibilityManagerAndroid::PopulateAccessibilityEvent(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& event,
    jint id,
    jint event_type) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (!node)
    return false;

  Java_BrowserAccessibilityManager_setAccessibilityEventBooleanAttributes(
      env, obj, event,
      node->IsChecked(),
      node->IsEnabled(),
      node->IsPassword(),
      node->IsScrollable());
  Java_BrowserAccessibilityManager_setAccessibilityEventClassName(
      env, obj, event,
      base::android::ConvertUTF8ToJavaString(env, node->GetClassName()).obj());
  Java_BrowserAccessibilityManager_setAccessibilityEventListAttributes(
      env, obj, event,
      node->GetItemIndex(),
      node->GetItemCount());
  Java_BrowserAccessibilityManager_setAccessibilityEventScrollAttributes(
      env, obj, event,
      node->GetScrollX(),
      node->GetScrollY(),
      node->GetMaxScrollX(),
      node->GetMaxScrollY());

  switch (event_type) {
    case ANDROID_ACCESSIBILITY_EVENT_TEXT_CHANGED: {
      base::string16 before_text = node->GetTextChangeBeforeText();
      base::string16 text = node->GetText();
      Java_BrowserAccessibilityManager_setAccessibilityEventTextChangedAttrs(
          env, obj, event,
          node->GetTextChangeFromIndex(),
          node->GetTextChangeAddedCount(),
          node->GetTextChangeRemovedCount(),
          base::android::ConvertUTF16ToJavaString(
              env, before_text).obj(),
          base::android::ConvertUTF16ToJavaString(env, text).obj());
      break;
    }
    case ANDROID_ACCESSIBILITY_EVENT_TEXT_SELECTION_CHANGED: {
      base::string16 text = node->GetText();
      Java_BrowserAccessibilityManager_setAccessibilityEventSelectionAttrs(
          env, obj, event,
          node->GetSelectionStart(),
          node->GetSelectionEnd(),
          node->GetEditableTextLength(),
          base::android::ConvertUTF16ToJavaString(env, text).obj());
      break;
    }
    default:
      break;
  }

  Java_BrowserAccessibilityManager_setAccessibilityEventLollipopAttributes(
      env, obj, event,
      node->CanOpenPopup(),
      node->IsContentInvalid(),
      node->IsDismissable(),
      node->IsMultiLine(),
      node->AndroidInputType(),
      node->AndroidLiveRegionType());
  if (node->IsCollection()) {
    Java_BrowserAccessibilityManager_setAccessibilityEventCollectionInfo(
        env, obj, event,
        node->RowCount(),
        node->ColumnCount(),
        node->IsHierarchical());
  }
  if (node->IsHeading()) {
    Java_BrowserAccessibilityManager_setAccessibilityEventHeadingFlag(
        env, obj, event, true);
  }
  if (node->IsCollectionItem()) {
    Java_BrowserAccessibilityManager_setAccessibilityEventCollectionItemInfo(
        env, obj, event,
        node->RowIndex(),
        node->RowSpan(),
        node->ColumnIndex(),
        node->ColumnSpan());
  }
  if (node->IsRangeType()) {
    Java_BrowserAccessibilityManager_setAccessibilityEventRangeInfo(
        env, obj, event,
        node->AndroidRangeType(),
        node->RangeMin(),
        node->RangeMax(),
        node->RangeCurrentValue());
  }

  return true;
}

void BrowserAccessibilityManagerAndroid::Click(JNIEnv* env,
                                               const JavaParamRef<jobject>& obj,
                                               jint id) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (node)
    DoDefaultAction(*node);
}

void BrowserAccessibilityManagerAndroid::Focus(JNIEnv* env,
                                               const JavaParamRef<jobject>& obj,
                                               jint id) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (node)
    SetFocus(*node);
}

void BrowserAccessibilityManagerAndroid::Blur(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  SetFocus(*GetRoot());
}

void BrowserAccessibilityManagerAndroid::ScrollToMakeNodeVisible(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint id) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (node)
    ScrollToMakeVisible(*node, gfx::Rect(node->GetLocation().size()));
}

void BrowserAccessibilityManagerAndroid::SetTextFieldValue(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint id,
    const JavaParamRef<jstring>& value) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (node) {
    BrowserAccessibilityManager::SetValue(
        *node, base::android::ConvertJavaStringToUTF16(env, value));
  }
}

void BrowserAccessibilityManagerAndroid::SetSelection(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint id,
    jint start,
    jint end) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (node)
    SetTextSelection(*node, start, end);
}

jboolean BrowserAccessibilityManagerAndroid::AdjustSlider(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint id,
    jboolean increment) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (!node)
    return false;

  BrowserAccessibilityAndroid* android_node =
      static_cast<BrowserAccessibilityAndroid*>(node);

  if (!android_node->IsSlider() || !android_node->IsEnabled())
    return false;

  float value = node->GetFloatAttribute(ui::AX_ATTR_VALUE_FOR_RANGE);
  float min = node->GetFloatAttribute(ui::AX_ATTR_MIN_VALUE_FOR_RANGE);
  float max = node->GetFloatAttribute(ui::AX_ATTR_MAX_VALUE_FOR_RANGE);
  if (max <= min)
    return false;

  // To behave similarly to an Android SeekBar, move by an increment of
  // approximately 20%.
  float original_value = value;
  float delta = (max - min) / 5.0f;
  value += (increment ? delta : -delta);
  value = std::max(std::min(value, max), min);
  if (value != original_value) {
    BrowserAccessibilityManager::SetValue(
        *node, base::UTF8ToUTF16(base::DoubleToString(value)));
    return true;
  }
  return false;
}

void BrowserAccessibilityManagerAndroid::HandleHoverEvent(
    BrowserAccessibility* node) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaRefFromRootManager();
  if (obj.is_null())
    return;

  BrowserAccessibilityAndroid* ancestor =
      static_cast<BrowserAccessibilityAndroid*>(node->GetParent());
  while (ancestor && ancestor != GetRoot()) {
    if (ancestor->PlatformIsLeaf() ||
        (ancestor->IsFocusable() && !ancestor->HasFocusableChild())) {
      node = ancestor;
      // Don't break - we want the highest ancestor that's focusable or a
      // leaf node.
    }
    ancestor = static_cast<BrowserAccessibilityAndroid*>(ancestor->GetParent());
  }

  Java_BrowserAccessibilityManager_handleHover(
      env, obj.obj(), node->unique_id());
}

jint BrowserAccessibilityManagerAndroid::FindElementType(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint start_id,
    const JavaParamRef<jstring>& element_type_str,
    jboolean forwards) {
  BrowserAccessibilityAndroid* start_node = GetFromUniqueID(start_id);
  if (!start_node)
    return 0;

  BrowserAccessibilityManager* root_manager = GetRootManager();
  if (!root_manager)
    return 0;

  BrowserAccessibility* root = root_manager->GetRoot();
  if (!root)
    return 0;

  AccessibilityMatchPredicate predicate = PredicateForSearchKey(
      base::android::ConvertJavaStringToUTF16(env, element_type_str));

  OneShotAccessibilityTreeSearch tree_search(root);
  tree_search.SetStartNode(start_node);
  tree_search.SetDirection(
      forwards ?
          OneShotAccessibilityTreeSearch::FORWARDS :
          OneShotAccessibilityTreeSearch::BACKWARDS);
  tree_search.SetResultLimit(1);
  tree_search.SetImmediateDescendantsOnly(false);
  tree_search.SetVisibleOnly(false);
  tree_search.AddPredicate(predicate);

  if (tree_search.CountMatches() == 0)
    return 0;

  return tree_search.GetMatchAtIndex(0)->unique_id();
}

jboolean BrowserAccessibilityManagerAndroid::NextAtGranularity(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint granularity,
    jboolean extend_selection,
    jint id,
    jint cursor_index) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (!node)
    return false;

  jint start_index = -1;
  int end_index = -1;
  if (NextAtGranularity(granularity, cursor_index, node,
                        &start_index, &end_index)) {
    base::string16 text = node->GetText();
    Java_BrowserAccessibilityManager_finishGranularityMove(
        env, obj, base::android::ConvertUTF16ToJavaString(
            env, text).obj(),
        extend_selection, start_index, end_index, true);
    return true;
  }
  return false;
}

jboolean BrowserAccessibilityManagerAndroid::PreviousAtGranularity(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint granularity,
    jboolean extend_selection,
    jint id,
    jint cursor_index) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (!node)
    return false;

  jint start_index = -1;
  int end_index = -1;
  if (PreviousAtGranularity(granularity, cursor_index, node,
                            &start_index, &end_index)) {
    Java_BrowserAccessibilityManager_finishGranularityMove(
        env, obj, base::android::ConvertUTF16ToJavaString(
            env, node->GetText()).obj(),
        extend_selection, start_index, end_index, false);
    return true;
  }
  return false;
}

bool BrowserAccessibilityManagerAndroid::NextAtGranularity(
    int32_t granularity,
    int32_t cursor_index,
    BrowserAccessibilityAndroid* node,
    int32_t* start_index,
    int32_t* end_index) {
  switch (granularity) {
    case ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_CHARACTER: {
      base::string16 text = node->GetText();
      if (cursor_index >= static_cast<int32_t>(text.length()))
        return false;
      base::i18n::UTF16CharIterator iter(text.data(), text.size());
      while (!iter.end() && iter.array_pos() <= cursor_index)
        iter.Advance();
      *start_index = iter.array_pos();
      *end_index = iter.array_pos();
      break;
    }
    case ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_WORD:
    case ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_LINE: {
      std::vector<int32_t> starts;
      std::vector<int32_t> ends;
      node->GetGranularityBoundaries(granularity, &starts, &ends, 0);
      if (starts.size() == 0)
        return false;

      size_t index = 0;
      while (index < starts.size() - 1 && starts[index] < cursor_index)
        index++;

      if (starts[index] < cursor_index)
        return false;

      *start_index = starts[index];
      *end_index = ends[index];
      break;
    }
    default:
      NOTREACHED();
  }

  return true;
}

bool BrowserAccessibilityManagerAndroid::PreviousAtGranularity(
    int32_t granularity,
    int32_t cursor_index,
    BrowserAccessibilityAndroid* node,
    int32_t* start_index,
    int32_t* end_index) {
  switch (granularity) {
    case ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_CHARACTER: {
      if (cursor_index <= 0)
        return false;
      base::string16 text = node->GetText();
      base::i18n::UTF16CharIterator iter(text.data(), text.size());
      int previous_index = 0;
      while (!iter.end() && iter.array_pos() < cursor_index) {
        previous_index = iter.array_pos();
        iter.Advance();
      }
      *start_index = previous_index;
      *end_index = previous_index;
      break;
    }
    case ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_WORD:
    case ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_LINE: {
      std::vector<int32_t> starts;
      std::vector<int32_t> ends;
      node->GetGranularityBoundaries(granularity, &starts, &ends, 0);
      if (starts.size() == 0)
        return false;

      size_t index = starts.size() - 1;
      while (index > 0 && starts[index] >= cursor_index)
        index--;

      if (starts[index] >= cursor_index)
        return false;

      *start_index = starts[index];
      *end_index = ends[index];
      break;
    }
    default:
      NOTREACHED();
  }

  return true;
}

void BrowserAccessibilityManagerAndroid::SetAccessibilityFocus(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint id) {
  if (delegate_)
    delegate_->AccessibilitySetAccessibilityFocus(id);
}

bool BrowserAccessibilityManagerAndroid::IsSlider(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint id) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (!node)
    return false;

  return node->GetRole() == ui::AX_ROLE_SLIDER;
}

bool BrowserAccessibilityManagerAndroid::Scroll(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint id,
    int direction) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (!node)
    return false;

  return node->Scroll(direction);
}

void BrowserAccessibilityManagerAndroid::OnAtomicUpdateFinished(
    ui::AXTree* tree,
    bool root_changed,
    const std::vector<ui::AXTreeDelegate::Change>& changes) {
  BrowserAccessibilityManager::OnAtomicUpdateFinished(
      tree, root_changed, changes);

  if (root_changed) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> obj = GetJavaRefFromRootManager();
    if (obj.is_null())
      return;

    Java_BrowserAccessibilityManager_handleNavigate(env, obj.obj());
  }
}

bool
BrowserAccessibilityManagerAndroid::UseRootScrollOffsetsWhenComputingBounds() {
  // The Java layer handles the root scroll offset.
  return false;
}

BrowserAccessibilityAndroid*
BrowserAccessibilityManagerAndroid::GetFromUniqueID(int32_t unique_id) {
  return static_cast<BrowserAccessibilityAndroid*>(
      BrowserAccessibility::GetFromUniqueID(unique_id));
}

ScopedJavaLocalRef<jobject>
BrowserAccessibilityManagerAndroid::GetJavaRefFromRootManager() {
  BrowserAccessibilityManagerAndroid* root_manager =
      static_cast<BrowserAccessibilityManagerAndroid*>(
          GetRootManager());
  if (!root_manager)
    return ScopedJavaLocalRef<jobject>();

  JNIEnv* env = AttachCurrentThread();
  return root_manager->java_ref().get(env);
}

bool RegisterBrowserAccessibilityManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
