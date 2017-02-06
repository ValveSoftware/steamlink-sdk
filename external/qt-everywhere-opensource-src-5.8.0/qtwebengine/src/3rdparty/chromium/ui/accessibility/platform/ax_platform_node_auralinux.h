// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_PLATFORM_NODE_AURALINUX_H_
#define UI_ACCESSIBILITY_AX_PLATFORM_NODE_AURALINUX_H_

#include <atk/atk.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/platform/ax_platform_node_base.h"

namespace base {
class TaskRunner;
}

namespace ui {

// Implements accessibility on Aura Linux using ATK.
class AXPlatformNodeAuraLinux : public AXPlatformNodeBase {
 public:
  AXPlatformNodeAuraLinux();

  // Set or get the root-level Application object that's the parent of all
  // top-level windows.
  AX_EXPORT static void SetApplication(AXPlatformNode* application);
  static AXPlatformNode* application() { return application_; }

  // Do static initialization using the given task runner for file operations.
  AX_EXPORT static void StaticInitialize(
      scoped_refptr<base::TaskRunner> init_task_runner);

  AtkRole GetAtkRole();
  void GetAtkState(AtkStateSet* state_set);
  void GetAtkRelations(AtkRelationSet* atk_relation_set);
  void GetExtents(gint* x, gint* y, gint* width, gint* height,
                  AtkCoordType coord_type);
  void GetPosition(gint* x, gint* y, AtkCoordType coord_type);
  void GetSize(gint* width, gint* height);

  void SetExtentsRelativeToAtkCoordinateType(
      gint* x, gint* y, gint* width, gint* height,
      AtkCoordType coord_type);

  // AXPlatformNode overrides.
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  void NotifyAccessibilityEvent(ui::AXEvent event_type) override;

  // AXPlatformNodeBase overrides.
  void Init(AXPlatformNodeDelegate* delegate) override;
  int GetIndexInParent() override;

 private:
  ~AXPlatformNodeAuraLinux() override;

  // We own a reference to this ref-counted object.
  AtkObject* atk_object_;

  // The root-level Application object that's the parent of all
  // top-level windows.
  static AXPlatformNode* application_;

  DISALLOW_COPY_AND_ASSIGN(AXPlatformNodeAuraLinux);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_PLATFORM_NODE_AURALINUX_H_
