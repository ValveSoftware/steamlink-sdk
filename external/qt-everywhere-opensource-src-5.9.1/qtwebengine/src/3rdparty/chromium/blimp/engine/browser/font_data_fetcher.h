// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_BROWSER_FONT_DATA_FETCHER_H_
#define BLIMP_ENGINE_BROWSER_FONT_DATA_FETCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "third_party/skia/include/core/SkStream.h"

namespace blimp {
namespace engine {

// FontDataFetcher does the real work of fetching font data.
// It should be created, called, and destroyed from the same thread.
class FontDataFetcher {
 public:
  using FontResponseCallback =
      base::Callback<void(std::unique_ptr<SkStream>)>;

  virtual ~FontDataFetcher() {}

  // Asynchronously loads the font identified by |font_hash| and invokes
  // |callback| with an SkStream containing the font data.
  // If there is no font for font_hash, the SkStream will be empty.
  virtual void FetchFontStream(const std::string& font_hash,
                               const FontResponseCallback& callback) const = 0;
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_BROWSER_FONT_DATA_FETCHER_H_
