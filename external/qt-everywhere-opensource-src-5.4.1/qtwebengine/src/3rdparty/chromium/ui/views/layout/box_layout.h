// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_BOX_LAYOUT_H_
#define UI_VIEWS_LAYOUT_BOX_LAYOUT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/gfx/insets.h"
#include "ui/views/layout/layout_manager.h"

namespace gfx {
class Rect;
class Size;
}

namespace views {

class View;

// A Layout manager that arranges child views vertically or horizontally in a
// side-by-side fashion with spacing around and between the child views. The
// child views are always sized according to their preferred size. If the
// host's bounds provide insufficient space, child views will be clamped.
// Excess space will not be distributed.
class VIEWS_EXPORT BoxLayout : public LayoutManager {
 public:
  enum Orientation {
    kHorizontal,
    kVertical,
  };

  // This specifies where along the main axis the children should be laid out.
  // e.g. a horizontal layout of MAIN_AXIS_ALIGNMENT_END will result in the
  // child views being right-aligned.
  enum MainAxisAlignment {
    MAIN_AXIS_ALIGNMENT_START,
    MAIN_AXIS_ALIGNMENT_CENTER,
    MAIN_AXIS_ALIGNMENT_END,

    // This distributes extra space among the child views. This increases the
    // size of child views along the main axis rather than the space between
    // them.
    MAIN_AXIS_ALIGNMENT_FILL,
    // TODO(calamity): Add MAIN_AXIS_ALIGNMENT_JUSTIFY which spreads blank space
    // in-between the child views.
  };

  // TODO(calamity): Add CrossAxisAlignment property to allow cross axis
  // alignment.

  // Use |inside_border_horizontal_spacing| and
  // |inside_border_vertical_spacing| to add additional space between the child
  // view area and the host view border. |between_child_spacing| controls the
  // space in between child views.
  BoxLayout(Orientation orientation,
            int inside_border_horizontal_spacing,
            int inside_border_vertical_spacing,
            int between_child_spacing);
  virtual ~BoxLayout();

  void set_main_axis_alignment(MainAxisAlignment main_axis_alignment) {
    main_axis_alignment_ = main_axis_alignment;
  }

  void set_inside_border_insets(const gfx::Insets& insets) {
    inside_border_insets_ = insets;
  }

  // Overridden from views::LayoutManager:
  virtual void Layout(View* host) OVERRIDE;
  virtual gfx::Size GetPreferredSize(const View* host) const OVERRIDE;
  virtual int GetPreferredHeightForWidth(const View* host,
                                         int width) const OVERRIDE;

 private:
  // Returns the size and position along the main axis of |child_area|.
  int MainAxisSize(const gfx::Rect& child_area) const;
  int MainAxisPosition(const gfx::Rect& child_area) const;

  // Sets the size and position along the main axis of |child_area|.
  void SetMainAxisSize(int size, gfx::Rect* child_area) const;
  void SetMainAxisPosition(int position, gfx::Rect* child_area) const;

  // The preferred size for the dialog given the width of the child area.
  gfx::Size GetPreferredSizeForChildWidth(const View* host,
                                          int child_area_width) const;

  // The amount of space the layout requires in addition to any space for the
  // child views.
  gfx::Size NonChildSize(const View* host) const;

  const Orientation orientation_;

  // Spacing between child views and host view border.
  gfx::Insets inside_border_insets_;

  // Spacing to put in between child views.
  const int between_child_spacing_;

  // The alignment of children in the main axis. This is
  // MAIN_AXIS_ALIGNMENT_START by default.
  MainAxisAlignment main_axis_alignment_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(BoxLayout);
};

} // namespace views

#endif  // UI_VIEWS_LAYOUT_BOX_LAYOUT_H_
