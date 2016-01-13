// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_LINUX_FONT_DELEGATE_H_
#define UI_GFX_LINUX_FONT_DELEGATE_H_

#include <string>

#include "ui/gfx/font_render_params_linux.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

// Allows a Linux platform specific overriding of font preferences.
class GFX_EXPORT LinuxFontDelegate {
 public:
  virtual ~LinuxFontDelegate() {}

  // Sets the dynamically loaded singleton that provides font preferences.
  // This pointer is not owned, and if this method is called a second time,
  // the first instance is not deleted.
  static void SetInstance(LinuxFontDelegate* instance);

  // Returns a LinuxFontDelegate instance for the toolkit used in
  // the user's desktop environment.
  //
  // Can return NULL, in case no toolkit has been set. (For example, if we're
  // running with the "--ash" flag.)
  static const LinuxFontDelegate* instance();

  // Whether we should antialias our text.
  virtual bool UseAntialiasing() const = 0;

  // What sort of text hinting should we apply?
  virtual FontRenderParams::Hinting GetHintingStyle() const = 0;

  // What sort of subpixel rendering should we perform.
  virtual FontRenderParams::SubpixelRendering
      GetSubpixelRenderingStyle() const = 0;

  // Returns the default font name for pango style rendering. The format is a
  // string of the form "[font name] [font size]".
  virtual std::string GetDefaultFontName() const = 0;
};

}  // namespace gfx

#endif  // UI_GFX_LINUX_FONT_DELEGATE_H_
