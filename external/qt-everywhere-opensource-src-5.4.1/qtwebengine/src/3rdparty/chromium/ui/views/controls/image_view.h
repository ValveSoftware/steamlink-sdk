// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_IMAGE_VIEW_H_
#define UI_VIEWS_CONTROLS_IMAGE_VIEW_H_

#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"

namespace gfx {
class Canvas;
}

namespace views {

class Painter;

/////////////////////////////////////////////////////////////////////////////
//
// ImageView class.
//
// An ImageView can display an image from an ImageSkia. If a size is provided,
// the ImageView will resize the provided image to fit if it is too big or will
// center the image if smaller. Otherwise, the preferred size matches the
// provided image size.
//
/////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT ImageView : public View {
 public:
  enum Alignment {
    LEADING = 0,
    CENTER,
    TRAILING
  };

  ImageView();
  virtual ~ImageView();

  // Set the image that should be displayed.
  void SetImage(const gfx::ImageSkia& img);

  // Set the image that should be displayed from a pointer. Reset the image
  // if the pointer is NULL. The pointer contents is copied in the receiver's
  // image.
  void SetImage(const gfx::ImageSkia* image_skia);

  // Returns the image currently displayed or NULL of none is currently set.
  // The returned image is still owned by the ImageView.
  const gfx::ImageSkia& GetImage();

  // Set the desired image size for the receiving ImageView.
  void SetImageSize(const gfx::Size& image_size);

  // Return the preferred size for the receiving view. Returns false if the
  // preferred size is not defined, which means that the view uses the image
  // size.
  bool GetImageSize(gfx::Size* image_size) const;

  // Returns the actual bounds of the visible image inside the view.
  gfx::Rect GetImageBounds() const;

  // Reset the image size to the current image dimensions.
  void ResetImageSize();

  // Set / Get the horizontal alignment.
  void SetHorizontalAlignment(Alignment ha);
  Alignment GetHorizontalAlignment() const;

  // Set / Get the vertical alignment.
  void SetVerticalAlignment(Alignment va);
  Alignment GetVerticalAlignment() const;

  // Set / Get the tooltip text.
  void SetTooltipText(const base::string16& tooltip);
  base::string16 GetTooltipText() const;

  void set_interactive(bool interactive) { interactive_ = interactive; }

  void SetFocusPainter(scoped_ptr<Painter> focus_painter);

  // Overriden from View:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE;
  virtual bool GetTooltipText(const gfx::Point& p,
                              base::string16* tooltip) const OVERRIDE;
  virtual bool CanProcessEventsWithinSubtree() const OVERRIDE;

 private:
  void OnPaintImage(gfx::Canvas* canvas);

  // Returns true if |img| is the same as the last image we painted. This is
  // intended to be a quick check, not exhaustive. In other words it's possible
  // for this to return false even though the images are in fact equal.
  bool IsImageEqual(const gfx::ImageSkia& img) const;

  // Compute the image origin given the desired size and the receiver alignment
  // properties.
  gfx::Point ComputeImageOrigin(const gfx::Size& image_size) const;

  // Whether the image size is set.
  bool image_size_set_;

  // The actual image size.
  gfx::Size image_size_;

  // The underlying image.
  gfx::ImageSkia image_;

  // Horizontal alignment.
  Alignment horiz_alignment_;

  // Vertical alignment.
  Alignment vert_alignment_;

  // The current tooltip text.
  base::string16 tooltip_text_;

  // A flag controlling hit test handling for interactivity.
  bool interactive_;

  // Scale last painted at.
  float last_paint_scale_;

  // Address of bytes we last painted. This is used only for comparison, so its
  // safe to cache.
  void* last_painted_bitmap_pixels_;

  scoped_ptr<views::Painter> focus_painter_;

  DISALLOW_COPY_AND_ASSIGN(ImageView);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_IMAGE_VIEW_H_
