// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/csspaint/PaintRenderingContext2D.h"

#include "platform/graphics/ImageBuffer.h"
#include <memory>

namespace blink {

PaintRenderingContext2D::PaintRenderingContext2D(
    std::unique_ptr<ImageBuffer> imageBuffer,
    bool hasAlpha,
    float zoom)
    : m_imageBuffer(std::move(imageBuffer)), m_hasAlpha(hasAlpha) {
  m_clipAntialiasing = AntiAliased;
  modifiableState().setShouldAntialias(true);

  // RecordingImageBufferSurface doesn't call ImageBufferSurface::clear
  // explicitly.
  DCHECK(m_imageBuffer);
  m_imageBuffer->canvas()->clear(hasAlpha ? SK_ColorTRANSPARENT
                                          : SK_ColorBLACK);
  m_imageBuffer->didDraw(FloatRect(0, 0, width(), height()));

  m_imageBuffer->canvas()->scale(zoom, zoom);
}

int PaintRenderingContext2D::width() const {
  ASSERT(m_imageBuffer);
  return m_imageBuffer->size().width();
}

int PaintRenderingContext2D::height() const {
  ASSERT(m_imageBuffer);
  return m_imageBuffer->size().height();
}

bool PaintRenderingContext2D::parseColorOrCurrentColor(
    Color& color,
    const String& colorString) const {
  // We ignore "currentColor" for PaintRenderingContext2D and just make it
  // "black". "currentColor" can be emulated by having "color" as an input
  // property for the css-paint-api.
  // https://github.com/w3c/css-houdini-drafts/issues/133
  return ::blink::parseColorOrCurrentColor(color, colorString, nullptr);
}

SkCanvas* PaintRenderingContext2D::drawingCanvas() const {
  return m_imageBuffer->canvas();
}

SkCanvas* PaintRenderingContext2D::existingDrawingCanvas() const {
  ASSERT(m_imageBuffer);
  return m_imageBuffer->canvas();
}

AffineTransform PaintRenderingContext2D::baseTransform() const {
  ASSERT(m_imageBuffer);
  return m_imageBuffer->baseTransform();
}

void PaintRenderingContext2D::didDraw(const SkIRect& dirtyRect) {
  ASSERT(m_imageBuffer);
  return m_imageBuffer->didDraw(SkRect::Make(dirtyRect));
}

void PaintRenderingContext2D::validateStateStack() const {
#if DCHECK_IS_ON()
  if (SkCanvas* skCanvas = existingDrawingCanvas()) {
    DCHECK_EQ(static_cast<size_t>(skCanvas->getSaveCount()),
              m_stateStack.size() + 1);
  }
#endif
}

}  // namespace blink
