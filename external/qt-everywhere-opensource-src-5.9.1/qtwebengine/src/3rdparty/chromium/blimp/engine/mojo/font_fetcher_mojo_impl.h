// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_MOJO_FONT_FETCHER_MOJO_IMPL_H_
#define BLIMP_ENGINE_MOJO_FONT_FETCHER_MOJO_IMPL_H_

#include <string>

#include "blimp/engine/mojo/font_fetcher.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"

class SkStream;

namespace blimp {
namespace engine {

class FontDataFetcher;

// Service for processing FontFetcher requests from the renderer.
// Caller is responsible for executing all methods on the IO thread.
class FontFetcherMojoImpl : public mojom::FontFetcher {
 public:
  // |font_data_fetcher|: The FontDataFetcher object which will get the font
  // data stream, it must outlive the Mojo connection.
  explicit FontFetcherMojoImpl(FontDataFetcher* font_data_fetcher);
  ~FontFetcherMojoImpl() override;

  // Factory method called by Mojo.
  // Binds |this| to the connection specified by |request|.
  void BindRequest(mojo::InterfaceRequest<mojom::FontFetcher> request);

  // mojom::FontAccess implementation.
  void GetFontStream(const std::string& font_hash,
                     const GetFontStreamCallback& callback) override;

 private:
  mojo::BindingSet<mojom::FontFetcher> bindings_;

  // Fetcher object which fetches the font stream and passed over the Mojo
  // service.
  FontDataFetcher* font_data_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(FontFetcherMojoImpl);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_MOJO_FONT_FETCHER_MOJO_IMPL_H_
