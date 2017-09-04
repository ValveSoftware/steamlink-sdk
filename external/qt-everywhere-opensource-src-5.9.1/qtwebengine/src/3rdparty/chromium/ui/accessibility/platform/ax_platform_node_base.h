// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_BASE_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_BASE_H_

#include "base/macros.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

struct AXNodeData;
class AXPlatformNodeDelegate;

class AXPlatformNodeBase : public AXPlatformNode {
 public:
   virtual void Init(AXPlatformNodeDelegate* delegate);

  // These are simple wrappers to our delegate.
  const AXNodeData& GetData() const;
  gfx::Rect GetBoundsInScreen() const;
  gfx::NativeViewAccessible GetParent();
  int GetChildCount();
  gfx::NativeViewAccessible ChildAtIndex(int index);

  // This needs to be implemented for each platform.
  virtual int GetIndexInParent() = 0;

  // AXPlatformNode.
  void Destroy() override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  AXPlatformNodeDelegate* GetDelegate() const override;

  // Helpers.
  AXPlatformNodeBase* GetPreviousSibling();
  AXPlatformNodeBase* GetNextSibling();
  bool IsDescendant(AXPlatformNodeBase* descendant);

  bool HasBoolAttribute(ui::AXBoolAttribute attr) const;
  bool GetBoolAttribute(ui::AXBoolAttribute attr) const;
  bool GetBoolAttribute(ui::AXBoolAttribute attr, bool* value) const;

  bool HasFloatAttribute(ui::AXFloatAttribute attr) const;
  float GetFloatAttribute(ui::AXFloatAttribute attr) const;
  bool GetFloatAttribute(ui::AXFloatAttribute attr, float* value) const;

  bool HasIntAttribute(ui::AXIntAttribute attribute) const;
  int GetIntAttribute(ui::AXIntAttribute attribute) const;
  bool GetIntAttribute(ui::AXIntAttribute attribute, int* value) const;

  bool HasStringAttribute(
      ui::AXStringAttribute attribute) const;
  const std::string& GetStringAttribute(ui::AXStringAttribute attribute) const;
  bool GetStringAttribute(ui::AXStringAttribute attribute,
                          std::string* value) const;
  bool GetString16Attribute(ui::AXStringAttribute attribute,
                            base::string16* value) const;
  base::string16 GetString16Attribute(
      ui::AXStringAttribute attribute) const;

  AXPlatformNodeDelegate* delegate_;  // Weak. Owns this.

 protected:
  AXPlatformNodeBase();
  ~AXPlatformNodeBase() override;

  // Cast a gfx::NativeViewAccessible to an AXPlatformNodeBase if it is one,
  // or return NULL if it's not an instance of this class.
  static AXPlatformNodeBase* FromNativeViewAccessible(
      gfx::NativeViewAccessible accessible);

  virtual void Dispose();

 private:
  DISALLOW_COPY_AND_ASSIGN(AXPlatformNodeBase);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_BASE_H_
