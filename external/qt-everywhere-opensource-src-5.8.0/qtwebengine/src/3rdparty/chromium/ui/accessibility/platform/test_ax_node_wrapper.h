// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_TEST_AX_NODE_WRAPPER_H_
#define UI_ACCESSIBILITY_PLATFORM_TEST_AX_NODE_WRAPPER_H_

#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"

namespace ui {

// For testing, a TestAXNodeWrapper wraps an AXNode, implements
// AXPlatformNodeDelegate, and owns an AXPlatformNode.
class TestAXNodeWrapper : public AXPlatformNodeDelegate {
 public:
  // Create TestAXNodeWrapper instances on-demand from an AXTree and AXNode.
  // Note that this sets the AXTreeDelegate, you can't use this class if
  // you also want to implement AXTreeDelegate.
  static TestAXNodeWrapper* GetOrCreate(AXTree* tree, AXNode* node);

  // Set a global coordinate offset for testing.
  static void SetGlobalCoordinateOffset(const gfx::Vector2d& offset);

  virtual ~TestAXNodeWrapper();

  AXPlatformNode* ax_platform_node() { return platform_node_; }

  // AXPlatformNodeDelegate.
  const AXNodeData& GetData() override;
  gfx::NativeViewAccessible GetParent() override;
  int GetChildCount() override;
  gfx::NativeViewAccessible ChildAtIndex(int index) override;
  gfx::Vector2d GetGlobalCoordinateOffset() override;
  gfx::NativeViewAccessible HitTestSync(int x, int y) override;
  gfx::NativeViewAccessible GetFocus() override;
  gfx::AcceleratedWidget GetTargetForNativeAccessibilityEvent() override;
  void DoDefaultAction() override;
  bool SetStringValue(const base::string16& new_value) override;

 private:
  TestAXNodeWrapper(AXTree* tree, AXNode* node);

  AXTree* tree_;
  AXNode* node_;
  AXPlatformNode* platform_node_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_TEST_AX_NODE_WRAPPER_H_
