// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_BROWSER_FONT_RESOURCE_TRUSTED_H_
#define CONTENT_CHILD_BROWSER_FONT_RESOURCE_TRUSTED_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/shared_impl/ppapi_preferences.h"
#include "ppapi/thunk/ppb_browser_font_trusted_api.h"

class SkCanvas;

namespace blink {
class WebFont;
}

namespace content {

class BrowserFontResource_Trusted
    : public ppapi::proxy::PluginResource,
      public ppapi::thunk::PPB_BrowserFont_Trusted_API {
 public:
  BrowserFontResource_Trusted(ppapi::proxy::Connection connection,
                              PP_Instance instance,
                              const PP_BrowserFont_Trusted_Description& desc,
                              const ppapi::Preferences& prefs);

  // Validates the parameters in thee description. Can be called on any thread.
  static bool IsPPFontDescriptionValid(
      const PP_BrowserFont_Trusted_Description& desc);

  // Resource override.
  virtual ::ppapi::thunk::PPB_BrowserFont_Trusted_API*
      AsPPB_BrowserFont_Trusted_API() OVERRIDE;

  // PPB_BrowserFont_Trusted_API implementation.
  virtual PP_Bool Describe(PP_BrowserFont_Trusted_Description* description,
                           PP_BrowserFont_Trusted_Metrics* metrics) OVERRIDE;
  virtual PP_Bool DrawTextAt(PP_Resource image_data,
                             const PP_BrowserFont_Trusted_TextRun* text,
                             const PP_Point* position,
                             uint32_t color,
                             const PP_Rect* clip,
                             PP_Bool image_data_is_opaque) OVERRIDE;
  virtual int32_t MeasureText(
      const PP_BrowserFont_Trusted_TextRun* text) OVERRIDE;
  virtual uint32_t CharacterOffsetForPixel(
      const PP_BrowserFont_Trusted_TextRun* text,
      int32_t pixel_position) OVERRIDE;
  virtual int32_t PixelOffsetForCharacter(
      const PP_BrowserFont_Trusted_TextRun* text,
      uint32_t char_offset) OVERRIDE;

 private:
  virtual ~BrowserFontResource_Trusted();

  // Internal version of DrawTextAt that takes a mapped PlatformCanvas.
  void DrawTextToCanvas(SkCanvas* destination,
                        const PP_BrowserFont_Trusted_TextRun& text,
                        const PP_Point* position,
                        uint32_t color,
                        const PP_Rect* clip,
                        PP_Bool image_data_is_opaque);

 private:
  scoped_ptr<blink::WebFont> font_;

  DISALLOW_COPY_AND_ASSIGN(BrowserFontResource_Trusted);
};

}  // namespace content

#endif  // CONTENT_CHILD_BROWSER_FONT_RESOURCE_TRUSTED_H_
