/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/graphics/GraphicsContext.h"

#include "platform/graphics/BitmapImage.h"
#include "platform/graphics/Path.h"
#include "platform/graphics/paint/PaintController.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkShader.h"
#include <memory>

namespace blink {

#define EXPECT_EQ_RECT(a, b) \
    EXPECT_EQ(a.x(), b.x()); \
    EXPECT_EQ(a.y(), b.y()); \
    EXPECT_EQ(a.width(), b.width()); \
    EXPECT_EQ(a.height(), b.height());

#define EXPECT_OPAQUE_PIXELS_IN_RECT(bitmap, opaqueRect) \
{ \
    SkAutoLockPixels locker(bitmap); \
    for (int y = opaqueRect.y(); y < opaqueRect.maxY(); ++y) \
        for (int x = opaqueRect.x(); x < opaqueRect.maxX(); ++x) { \
            int alpha = *bitmap.getAddr32(x, y) >> 24; \
            EXPECT_EQ(255, alpha); \
        } \
}

#define EXPECT_OPAQUE_PIXELS_ONLY_IN_RECT(bitmap, opaqueRect) \
{ \
    SkAutoLockPixels locker(bitmap); \
    for (int y = 0; y < bitmap.height(); ++y) \
        for (int x = 0; x < bitmap.width(); ++x) {     \
            int alpha = *bitmap.getAddr32(x, y) >> 24; \
            bool opaque = opaqueRect.contains(x, y); \
            EXPECT_EQ(opaque, alpha == 255); \
        } \
}

TEST(GraphicsContextTest, pictureRecording)
{
    SkBitmap bitmap;
    bitmap.allocN32Pixels(100, 100);
    bitmap.eraseColor(0);
    SkCanvas canvas(bitmap);

    std::unique_ptr<PaintController> paintController = PaintController::create();
    GraphicsContext context(*paintController);

    Color opaque(1.0f, 0.0f, 0.0f, 1.0f);
    FloatRect bounds(0, 0, 100, 100);

    context.beginRecording(bounds);
    context.fillRect(FloatRect(0, 0, 50, 50), opaque, SkXfermode::kSrcOver_Mode);
    RefPtr<const SkPicture> picture = context.endRecording();
    canvas.drawPicture(picture.get());
    EXPECT_OPAQUE_PIXELS_ONLY_IN_RECT(bitmap, IntRect(0, 0, 50, 50))

    context.beginRecording(bounds);
    context.fillRect(FloatRect(0, 0, 100, 100), opaque, SkXfermode::kSrcOver_Mode);
    picture = context.endRecording();
    // Make sure the opaque region was unaffected by the rect drawn during Picture recording.
    EXPECT_OPAQUE_PIXELS_ONLY_IN_RECT(bitmap, IntRect(0, 0, 50, 50))

    canvas.drawPicture(picture.get());
    EXPECT_OPAQUE_PIXELS_ONLY_IN_RECT(bitmap, IntRect(0, 0, 100, 100))
}

TEST(GraphicsContextTest, UnboundedDrawsAreClipped)
{
    SkBitmap bitmap;
    bitmap.allocN32Pixels(400, 400);
    bitmap.eraseColor(0);
    SkCanvas canvas(bitmap);

    Color opaque(1.0f, 0.0f, 0.0f, 1.0f);
    Color alpha(0.0f, 0.0f, 0.0f, 0.0f);
    FloatRect bounds(0, 0, 100, 100);

    std::unique_ptr<PaintController> paintController = PaintController::create();
    GraphicsContext context(*paintController);
    context.beginRecording(bounds);

    context.setShouldAntialias(false);
    context.setMiterLimit(1);
    context.setStrokeThickness(5);
    context.setLineCap(SquareCap);
    context.setStrokeStyle(SolidStroke);

    // Make skia unable to compute fast bounds for our paths.
    DashArray dashArray;
    dashArray.append(1);
    dashArray.append(0);
    context.setLineDash(dashArray, 0);

    // Make the device opaque in 10,10 40x40.
    context.fillRect(FloatRect(10, 10, 40, 40), opaque, SkXfermode::kSrcOver_Mode);
    RefPtr<const SkPicture> picture = context.endRecording();
    canvas.drawPicture(picture.get());
    EXPECT_OPAQUE_PIXELS_ONLY_IN_RECT(bitmap, IntRect(10, 10, 40, 40));

    context.beginRecording(bounds);
    // Clip to the left edge of the opaque area.
    context.clip(IntRect(10, 10, 10, 40));

    // Draw a path that gets clipped. This should destroy the opaque area but only inside the clip.
    Path path;
    path.moveTo(FloatPoint(10, 10));
    path.addLineTo(FloatPoint(40, 40));
    SkPaint paint;
    paint.setColor(alpha.rgb());
    paint.setXfermodeMode(SkXfermode::kSrcOut_Mode);
    context.drawPath(path.getSkPath(), paint);

    picture = context.endRecording();
    canvas.drawPicture(picture.get());
    EXPECT_OPAQUE_PIXELS_IN_RECT(bitmap, IntRect(20, 10, 30, 40));
}

} // namespace blink
