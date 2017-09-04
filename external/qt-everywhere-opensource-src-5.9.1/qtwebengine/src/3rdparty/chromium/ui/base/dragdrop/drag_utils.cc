// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/drag_utils.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "url/gurl.h"

namespace drag_utils {

namespace {

// Maximum width of the link drag image in pixels.
static const int kLinkDragImageVPadding = 3;

// File dragging pixel measurements
static const int kFileDragImageMaxWidth = 200;
static const SkColor kFileDragImageTextColor = SK_ColorBLACK;

class FileDragImageSource : public gfx::CanvasImageSource {
 public:
  FileDragImageSource(const base::FilePath& file_name,
                      const gfx::ImageSkia& icon)
      : CanvasImageSource(CalculateSize(icon), false),
        file_name_(file_name),
        icon_(icon) {
  }

  ~FileDragImageSource() override {}

  // Overridden from gfx::CanvasImageSource.
  void Draw(gfx::Canvas* canvas) override {
    if (!icon_.isNull()) {
      // Paint the icon.
      canvas->DrawImageInt(icon_, (size().width() - icon_.width()) / 2, 0);
    }

    base::string16 name = file_name_.BaseName().LossyDisplayName();
    const int flags = gfx::Canvas::TEXT_ALIGN_CENTER;
    const gfx::FontList font_list;
#if defined(OS_WIN)
    // Paint the file name. We inset it one pixel to allow room for the halo.
    const gfx::Rect rect(1, icon_.height() + kLinkDragImageVPadding + 1,
                         size().width() - 2, font_list.GetHeight());
    canvas->DrawStringRectWithHalo(name, font_list, kFileDragImageTextColor,
                                   SK_ColorWHITE, rect, flags);
#else
    // NO_SUBPIXEL_RENDERING is required when drawing to a non-opaque canvas.
    const gfx::Rect rect(0, icon_.height() + kLinkDragImageVPadding,
                         size().width(), font_list.GetHeight());
    canvas->DrawStringRectWithFlags(name, font_list, kFileDragImageTextColor,
                                    rect,
                                    flags | gfx::Canvas::NO_SUBPIXEL_RENDERING);
#endif
  }

 private:
  gfx::Size CalculateSize(const gfx::ImageSkia& icon) const {
    const int width = kFileDragImageMaxWidth;
    // Add +2 here to allow room for the halo.
    const int height = gfx::FontList().GetHeight() + icon.height() +
                       kLinkDragImageVPadding + 2;
    return gfx::Size(width, height);
  }

  const base::FilePath file_name_;
  const gfx::ImageSkia icon_;

  DISALLOW_COPY_AND_ASSIGN(FileDragImageSource);
};

}  // namespace

void CreateDragImageForFile(const base::FilePath& file_name,
                            const gfx::ImageSkia& icon,
                            ui::OSExchangeData* data_object) {
  DCHECK(data_object);
  gfx::CanvasImageSource* source = new FileDragImageSource(file_name, icon);
  gfx::Size size = source->size();
  // ImageSkia takes ownership of |source|.
  gfx::ImageSkia image = gfx::ImageSkia(source, size);

  gfx::Vector2d cursor_offset(size.width() / 2, kLinkDragImageVPadding);
  SetDragImageOnDataObject(image, cursor_offset, data_object);
}

void SetDragImageOnDataObject(const gfx::Canvas& canvas,
                              const gfx::Vector2d& cursor_offset,
                              ui::OSExchangeData* data_object) {
  gfx::ImageSkia image = gfx::ImageSkia(canvas.ExtractImageRep());
  SetDragImageOnDataObject(image, cursor_offset, data_object);
}

#if !defined(OS_WIN)
void SetDragImageOnDataObject(const gfx::ImageSkia& image_skia,
                              const gfx::Vector2d& cursor_offset,
                              ui::OSExchangeData* data_object) {
  data_object->provider().SetDragImage(image_skia, cursor_offset);
}
#endif

}  // namespace drag_utils
