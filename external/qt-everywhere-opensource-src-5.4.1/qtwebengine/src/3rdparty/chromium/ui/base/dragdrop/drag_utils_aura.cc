// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/drag_utils.h"

#include "base/logging.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/size.h"
#include "ui/gfx/vector2d.h"

namespace drag_utils {

void SetDragImageOnDataObject(const gfx::ImageSkia& image,
                              const gfx::Vector2d& cursor_offset,
                              ui::OSExchangeData* data_object) {

  data_object->provider().SetDragImage(image, cursor_offset);
}

}  // namespace drag_utils
