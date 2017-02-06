/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/graphics/GeneratedImage.h"

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/SkPictureBuilder.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace blink {

void GeneratedImage::drawPattern(GraphicsContext& destContext, const FloatRect& srcRect, const FloatSize& scale,
    const FloatPoint& phase, SkXfermode::Mode compositeOp, const FloatRect& destRect,
    const FloatSize& repeatSpacing)
{
    FloatRect tileRect = srcRect;
    tileRect.expand(FloatSize(repeatSpacing));

    SkPictureBuilder builder(tileRect, nullptr, &destContext);
    builder.context().beginRecording(tileRect);
    drawTile(builder.context(), srcRect);
    RefPtr<SkPicture> tilePicture = builder.endRecording();

    SkMatrix patternMatrix = SkMatrix::MakeTrans(phase.x(), phase.y());
    patternMatrix.preScale(scale.width(), scale.height());
    patternMatrix.preTranslate(tileRect.x(), tileRect.y());

    RefPtr<Pattern> picturePattern = Pattern::createPicturePattern(tilePicture.release());

    SkPaint fillPaint = destContext.fillPaint();
    picturePattern->applyToPaint(fillPaint, patternMatrix);
    fillPaint.setColor(SK_ColorBLACK);
    fillPaint.setXfermodeMode(compositeOp);

    destContext.drawRect(destRect, fillPaint);
}

PassRefPtr<SkImage> GeneratedImage::imageForCurrentFrame()
{
    return nullptr;
}

} // namespace blink
