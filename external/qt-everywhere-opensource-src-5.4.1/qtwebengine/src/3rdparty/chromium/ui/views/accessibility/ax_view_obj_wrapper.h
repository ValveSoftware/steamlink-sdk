// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_AX_VIEW_OBJ_WRAPPER_H_
#define UI_VIEWS_ACCESSIBILITY_AX_VIEW_OBJ_WRAPPER_H_

#include "ui/views/accessibility/ax_aura_obj_wrapper.h"

namespace views {
class View;

// Describes a |View| for use with other AX classes.
class AXViewObjWrapper : public AXAuraObjWrapper {
 public:
  explicit AXViewObjWrapper(View* view);
  virtual ~AXViewObjWrapper();

  // AXAuraObjWrapper overrides.
  virtual AXAuraObjWrapper* GetParent() OVERRIDE;
  virtual void GetChildren(
      std::vector<AXAuraObjWrapper*>* out_children) OVERRIDE;
  virtual void Serialize(ui::AXNodeData* out_node_data) OVERRIDE;
  virtual int32 GetID() OVERRIDE;
  virtual void DoDefault() OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual void MakeVisible() OVERRIDE;
  virtual void SetSelection(int32 start, int32 end) OVERRIDE;

 private:
  View* view_;

  DISALLOW_COPY_AND_ASSIGN(AXViewObjWrapper);
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_AX_VIEW_OBJ_WRAPPER_H_
