// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/imagebitmap/ImageBitmapRenderingContext.h"

#include "bindings/modules/v8/RenderingContext.h"
#include "core/frame/ImageBitmap.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

ImageBitmapRenderingContext::ImageBitmapRenderingContext(HTMLCanvasElement* canvas, CanvasContextCreationAttributes attrs, Document& document)
    : CanvasRenderingContext(canvas)
    , m_hasAlpha(attrs.alpha())
{ }

ImageBitmapRenderingContext::~ImageBitmapRenderingContext() { }

void ImageBitmapRenderingContext::setCanvasGetContextResult(RenderingContext& result)
{
    result.setImageBitmapRenderingContext(this);
}

void ImageBitmapRenderingContext::transferFromImageBitmap(ImageBitmap* imageBitmap)
{
    m_image = imageBitmap->bitmapImage();
    if (!m_image)
        return;

    RefPtr<SkImage> skImage = m_image->imageForCurrentFrame();
    if (skImage->isTextureBacked()) {
        // TODO(junov): crbug.com/585607 Eliminate this readback and use an ExternalTextureLayer
        sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(skImage->width(), skImage->height());
        if (!surface) {
            // silent failure
            m_image.clear();
            return;
        }
        surface->getCanvas()->drawImage(skImage.get(), 0, 0);
        m_image = StaticBitmapImage::create(fromSkSp(surface->makeImageSnapshot()));
    }
    canvas()->didDraw(FloatRect(FloatPoint(), FloatSize(m_image->width(), m_image->height())));
}

bool ImageBitmapRenderingContext::paint(GraphicsContext& gc, const IntRect& r)
{
    if (!m_image)
        return true;

    // With impl-side painting, it is unsafe to use a gpu-backed SkImage
    ASSERT(!m_image->imageForCurrentFrame()->isTextureBacked());
    gc.drawImage(m_image.get(), r, nullptr, m_hasAlpha ? SkXfermode::kSrcOver_Mode : SkXfermode::kSrc_Mode);

    return true;
}

CanvasRenderingContext* ImageBitmapRenderingContext::Factory::create(HTMLCanvasElement* canvas, const CanvasContextCreationAttributes& attrs, Document& document)
{
    if (!RuntimeEnabledFeatures::experimentalCanvasFeaturesEnabled())
        return nullptr;
    return new ImageBitmapRenderingContext(canvas, attrs, document);
}

void ImageBitmapRenderingContext::stop()
{
    m_image.clear();
}

} // blink
