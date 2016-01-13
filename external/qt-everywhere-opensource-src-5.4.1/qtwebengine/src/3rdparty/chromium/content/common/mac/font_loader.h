// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MAC_FONT_LOADER_H_
#define CONTENT_COMMON_MAC_FONT_LOADER_H_

#include <ApplicationServices/ApplicationServices.h>

#include "base/memory/shared_memory.h"
#include "content/common/content_export.h"

#ifdef __OBJC__
@class NSFont;
#else
class NSFont;
#endif

struct FontDescriptor;

// Provides functionality to transmit fonts over IPC.
//
// Note about font formats: .dfont (datafork suitcase) fonts are currently not
// supported by this code since CGFontCreateWithDataProvider() can't handle them
// directly.

class FontLoader {
 public:
  // This structure holds the result of LoadFont(). This structure is passed to
  // LoadFont(), which should run on the file thread, then it is passed to a
  // task which sends the result to the originating renderer.
  struct Result {
    uint32 font_data_size;
    base::SharedMemory font_data;
    uint32 font_id;
  };
  // Load a font specified by |font| into a shared memory buffer suitable for
  // sending over IPC.
  //
  // On return:
  //  |result->font_data| - shared memory buffer containing the raw data for
  // the font file. The buffer is only valid when both |result->font_data_size|
  // and |result->font_id| are not zero.
  //  |result->font_data_size| - size of data contained in |result->font_data|.
  // This value is zero on failure.
  //  |result->font_id| - unique identifier for the on-disk file we load for
  // the font. This value is zero on failure.
  CONTENT_EXPORT
  static void LoadFont(const FontDescriptor& font, FontLoader::Result* result);

  // Given a shared memory buffer containing the raw data for a font file, load
  // the font and return a CGFontRef.
  //
  // |data| - A shared memory handle pointing to the raw data from a font file.
  // |data_size| - Size of |data|.
  //
  // On return:
  //  returns true on success, false on failure.
  //  |out| - A CGFontRef corresponding to the designated font.
  //  The caller is responsible for releasing this value via CGFontRelease()
  //  when done.
  CONTENT_EXPORT
  static bool CGFontRefFromBuffer(base::SharedMemoryHandle font_data,
                                  uint32 font_data_size,
                                  CGFontRef* out);
};

#endif // CONTENT_COMMON_MAC_FONT_LOADER_H_
