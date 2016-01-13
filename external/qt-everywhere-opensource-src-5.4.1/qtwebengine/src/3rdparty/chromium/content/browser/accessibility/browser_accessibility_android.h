// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_ANDROID_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "content/browser/accessibility/browser_accessibility.h"

namespace content {

class BrowserAccessibilityAndroid : public BrowserAccessibility {
 public:
  // Overrides from BrowserAccessibility.
  virtual void OnDataChanged() OVERRIDE;
  virtual bool IsNative() const OVERRIDE;

  virtual bool PlatformIsLeaf() const OVERRIDE;

  bool IsCheckable() const;
  bool IsChecked() const;
  bool IsClickable() const;
  bool IsCollection() const;
  bool IsCollectionItem() const;
  bool IsContentInvalid() const;
  bool IsDismissable() const;
  bool IsEnabled() const;
  bool IsFocusable() const;
  bool IsFocused() const;
  bool IsHeading() const;
  bool IsHierarchical() const;
  bool IsLink() const;
  bool IsMultiLine() const;
  bool IsPassword() const;
  bool IsRangeType() const;
  bool IsScrollable() const;
  bool IsSelected() const;
  bool IsVisibleToUser() const;

  bool CanOpenPopup() const;

  bool HasFocusableChild() const;

  const char* GetClassName() const;
  base::string16 GetText() const;

  int GetItemIndex() const;
  int GetItemCount() const;

  int GetScrollX() const;
  int GetScrollY() const;
  int GetMaxScrollX() const;
  int GetMaxScrollY() const;

  int GetTextChangeFromIndex() const;
  int GetTextChangeAddedCount() const;
  int GetTextChangeRemovedCount() const;
  base::string16 GetTextChangeBeforeText() const;

  int GetSelectionStart() const;
  int GetSelectionEnd() const;
  int GetEditableTextLength() const;

  int AndroidInputType() const;
  int AndroidLiveRegionType() const;
  int AndroidRangeType() const;

  int RowCount() const;
  int ColumnCount() const;

  int RowIndex() const;
  int RowSpan() const;
  int ColumnIndex() const;
  int ColumnSpan() const;

  float RangeMin() const;
  float RangeMax() const;
  float RangeCurrentValue() const;

 private:
  // This gives BrowserAccessibility::Create access to the class constructor.
  friend class BrowserAccessibility;

  BrowserAccessibilityAndroid();

  bool HasOnlyStaticTextChildren() const;
  bool IsIframe() const;

  void NotifyLiveRegionUpdate(base::string16& aria_live);

  int CountChildrenWithRole(ui::AXRole role) const;

  base::string16 cached_text_;
  bool first_time_;
  base::string16 old_value_;
  base::string16 new_value_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityAndroid);
};

}  // namespace content

#endif // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_ANDROID_H_
