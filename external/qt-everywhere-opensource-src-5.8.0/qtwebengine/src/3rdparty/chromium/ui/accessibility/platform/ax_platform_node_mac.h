// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_PLATFORM_NODE_MAC_H_
#define UI_ACCESSIBILITY_AX_PLATFORM_NODE_MAC_H_

#import <Foundation/Foundation.h>

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/platform/ax_platform_node_base.h"

@class AXPlatformNodeCocoa;

namespace ui {

class AXPlatformNodeMac : public AXPlatformNodeBase {
 public:
  AXPlatformNodeMac();

  // AXPlatformNode.
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  void NotifyAccessibilityEvent(ui::AXEvent event_type) override;

  // AXPlatformNodeBase.
  void Destroy() override;
  int GetIndexInParent() override;

 private:
  ~AXPlatformNodeMac() override;

  base::scoped_nsobject<AXPlatformNodeCocoa> native_node_;

  DISALLOW_COPY_AND_ASSIGN(AXPlatformNodeMac);
};

}  // namespace ui

AX_EXPORT
@interface AXPlatformNodeCocoa : NSObject {
 @private
  ui::AXPlatformNodeBase* node_;  // Weak. Retains us.
}

+ (NSString*)nativeRoleFromAXRole:(ui::AXRole)role;
+ (NSString*)nativeSubroleFromAXRole:(ui::AXRole)role;
+ (NSString*)nativeNotificationFromAXEvent:(ui::AXEvent)event;

- (instancetype)initWithNode:(ui::AXPlatformNodeBase*)node;
- (void)detach;

@property(nonatomic, readonly) NSRect boundsInScreen;

@end

#endif  // UI_ACCESSIBILITY_AX_PLATFORM_NODE_MAC_H_
