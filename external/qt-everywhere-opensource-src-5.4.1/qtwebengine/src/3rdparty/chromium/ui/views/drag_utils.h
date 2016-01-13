// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_DRAG_UTILS_H_
#define UI_VIEWS_DRAG_UTILS_H_

#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/views_export.h"

namespace gfx {
class Canvas;
class Point;
class Size;
}

namespace ui {
class OSExchangeData;
}

namespace views {
class Widget;

// Starts a drag operation. This blocks until the drag operation completes.
VIEWS_EXPORT void RunShellDrag(gfx::NativeView view,
                               const ui::OSExchangeData& data,
                               const gfx::Point& location,
                               int operation,
                               ui::DragDropTypes::DragEventSource source);

// Returns a canvas that can be used to draw the drag image. Caller owns the
// returned object. |widget| is Widget hosting the view being dragged.
VIEWS_EXPORT gfx::Canvas* GetCanvasForDragImage(Widget* widget,
                                                const gfx::Size& canvas_size);

}  // namespace views

#endif  // UI_VIEWS_DRAG_UTILS_H_
