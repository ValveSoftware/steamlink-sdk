// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_TRUETYPE_FONT_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_TRUETYPE_FONT_H_

#include <string>
#include <vector>

#include "ppapi/proxy/serialized_structs.h"

namespace content {

class PepperTrueTypeFont {
 public:
  // Creates a font matching the given descriptor. The exact font that is
  // returned will depend on the host platform's font matching and fallback
  // algorithm.
  static PepperTrueTypeFont* Create(
      const ppapi::proxy::SerializedTrueTypeFontDesc& desc);
  virtual ~PepperTrueTypeFont() {}

  // Returns true if the font was successfully created, false otherwise.
  virtual bool IsValid() = 0;

  // Returns a description of the actual font. Use this to see the actual
  // characteristics of the font after running the host platform's font matching
  // and fallback algorithm. Returns PP_OK on success, a Pepper error code on
  // failure. 'desc' is written only on success.
  virtual int32_t Describe(ppapi::proxy::SerializedTrueTypeFontDesc* desc) = 0;

  // Retrieves an array of TrueType table tags contained in this font. Returns
  // the number of tags on success, a Pepper error code on failure. 'tags' are
  // written only on success.
  virtual int32_t GetTableTags(std::vector<uint32_t>* tags) = 0;

  // Gets a TrueType font table corresponding to the given tag. The 'offset' and
  // 'max_data_length' parameters determine what part of the table is returned.
  // Returns the data size in bytes on success, a Pepper error code on failure.
  // 'data' is written only on success.
  virtual int32_t GetTable(uint32_t table_tag,
                           int32_t offset,
                           int32_t max_data_length,
                           std::string* data) = 0;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_TRUETYPE_FONT_H_
