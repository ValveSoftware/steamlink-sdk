// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/skia/ImagePixelLocker.h"

#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace blink {

namespace {

bool infoIsCompatible(const SkImageInfo& info,
                      SkAlphaType alphaType,
                      SkColorType colorType) {
  ASSERT(alphaType != kUnknown_SkAlphaType);

  if (info.colorType() != colorType)
    return false;

  // kOpaque_SkAlphaType works regardless of the requested alphaType.
  return info.alphaType() == alphaType ||
         info.alphaType() == kOpaque_SkAlphaType;
}

}  // anonymous namespace

ImagePixelLocker::ImagePixelLocker(sk_sp<const SkImage> image,
                                   SkAlphaType alphaType,
                                   SkColorType colorType)
    : m_image(std::move(image)) {
  // If the image has in-RAM pixels and their format matches, use them directly.
  // TODO(fmalita): All current clients expect packed pixel rows.  Maybe we
  // could update them to support arbitrary rowBytes, and relax the check below.
  SkPixmap pixmap;
  m_image->peekPixels(&pixmap);
  m_pixels = pixmap.addr();
  if (m_pixels && infoIsCompatible(pixmap.info(), alphaType, colorType) &&
      pixmap.rowBytes() == pixmap.info().minRowBytes()) {
    return;
  }

  m_pixels = nullptr;

  // No luck, we need to read the pixels into our local buffer.
  SkImageInfo info = SkImageInfo::Make(m_image->width(), m_image->height(),
                                       colorType, alphaType);
  size_t rowBytes = info.minRowBytes();
  size_t size = info.getSafeSize(rowBytes);
  if (0 == size)
    return;

  m_pixelStorage.resize(size);  // this will throw on failure
  pixmap.reset(info, m_pixelStorage.data(), rowBytes);

  if (!m_image->readPixels(pixmap, 0, 0))
    return;

  m_pixels = m_pixelStorage.data();
}

}  // namespace blink
