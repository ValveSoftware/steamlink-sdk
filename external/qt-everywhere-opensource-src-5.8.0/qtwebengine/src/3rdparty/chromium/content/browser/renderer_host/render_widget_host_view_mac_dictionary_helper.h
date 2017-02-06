// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_MAC_DICTIONARY_HELPER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_MAC_DICTIONARY_HELPER_H_

#include "base/macros.h"
#include "ui/gfx/geometry/vector2d.h"

namespace content {

class RenderWidgetHostView;
class RenderWidgetHostViewMac;

// A helper class to bring up definition of word for a RWHV.
//
// This is triggered by "Lookup in Dictionary" context menu item.
// Either uses Dictionary.app or a light-weight dictionary panel, based on
// system settings.
class RenderWidgetHostViewMacDictionaryHelper {
 public:
  explicit RenderWidgetHostViewMacDictionaryHelper(RenderWidgetHostView* view);

  // Overrides the view to use to bring up dictionary panel.
  // This |target_view| can be different from |view_|, |view_| is used to get
  // the current selection value where |target_view| is used to bring up the
  // cocoa dictionary panel.
  void SetTargetView(RenderWidgetHostView* target_view);
  void set_offset(const gfx::Vector2d& offset) { offset_ = offset; }

  // Brings up either Dictionary.app or a light-weight dictionary panel,
  // depending on system settings.
  void ShowDefinitionForSelection();

 private:
  // This class shows definition for this view.
  RenderWidgetHostViewMac* view_;
  // This view is use to bring up the dictionary panel. Generally this is the
  // same as |view_|. One can override the view to use via SetTargetView().
  RenderWidgetHostViewMac* target_view_;
  // The extra offset to use while positioning the dicitonary panel.
  gfx::Vector2d offset_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewMacDictionaryHelper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_MAC_DICTIONARY_HELPER_H_
