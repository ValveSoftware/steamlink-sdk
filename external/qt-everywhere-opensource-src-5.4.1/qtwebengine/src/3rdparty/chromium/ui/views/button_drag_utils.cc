// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/button_drag_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "ui/base/dragdrop/drag_utils.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/drag_utils.h"
#include "url/gurl.h"

namespace button_drag_utils {

// Maximum width of the link drag image in pixels.
static const int kLinkDragImageMaxWidth = 150;

void SetURLAndDragImage(const GURL& url,
                        const base::string16& title,
                        const gfx::ImageSkia& icon,
                        const gfx::Point* press_pt,
                        ui::OSExchangeData* data,
                        views::Widget* widget) {
  DCHECK(url.is_valid() && data);
  data->SetURL(url, title);
  SetDragImage(url, title, icon, press_pt, data, widget);
}

void SetDragImage(const GURL& url,
                  const base::string16& title,
                  const gfx::ImageSkia& icon,
                  const gfx::Point* press_pt,
                  ui::OSExchangeData* data,
                  views::Widget* widget) {
  // Create a button to render the drag image for us.
  views::TextButton button(NULL,
                           title.empty() ? base::UTF8ToUTF16(url.spec())
                                         : title);
  button.set_max_width(kLinkDragImageMaxWidth);
  if (icon.isNull()) {
    button.SetIcon(*ui::ResourceBundle::GetSharedInstance().GetImageNamed(
                   IDR_DEFAULT_FAVICON).ToImageSkia());
  } else {
    button.SetIcon(icon);
  }
  gfx::Size prefsize = button.GetPreferredSize();
  button.SetBounds(0, 0, prefsize.width(), prefsize.height());

  gfx::Vector2d press_point;
  if (press_pt)
    press_point = press_pt->OffsetFromOrigin();
  else
    press_point = gfx::Vector2d(prefsize.width() / 2, prefsize.height() / 2);

  // Render the image.
  scoped_ptr<gfx::Canvas> canvas(
      views::GetCanvasForDragImage(widget, prefsize));
  button.PaintButton(canvas.get(), views::TextButton::PB_FOR_DRAG);
  drag_utils::SetDragImageOnDataObject(*canvas, press_point, data);
}

}  // namespace button_drag_utils
