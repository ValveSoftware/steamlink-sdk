// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MOJO_DISPLAY_TYPE_CONVERTERS_H_
#define UI_DISPLAY_MOJO_DISPLAY_TYPE_CONVERTERS_H_

#include "components/mus/public/interfaces/window_manager_constants.mojom.h"
#include "ui/display/display.h"
#include "ui/display/mojo/mojo_display_export.h"

namespace mojo {

template <>
struct MOJO_DISPLAY_EXPORT
    TypeConverter<display::Display, mus::mojom::DisplayPtr> {
  static display::Display Convert(const mus::mojom::DisplayPtr& input);
};

}  // namespace mojo

#endif  // UI_DISPLAY_MOJO_DISPLAY_TYPE_CONVERTERS_H_
