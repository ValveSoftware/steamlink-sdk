// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutTextControlItem_h
#define LayoutTextControlItem_h

#include "core/layout/LayoutTextControl.h"
#include "core/layout/api/LayoutBoxModel.h"
#include "platform/scroll/ScrollTypes.h"

namespace blink {

class LayoutTextControlItem : public LayoutBoxModel {
 public:
  explicit LayoutTextControlItem(LayoutTextControl* layoutTextControl)
      : LayoutBoxModel(layoutTextControl) {}

  explicit LayoutTextControlItem(const LayoutItem& item)
      : LayoutBoxModel(item) {
    SECURITY_DCHECK(!item || item.isTextControl());
  }

  explicit LayoutTextControlItem(std::nullptr_t) : LayoutBoxModel(nullptr) {}

  LayoutTextControlItem() {}

  PassRefPtr<ComputedStyle> createInnerEditorStyle(
      const ComputedStyle& startStyle) const {
    return toTextControl()->createInnerEditorStyle(startStyle);
  }

 private:
  LayoutTextControl* toTextControl() {
    return toLayoutTextControl(layoutObject());
  }

  const LayoutTextControl* toTextControl() const {
    return toLayoutTextControl(layoutObject());
  }
};

}  // namespace blink

#endif  // LayoutTextControlItem_h
