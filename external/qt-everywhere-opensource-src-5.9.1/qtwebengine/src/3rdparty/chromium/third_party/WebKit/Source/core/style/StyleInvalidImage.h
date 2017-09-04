// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleInvalidImage_h
#define StyleInvalidImage_h

#include "core/css/CSSImageValue.h"
#include "core/style/StyleImage.h"

namespace blink {

class StyleInvalidImage final : public StyleImage {
 public:
  static StyleInvalidImage* create(const String& url) {
    return new StyleInvalidImage(url);
  }

  WrappedImagePtr data() const override { return m_url.impl(); }

  CSSValue* cssValue() const override {
    return CSSImageValue::create(AtomicString(m_url));
  }

  CSSValue* computedCSSValue() const override { return cssValue(); }

  LayoutSize imageSize(const LayoutObject&,
                       float /*multiplier*/,
                       const LayoutSize& /*defaultObjectSize*/) const override {
    return LayoutSize();
  }
  bool imageHasRelativeSize() const override { return false; }
  bool usesImageContainerSize() const override { return false; }
  void addClient(LayoutObject*) override {}
  void removeClient(LayoutObject*) override {}
  PassRefPtr<Image> image(const LayoutObject&,
                          const IntSize&,
                          float) const override {
    return nullptr;
  }
  bool knownToBeOpaque(const LayoutObject&) const override { return false; }

  DEFINE_INLINE_VIRTUAL_TRACE() { StyleImage::trace(visitor); }

 private:
  explicit StyleInvalidImage(const String& url) : m_url(url) {
    m_isInvalidImage = true;
  }

  String m_url;
};

DEFINE_STYLE_IMAGE_TYPE_CASTS(StyleInvalidImage, isInvalidImage());

}  // namespace blink
#endif
