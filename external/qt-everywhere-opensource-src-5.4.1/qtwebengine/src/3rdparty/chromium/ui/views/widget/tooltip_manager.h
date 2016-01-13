// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_TOOLTIP_MANAGER_H_
#define UI_VIEWS_WIDGET_TOOLTIP_MANAGER_H_

#include <string>

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/views_export.h"

namespace gfx {
class Display;
class FontList;
}  // namespace gfx

namespace views {

class View;

// TooltipManager takes care of the wiring to support tooltips for Views. You
// almost never need to interact directly with TooltipManager, rather look to
// the various tooltip methods on View.
class VIEWS_EXPORT TooltipManager {
 public:
  // When a NativeView has capture all events are delivered to it. In some
  // situations, such as menus, we want the tooltip to be shown for the
  // NativeView the mouse is over, even if it differs from the NativeView that
  // has capture (with menus the first menu shown has capture). To enable this
  // if the NativeView that has capture has the same value for the property
  // |kGroupingPropertyKey| as the NativeView the mouse is over the tooltip is
  // shown.
  static const char kGroupingPropertyKey[];

  TooltipManager() {}
  virtual ~TooltipManager() {}

  // Returns the height of tooltips. This should only be invoked from within
  // GetTooltipTextOrigin.
  static int GetTooltipHeight();

  // Returns the maximum width of the tooltip. |x| and |y| give the location
  // the tooltip is to be displayed on in screen coordinates. |context| is
  // used to determine which gfx::Screen should be used.
  static int GetMaxWidth(int x, int y, gfx::NativeView context);

  // Same as GetMaxWidth(), but takes a Display.
  static int GetMaxWidth(const gfx::Display& display);

  // If necessary trims the text of a tooltip to ensure we don't try to display
  // a mega-tooltip.
  static void TrimTooltipText(base::string16* text);

  // Returns the font list used for tooltips.
  virtual const gfx::FontList& GetFontList() const = 0;

  // Notification that the view hierarchy has changed in some way.
  virtual void UpdateTooltip() = 0;

  // Invoked when the tooltip text changes for the specified views.
  virtual void TooltipTextChanged(View* view) = 0;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_TOOLTIP_MANAGER_H_
