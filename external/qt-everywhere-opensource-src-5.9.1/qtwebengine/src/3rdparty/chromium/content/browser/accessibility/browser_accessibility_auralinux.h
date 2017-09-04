// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_AURALINUX_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_AURALINUX_H_

#include <atk/atk.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/common/content_export.h"

namespace content {

class BrowserAccessibilityAuraLinux;

G_BEGIN_DECLS

#define BROWSER_ACCESSIBILITY_TYPE (browser_accessibility_get_type())
#define BROWSER_ACCESSIBILITY(obj)                               \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), BROWSER_ACCESSIBILITY_TYPE, \
                              BrowserAccessibilityAtk))
#define BROWSER_ACCESSIBILITY_CLASS(klass)                      \
  (G_TYPE_CHECK_CLASS_CAST((klass), BROWSER_ACCESSIBILITY_TYPE, \
                           BrowserAccessibilityAtkClass))
#define IS_BROWSER_ACCESSIBILITY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), BROWSER_ACCESSIBILITY_TYPE))
#define IS_BROWSER_ACCESSIBILITY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), BROWSER_ACCESSIBILITY_TYPE))
#define BROWSER_ACCESSIBILITY_GET_CLASS(obj)                    \
  (G_TYPE_INSTANCE_GET_CLASS((obj), BROWSER_ACCESSIBILITY_TYPE, \
                             BrowserAccessibilityAtkClass))

typedef struct _BrowserAccessibilityAtk BrowserAccessibilityAtk;
typedef struct _BrowserAccessibilityAtkClass BrowserAccessibilityAtkClass;

struct _BrowserAccessibilityAtk {
  AtkObject parent;
  BrowserAccessibilityAuraLinux* m_object;
};

struct _BrowserAccessibilityAtkClass {
  AtkObjectClass parent_class;
};

GType browser_accessibility_get_type(void) G_GNUC_CONST;

BrowserAccessibilityAtk* browser_accessibility_new(
    BrowserAccessibilityAuraLinux* object);

BrowserAccessibilityAuraLinux* browser_accessibility_get_object(
    BrowserAccessibilityAtk* atk_object);

void browser_accessibility_detach(BrowserAccessibilityAtk* atk_object);

AtkObject* browser_accessibility_get_focused_element(
    BrowserAccessibilityAtk* atk_object);

G_END_DECLS

class BrowserAccessibilityAuraLinux : public BrowserAccessibility {
 public:
  BrowserAccessibilityAuraLinux();

  ~BrowserAccessibilityAuraLinux() override;

  AtkObject* GetAtkObject() const;

  AtkRole atk_role() { return atk_role_; }

  // BrowserAccessibility methods.
  void OnDataChanged() override;
  bool IsNative() const override;

 private:
  virtual void InitRoleAndState();

  // Give BrowserAccessibility::Create access to our constructor.
  friend class BrowserAccessibility;

  AtkObject* atk_object_;
  AtkRole atk_role_;
  int interface_mask_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityAuraLinux);
};

CONTENT_EXPORT const BrowserAccessibilityAuraLinux*
ToBrowserAccessibilityAuraLinux(const BrowserAccessibility* obj);

CONTENT_EXPORT BrowserAccessibilityAuraLinux* ToBrowserAccessibilityAuraLinux(
    BrowserAccessibility* obj);

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_AURALINUX_H_
