// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_MOJO_IME_TYPE_CONVERTERS_H_
#define UI_PLATFORM_WINDOW_MOJO_IME_TYPE_CONVERTERS_H_

#include "ui/platform_window/mojo/mojo_ime_export.h"
#include "ui/platform_window/mojo/text_input_state.mojom.h"
#include "ui/platform_window/text_input_state.h"

namespace mojo {

template <>
struct MOJO_IME_EXPORT TypeConverter<TextInputType, ui::TextInputType> {
  static TextInputType Convert(const ui::TextInputType& input);
};

template <>
struct MOJO_IME_EXPORT TypeConverter<ui::TextInputType, TextInputType> {
  static ui::TextInputType Convert(const TextInputType& input);
};

template <>
struct MOJO_IME_EXPORT TypeConverter<ui::TextInputState, TextInputStatePtr> {
  static ui::TextInputState Convert(const TextInputStatePtr& input);
};

}  // namespace mojo

#endif  // UI_PLATFORM_WINDOW_MOJO_IME_TYPE_CONVERTERS_H_
