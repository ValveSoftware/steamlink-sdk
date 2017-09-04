// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "blimp/engine/browser/font_data_fetcher.h"
#include "blimp/engine/mojo/font_fetcher_mojo_impl.h"
#include "mojo/public/cpp/system/buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace engine {
namespace {

const char kFakeHash[] = "fake hash";
const char kTestString[] = "Hello world";

class FakeFontDataFetcher : public FontDataFetcher {
 public:
  FakeFontDataFetcher() {}

  void FetchFontStream(const std::string& font_hash,
                       const FontResponseCallback& callback) const override {
    // Return an empty stream if font_hash is empty, otherwise return a SkStream
    // which contains kTestString.
    if (font_hash.empty()) {
      callback.Run(base::MakeUnique<SkMemoryStream>());
    } else {
      callback.Run(base::MakeUnique<SkMemoryStream>(
          kTestString, arraysize(kTestString) - 1));
    }
  }
};

void VerifyEmptyDataWrite(mojo::ScopedSharedBufferHandle handle,
                              uint32_t size) {
  ASSERT_FALSE(handle.is_valid());
  EXPECT_EQ(size, (uint32_t)0);
}

void VerifyDataWriteCorrectly(mojo::ScopedSharedBufferHandle handle,
                              uint32_t size) {
  ASSERT_TRUE(handle.is_valid());

  mojo::ScopedSharedBufferMapping mapping = handle->Map(size);
  ASSERT_TRUE(mapping);

  std::string contents(static_cast<const char*>(mapping.get()), size);
  EXPECT_EQ(kTestString, contents);
}

class FontFetcherMojoImplUnittest : public testing::Test {
 public:
  FontFetcherMojoImplUnittest()
      : font_fetcher_mojo_impl_(new FakeFontDataFetcher()) {}
  ~FontFetcherMojoImplUnittest() override {}

  FontFetcherMojoImpl font_fetcher_mojo_impl_;
};

// TODO(mlliu): add tests later to do a full IPC test, calling the FontFetcher
// API via the corresponding Mojo client interface.
TEST_F(FontFetcherMojoImplUnittest, VerifyWriteToDataPipe) {
  // Expect that dummy font data is written correctly to a Mojo DataPipe.
  font_fetcher_mojo_impl_.GetFontStream(kFakeHash,
                                        base::Bind(&VerifyDataWriteCorrectly));

  // Expect empty handle if the font stream is empty.
  font_fetcher_mojo_impl_.GetFontStream("", base::Bind(&VerifyEmptyDataWrite));
}

}  // namespace
}  // namespace engine
}  // namespace blimp
