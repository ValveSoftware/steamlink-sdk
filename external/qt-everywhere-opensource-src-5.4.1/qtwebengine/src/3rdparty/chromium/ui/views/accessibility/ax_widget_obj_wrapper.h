// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_AX_WIDGET_OBJ_WRAPPER_H_
#define UI_VIEWS_ACCESSIBILITY_AX_WIDGET_OBJ_WRAPPER_H_

#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/widget/widget_removals_observer.h"

namespace views {
class Widget;

// Describes a |Widget| for use with other AX classes.
class AXWidgetObjWrapper : public AXAuraObjWrapper,
                           public WidgetObserver,
                           public WidgetRemovalsObserver {
 public:
  explicit AXWidgetObjWrapper(Widget* widget);
  virtual ~AXWidgetObjWrapper();

  // AXAuraObjWrapper overrides.
  virtual AXAuraObjWrapper* GetParent() OVERRIDE;
  virtual void GetChildren(
      std::vector<AXAuraObjWrapper*>* out_children) OVERRIDE;
  virtual void Serialize(ui::AXNodeData* out_node_data) OVERRIDE;
  virtual int32 GetID() OVERRIDE;

  // WidgetObserver overrides.
  virtual void OnWidgetDestroying(Widget* widget) OVERRIDE;

  // WidgetRemovalsObserver overrides.
  virtual void OnWillRemoveView(Widget* widget, View* view) OVERRIDE;

 private:
  Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(AXWidgetObjWrapper);
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_AX_WIDGET_OBJ_WRAPPER_H_
