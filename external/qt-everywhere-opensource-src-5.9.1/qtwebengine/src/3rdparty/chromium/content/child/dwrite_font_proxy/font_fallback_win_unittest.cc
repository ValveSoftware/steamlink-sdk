// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/dwrite_font_proxy/font_fallback_win.h"

#include <dwrite.h>
#include <shlobj.h>
#include <wrl.h>

#include <vector>

#include "base/memory/ref_counted.h"
#include "content/child/dwrite_font_proxy/dwrite_font_proxy_win.h"
#include "content/test/dwrite_font_fake_sender_win.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/win/text_analysis_source.h"

namespace mswr = Microsoft::WRL;

namespace content {

namespace {

class FontFallbackUnitTest : public testing::Test {
 public:
  FontFallbackUnitTest() {
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                        &factory_);

    factory_->CreateNumberSubstitution(DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE,
                                       L"en-us", true /* ignoreUserOverride */,
                                       &number_substitution_);

    std::vector<base::char16> font_path;
    font_path.resize(MAX_PATH);
    SHGetSpecialFolderPath(nullptr /* hwndOwner - reserved */, font_path.data(),
                           CSIDL_FONTS, FALSE /* fCreate*/);
    base::string16 segoe_path;
    segoe_path.append(font_path.data()).append(L"\\seguisym.ttf");

    fake_collection_ = new FakeFontCollection();
    fake_collection_->AddFont(L"Segoe UI Symbol")
        .AddFamilyName(L"en-us", L"Segoe UI Symbol")
        .AddFilePath(segoe_path);

    mswr::MakeAndInitialize<DWriteFontCollectionProxy>(
        &collection_, factory_.Get(), fake_collection_->GetSender());
  }

  scoped_refptr<FakeFontCollection> fake_collection_;
  mswr::ComPtr<IDWriteFactory> factory_;
  mswr::ComPtr<DWriteFontCollectionProxy> collection_;
  mswr::ComPtr<IDWriteNumberSubstitution> number_substitution_;
};

TEST_F(FontFallbackUnitTest, MapCharacters) {
  mswr::ComPtr<FontFallback> fallback;
  mswr::MakeAndInitialize<FontFallback>(&fallback, collection_.Get(),
                                        fake_collection_->GetSender());

  mswr::ComPtr<IDWriteFont> font;
  UINT32 mapped_length = 0;
  float scale = 0.0;

  mswr::ComPtr<gfx::win::TextAnalysisSource> text;
  mswr::MakeAndInitialize<gfx::win::TextAnalysisSource>(
      &text, L"hello", L"en-us", number_substitution_.Get(),
      DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
  fallback->MapCharacters(text.Get(), 0, 5, nullptr, nullptr,
                          DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                          DWRITE_FONT_STRETCH_NORMAL, &mapped_length, &font,
                          &scale);

  EXPECT_EQ(1u, mapped_length);  // The fake sender only maps one character
  EXPECT_NE(nullptr, font.Get());
}

TEST_F(FontFallbackUnitTest, DuplicateCallsShouldNotRepeatIPC) {
  mswr::ComPtr<FontFallback> fallback;
  mswr::MakeAndInitialize<FontFallback>(&fallback, collection_.Get(),
                                        fake_collection_->GetTrackingSender());

  mswr::ComPtr<IDWriteFont> font;
  UINT32 mapped_length = 0;
  float scale = 0.0;

  mswr::ComPtr<gfx::win::TextAnalysisSource> text;
  mswr::MakeAndInitialize<gfx::win::TextAnalysisSource>(
      &text, L"hello", L"en-us", number_substitution_.Get(),
      DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
  fallback->MapCharacters(text.Get(), 0, 5, nullptr, nullptr,
                          DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                          DWRITE_FONT_STRETCH_NORMAL, &mapped_length, &font,
                          &scale);
  mapped_length = 0;
  fallback->MapCharacters(text.Get(), 0, 5, nullptr, nullptr,
                          DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                          DWRITE_FONT_STRETCH_NORMAL, &mapped_length, &font,
                          &scale);

  EXPECT_EQ(1u, fake_collection_->MessageCount());
  EXPECT_EQ(5u, mapped_length);
}

TEST_F(FontFallbackUnitTest, DifferentFamilyShouldNotReuseCache) {
  mswr::ComPtr<FontFallback> fallback;
  mswr::MakeAndInitialize<FontFallback>(&fallback, collection_.Get(),
                                        fake_collection_->GetTrackingSender());

  mswr::ComPtr<IDWriteFont> font;
  UINT32 mapped_length = 0;
  float scale = 0.0;

  mswr::ComPtr<gfx::win::TextAnalysisSource> text;
  mswr::MakeAndInitialize<gfx::win::TextAnalysisSource>(
      &text, L"hello", L"en-us", number_substitution_.Get(),
      DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
  fallback->MapCharacters(text.Get(), 0, 5, nullptr, L"font1",
                          DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                          DWRITE_FONT_STRETCH_NORMAL, &mapped_length, &font,
                          &scale);
  fallback->MapCharacters(text.Get(), 0, 5, nullptr, L"font2",
                          DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                          DWRITE_FONT_STRETCH_NORMAL, &mapped_length, &font,
                          &scale);

  EXPECT_EQ(2u, fake_collection_->MessageCount());
}

TEST_F(FontFallbackUnitTest, CacheMissShouldRepeatIPC) {
  mswr::ComPtr<FontFallback> fallback;
  mswr::MakeAndInitialize<FontFallback>(&fallback, collection_.Get(),
                                        fake_collection_->GetTrackingSender());

  mswr::ComPtr<IDWriteFont> font;
  UINT32 mapped_length = 0;
  float scale = 0.0;

  mswr::ComPtr<gfx::win::TextAnalysisSource> text;
  mswr::MakeAndInitialize<gfx::win::TextAnalysisSource>(
      &text, L"hello", L"en-us", number_substitution_.Get(),
      DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
  mswr::ComPtr<gfx::win::TextAnalysisSource> unmappable_text;
  mswr::MakeAndInitialize<gfx::win::TextAnalysisSource>(
      &unmappable_text, L"\uffff", L"en-us", number_substitution_.Get(),
      DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
  fallback->MapCharacters(text.Get(), 0, 5, nullptr, nullptr,
                          DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                          DWRITE_FONT_STRETCH_NORMAL, &mapped_length, &font,
                          &scale);
  fallback->MapCharacters(unmappable_text.Get(), 0, 1, nullptr, nullptr,
                          DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                          DWRITE_FONT_STRETCH_NORMAL, &mapped_length, &font,
                          &scale);

  EXPECT_EQ(2u, fake_collection_->MessageCount());
}

TEST_F(FontFallbackUnitTest, SurrogatePairCacheHit) {
  mswr::ComPtr<FontFallback> fallback;
  mswr::MakeAndInitialize<FontFallback>(&fallback, collection_.Get(),
                                        fake_collection_->GetTrackingSender());

  mswr::ComPtr<IDWriteFont> font;
  UINT32 mapped_length = 0;
  float scale = 0.0;

  mswr::ComPtr<gfx::win::TextAnalysisSource> text;
  mswr::MakeAndInitialize<gfx::win::TextAnalysisSource>(
      &text, L"hello", L"en-us", number_substitution_.Get(),
      DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
  mswr::ComPtr<gfx::win::TextAnalysisSource> surrogate_pair_text;
  mswr::MakeAndInitialize<gfx::win::TextAnalysisSource>(
      &surrogate_pair_text, L"\U0001d300", L"en-us", number_substitution_.Get(),
      DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
  fallback->MapCharacters(text.Get(), 0, 5, nullptr, nullptr,
                          DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                          DWRITE_FONT_STRETCH_NORMAL, &mapped_length, &font,
                          &scale);
  fallback->MapCharacters(surrogate_pair_text.Get(), 0, 2, nullptr, nullptr,
                          DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                          DWRITE_FONT_STRETCH_NORMAL, &mapped_length, &font,
                          &scale);

  EXPECT_EQ(1u, fake_collection_->MessageCount());
  EXPECT_EQ(2u, mapped_length);
}

}  // namespace
}  // namespace content
