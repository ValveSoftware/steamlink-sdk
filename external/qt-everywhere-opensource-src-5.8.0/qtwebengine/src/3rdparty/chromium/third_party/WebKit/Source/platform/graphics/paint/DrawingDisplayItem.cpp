// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/DrawingDisplayItem.h"

#include "platform/graphics/GraphicsContext.h"
#include "public/platform/WebDisplayItemList.h"
#include "third_party/skia/include/core/SkPictureAnalyzer.h"

#if ENABLE(ASSERT)
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkStream.h"
#endif

namespace blink {

void DrawingDisplayItem::replay(GraphicsContext& context) const
{
    if (m_picture)
        context.drawPicture(m_picture.get());
}

void DrawingDisplayItem::appendToWebDisplayItemList(const IntRect& visualRect, WebDisplayItemList* list) const
{
    if (m_picture)
        list->appendDrawingItem(visualRect, toSkSp(m_picture));
}

bool DrawingDisplayItem::drawsContent() const
{
    return m_picture.get();
}

void DrawingDisplayItem::analyzeForGpuRasterization(SkPictureGpuAnalyzer& analyzer) const
{
    analyzer.analyzePicture(m_picture.get());
}

#ifndef NDEBUG
void DrawingDisplayItem::dumpPropertiesAsDebugString(WTF::StringBuilder& stringBuilder) const
{
    DisplayItem::dumpPropertiesAsDebugString(stringBuilder);
    if (m_picture) {
        stringBuilder.append(WTF::String::format(", rect: [%f,%f %fx%f]",
            m_picture->cullRect().x(), m_picture->cullRect().y(),
            m_picture->cullRect().width(), m_picture->cullRect().height()));
    }
}
#endif

#if ENABLE(ASSERT)
static bool bitmapIsAllZero(const SkBitmap& bitmap)
{
    bitmap.lockPixels();
    bool result = true;
    for (int x = 0; result && x < bitmap.width(); ++x) {
        for (int y = 0; result && y < bitmap.height(); ++y) {
            if (SkColorSetA(bitmap.getColor(x, y), 0) != SK_ColorTRANSPARENT) {
                result = false;
                break;
            }
        }
    }
    bitmap.unlockPixels();
    return result;
}

bool DrawingDisplayItem::equals(const DisplayItem& other) const
{
    if (!DisplayItem::equals(other))
        return false;

    RefPtr<const SkPicture> picture = this->picture();
    RefPtr<const SkPicture> otherPicture = static_cast<const DrawingDisplayItem&>(other).picture();

    if (!picture && !otherPicture)
        return true;
    if (!picture || !otherPicture)
        return false;

    switch (getUnderInvalidationCheckingMode()) {
    case DrawingDisplayItem::CheckPicture: {
        if (picture->approximateOpCount() != otherPicture->approximateOpCount())
            return false;

        SkDynamicMemoryWStream pictureSerialized;
        picture->serialize(&pictureSerialized);
        SkDynamicMemoryWStream otherPictureSerialized;
        otherPicture->serialize(&otherPictureSerialized);
        if (pictureSerialized.bytesWritten() != otherPictureSerialized.bytesWritten())
            return false;

        RefPtr<SkData> oldData = adoptRef(otherPictureSerialized.copyToData());
        RefPtr<SkData> newData = adoptRef(pictureSerialized.copyToData());
        return oldData->equals(newData.get());
    }
    case DrawingDisplayItem::CheckBitmap: {
        SkRect rect = picture->cullRect();
        if (rect != otherPicture->cullRect())
            return false;

        SkBitmap bitmap;
        bitmap.allocPixels(SkImageInfo::MakeN32Premul(rect.width(), rect.height()));
        SkCanvas canvas(bitmap);
        canvas.translate(-rect.x(), -rect.y());
        canvas.drawPicture(otherPicture.get());
        SkPaint diffPaint;
        diffPaint.setXfermodeMode(SkXfermode::kDifference_Mode);
        canvas.drawPicture(picture.get(), nullptr, &diffPaint);
        return bitmapIsAllZero(bitmap);
    }
    default:
        ASSERT_NOT_REACHED();
    }
    return false;
}
#endif // ENABLE(ASSERT)

} // namespace blink
