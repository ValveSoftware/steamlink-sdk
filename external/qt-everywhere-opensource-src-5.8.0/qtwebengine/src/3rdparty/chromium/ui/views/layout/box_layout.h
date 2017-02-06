// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_BOX_LAYOUT_H_
#define UI_VIEWS_LAYOUT_BOX_LAYOUT_H_

#include <map>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/gfx/geometry/insets.h"
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
    // TODO(calamity): Add MAIN_AXIS_ALIGNMENT_JUSTIFY which spreads blank space
    // in-between the child views.
  };

  // This specifies where along the cross axis the children should be laid out.
  // e.g. a horizontal layout of CROSS_AXIS_ALIGNMENT_END will result in the
  // child views being bottom-aligned.
  enum CrossAxisAlignment {
    // This causes the child view to stretch to fit the host in the cross axis.
    CROSS_AXIS_ALIGNMENT_STRETCH,
    CROSS_AXIS_ALIGNMENT_START,
    CROSS_AXIS_ALIGNMENT_CENTER,
    CROSS_AXIS_ALIGNMENT_END,
  };

  // Use |inside_border_horizontal_spacing| and
  // |inside_border_vertical_spacing| to add additional space between the child
  // view area and the host view border. |between_child_spacing| controls the
  // space in between child views.
  BoxLayout(Orientation orientation,
            int inside_border_horizontal_spacing,
            int inside_border_vertical_spacing,
            int between_child_spacing);
  ~BoxLayout() override;

  void set_main_axis_alignment(MainAxisAlignment main_axis_alignment) {
    main_axis_alignment_ = main_axis_alignment;
  }

  void set_cross_axis_alignment(CrossAxisAlignment cross_axis_alignment) {
    cross_axis_alignment_ = cross_axis_alignment;
  }

  void set_inside_border_insets(const gfx::Insets& insets) {
    inside_border_insets_ = insets;
  }

  void set_minimum_cross_axis_size(int size) {
    minimum_cross_axis_size_ = size;
  }

  // Sets the flex weight for the given |view|. Using the preferred size as
  // the basis, free space along the main axis is distributed to views in the
  // ratio of their flex weights. Similarly, if the views will overflow the
  // parent, space is subtracted in these ratios.
  //
  // A flex of 0 means this view is not resized. Flex values must not be
  // negative.
  void SetFlexForView(const View* view, int flex);

  // Clears the flex for the given |view|, causing it to use the default
  // flex.
  void ClearFlexForView(const View* view);

  // Sets the flex for views to use when none is specified.
  void SetDefaultFlex(int default_flex);

  // Overridden from views::LayoutManager:
  void Installed(View* host) override;
  void Uninstalled(View* host) override;
  void ViewRemoved(View* host, View* view) override;
  void Layout(View* host) override;
  gfx::Size GetPreferredSize(const View* host) const override;
  int GetPreferredHeightForWidth(const View* host, int width) const override;

 private:
  // Returns the flex for the specified |view|.
  int GetFlexForView(const View* view) const;

  // Returns the size and position along the main axis of |rect|.
  int MainAxisSize(const gfx::Rect& rect) const;
  int MainAxisPosition(const gfx::Rect& rect) const;

  // Sets the size and position along the main axis of |rect|.
  void SetMainAxisSize(int size, gfx::Rect* rect) const;
  void SetMainAxisPosition(int position, gfx::Rect* rect) const;

  // Returns the size and position along the cross axis of |rect|.
  int CrossAxisSize(const gfx::Rect& rect) const;
  int CrossAxisPosition(const gfx::Rect& rect) const;

  // Sets the size and position along the cross axis of |rect|.
  void SetCrossAxisSize(int size, gfx::Rect* rect) const;
  void SetCrossAxisPosition(int size, gfx::Rect* rect) const;

  // Returns the main axis size for the given view. |child_area_width| is needed
  // to calculate the height of the view when the orientation is vertical.
  int MainAxisSizeForView(const View* view, int child_area_width) const;

  // Returns the cross axis size for the given view.
  int CrossAxisSizeForView(const View* view) const;

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

  // The alignment of children in the cross axis. This is
  // CROSS_AXIS_ALIGNMENT_STRETCH by default.
  CrossAxisAlignment cross_axis_alignment_;

  // A map of views to their flex weights.
  std::map<const View*, int> flex_map_;

  // The flex weight for views if none is set. Defaults to 0.
  int default_flex_;

  // The minimum cross axis size for the layout.
  int minimum_cross_axis_size_;

  // The view that this BoxLayout is managing the layout for.
  views::View* host_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(BoxLayout);
};

} // namespace views

#endif  // UI_VIEWS_LAYOUT_BOX_LAYOUT_H_
