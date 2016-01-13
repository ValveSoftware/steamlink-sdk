// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_TOOLTIP_AURA_H_
#define UI_VIEWS_COREWM_TOOLTIP_AURA_H_

#include "ui/gfx/screen_type_delegate.h"
#include "ui/views/controls/label.h"
#include "ui/views/corewm/tooltip.h"
#include "ui/views/widget/widget_observer.h"

namespace gfx {
class FontList;
}  // namespace gfx

namespace views {

class Widget;

namespace corewm {

// Implementation of Tooltip that shows the tooltip using a Widget and Label.
class VIEWS_EXPORT TooltipAura : public Tooltip, public WidgetObserver {
 public:
  explicit TooltipAura(gfx::ScreenType screen_type);
  virtual ~TooltipAura();

  // Trims the tooltip to fit in the width |max_width|, setting |text| to the
  // clipped result, |width| to the width (in pixels) of the clipped text
  // and |line_count| to the number of lines of text in the tooltip. |font_list|
  // is used to layout |text|. |max_width| comes from GetMaxWidth().
  static void TrimTooltipToFit(const gfx::FontList& font_list,
                               int max_width,
                               base::string16* text,
                               int* width,
                               int* line_count);

 private:
  // Returns the max width of the tooltip when shown at the specified location.
  int GetMaxWidth(const gfx::Point& location) const;

  // Adjusts the bounds given by the arguments to fit inside the desktop
  // and applies the adjusted bounds to the label_.
  void SetTooltipBounds(const gfx::Point& mouse_pos,
                        int tooltip_width,
                        int tooltip_height);

  // Destroys |widget_|.
  void DestroyWidget();

  // Tooltip:
  virtual void SetText(aura::Window* window,
                       const base::string16& tooltip_text,
                       const gfx::Point& location) OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual bool IsVisible() OVERRIDE;

  // WidgetObserver:
  virtual void OnWidgetDestroying(Widget* widget) OVERRIDE;

  const gfx::ScreenType screen_type_;

  // The label showing the tooltip.
  Label label_;

  // The widget containing the tooltip. May be NULL.
  Widget* widget_;

  // The window we're showing the tooltip for. Never NULL and valid while
  // showing.
  aura::Window* tooltip_window_;

  DISALLOW_COPY_AND_ASSIGN(TooltipAura);
};

}  // namespace corewm
}  // namespace views

#endif  // UI_VIEWS_COREWM_TOOLTIP_AURA_H_
