// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BinaryDataFontFaceSource_h
#define BinaryDataFontFaceSource_h

#include "core/css/CSSFontFaceSource.h"
#include <memory>

namespace blink {

class FontCustomPlatformData;
class SharedBuffer;

class BinaryDataFontFaceSource final : public CSSFontFaceSource {
public:
    BinaryDataFontFaceSource(SharedBuffer*, String&);
    ~BinaryDataFontFaceSource() override;
    bool isValid() const override;

private:
    PassRefPtr<SimpleFontData> createFontData(const FontDescription&) override;

    std::unique_ptr<FontCustomPlatformData> m_customPlatformData;
};

} // namespace blink

#endif
