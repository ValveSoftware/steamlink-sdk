// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_PUBLIC_TOOLTIP_CLIENT_H_
#define UI_WM_PUBLIC_TOOLTIP_CLIENT_H_

#include "ui/aura/aura_export.h"
#include "ui/gfx/font.h"

namespace gfx {
class Point;
}

namespace aura {
class Window;
namespace client {

class ScopedTooltipDisabler;

class AURA_EXPORT TooltipClient {
 public:
  // Returns the max width of the tooltip when shown at the specified location.
  virtual int GetMaxWidth(const gfx::Point& point) const = 0;

  // Informs the shell tooltip manager of change in tooltip for window |target|.
  virtual void UpdateTooltip(Window* target) = 0;

  // Sets the time after which the tooltip is hidden for Window |target|. If
  // |timeout_in_ms| is <= 0, the tooltip is shown indefinitely.
  virtual void SetTooltipShownTimeout(Window* target, int timeout_in_ms) = 0;

 protected:
  // Enables/Disables tooltips. This is treated as a reference count. Consumers
  // must use ScopedTooltipDisabler to enable/disabled tooltips.
  virtual void SetTooltipsEnabled(bool enable) = 0;

 private:
  friend class ScopedTooltipDisabler;
};

AURA_EXPORT void SetTooltipClient(Window* root_window,
                                  TooltipClient* client);
AURA_EXPORT TooltipClient* GetTooltipClient(Window* root_window);

// Sets the text for the tooltip. The id is used to determine uniqueness when
// the text does not change. For example, if the tooltip text does not change,
// but the id does then the position of the tooltip is updated.
AURA_EXPORT void SetTooltipText(Window* window,
                                base::string16* tooltip_text);
AURA_EXPORT void SetTooltipId(Window* window, void* id);
AURA_EXPORT const base::string16 GetTooltipText(Window* window);
AURA_EXPORT const void* GetTooltipId(Window* window);

}  // namespace client
}  // namespace aura

#endif  // UI_WM_PUBLIC_TOOLTIP_CLIENT_H_
