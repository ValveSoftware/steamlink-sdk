// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/mojo/font_fetcher_mojo_impl.h"

#include <stdint.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "blimp/engine/browser/font_data_fetcher.h"
#include "mojo/public/cpp/system/buffer.h"
#include "third_party/skia/include/core/SkStream.h"

namespace blimp {
namespace engine {

namespace {

void ReturnFontRequestResponse(
    const FontFetcherMojoImpl::GetFontStreamCallback& callback,
    std::unique_ptr<SkStream> font_stream) {
  uint32_t font_stream_size = font_stream->getLength();
  if (font_stream_size == 0) {
    LOG(WARNING) << "Font data not available.";
    callback.Run(mojo::ScopedSharedBufferHandle(), 0);
    return;
  }

  mojo::ScopedSharedBufferHandle handle =
      mojo::SharedBufferHandle::Create(font_stream_size);
  if (!handle.is_valid()) {
    LOG(ERROR) << "Failed to create SharedBufferHandle of " << font_stream_size
        << " bytes.";
    callback.Run(mojo::ScopedSharedBufferHandle(), 0);
    return;
  }

  mojo::ScopedSharedBufferMapping mapping = handle->Map(font_stream_size);
  if (!mapping) {
    LOG(ERROR) << "Failed to mmap region of " << font_stream_size << " bytes.";
    callback.Run(mojo::ScopedSharedBufferHandle(), 0);
    return;
  }

  uint32_t bytes_read = font_stream->read(mapping.get(), font_stream_size);
  DCHECK_EQ(bytes_read, font_stream_size);
  if (bytes_read < font_stream_size) {
    LOG(ERROR) << "SKStream read failed. Read " << bytes_read
        << " bytes, should read " << font_stream_size << "bytes";
    callback.Run(mojo::ScopedSharedBufferHandle(), 0);
    return;
  }

  callback.Run(handle->Clone(mojo::SharedBufferHandle::AccessMode::READ_ONLY),
               font_stream_size);
}

}  // namespace

FontFetcherMojoImpl::FontFetcherMojoImpl(FontDataFetcher* font_data_fetcher)
    : font_data_fetcher_(font_data_fetcher) {
  DCHECK(font_data_fetcher_);
}

FontFetcherMojoImpl::~FontFetcherMojoImpl() {}

void FontFetcherMojoImpl::GetFontStream(const std::string& font_hash,
                                   const GetFontStreamCallback& callback) {
  VLOG(1) << "FontFetcherIPC::GetFontStream called for font_hash "
      << font_hash;

  font_data_fetcher_->FetchFontStream(
      font_hash, base::Bind(&ReturnFontRequestResponse, callback));
}

void FontFetcherMojoImpl::BindRequest(
    mojo::InterfaceRequest<mojom::FontFetcher> request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace engine
}  // namespace blimp
