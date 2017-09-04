// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/FontTestHelpers.h"

#include "platform/fonts/Font.h"
#include "platform/fonts/FontCustomPlatformData.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontSelector.h"
#include "platform/testing/UnitTestHelpers.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {
namespace testing {

namespace {

class TestFontSelector : public FontSelector {
 public:
  static TestFontSelector* create(const String& path) {
    RefPtr<SharedBuffer> fontBuffer = testing::readFromFile(path);
    String otsParseMessage;
    return new TestFontSelector(
        FontCustomPlatformData::create(fontBuffer.get(), otsParseMessage));
  }

  ~TestFontSelector() override {}

  PassRefPtr<FontData> getFontData(const FontDescription& fontDescription,
                                   const AtomicString& familyName) override {
    FontPlatformData platformData = m_customPlatformData->fontPlatformData(
        fontDescription.effectiveFontSize(), fontDescription.isSyntheticBold(),
        fontDescription.isSyntheticItalic(), fontDescription.orientation());
    return SimpleFontData::create(platformData, CustomFontData::create());
  }

  void willUseFontData(const FontDescription&,
                       const AtomicString& familyName,
                       const String& text) override {}
  void willUseRange(const FontDescription&,
                    const AtomicString& familyName,
                    const FontDataForRangeSet&) override{};

  unsigned version() const override { return 0; }
  void fontCacheInvalidated() override {}

 private:
  TestFontSelector(std::unique_ptr<FontCustomPlatformData> customPlatformData)
      : m_customPlatformData(std::move(customPlatformData)) {}

  std::unique_ptr<FontCustomPlatformData> m_customPlatformData;
};

}  // namespace

Font createTestFont(const AtomicString& familyName,
                    const String& fontPath,
                    float size) {
  FontFamily family;
  family.setFamily(familyName);

  FontDescription fontDescription;
  fontDescription.setFamily(family);
  fontDescription.setSpecifiedSize(size);
  fontDescription.setComputedSize(size);

  Font font(fontDescription);
  font.update(TestFontSelector::create(fontPath));
  return font;
}

}  // namespace testing
}  // namespace blink
