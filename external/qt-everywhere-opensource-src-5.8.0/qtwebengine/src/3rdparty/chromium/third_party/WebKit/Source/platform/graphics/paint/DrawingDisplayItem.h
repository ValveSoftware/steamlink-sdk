// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DrawingDisplayItem_h
#define DrawingDisplayItem_h

#include "platform/PlatformExport.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace blink {

class PLATFORM_EXPORT DrawingDisplayItem final : public DisplayItem {
public:
#if ENABLE(ASSERT)
    enum UnderInvalidationCheckingMode {
        CheckPicture, // Check if the new picture and the old picture are the same
        CheckBitmap, // Check if the new picture and the old picture produce the same bitmap
    };
#endif

    DrawingDisplayItem(const DisplayItemClient& client
        , Type type
        , PassRefPtr<const SkPicture> picture
        , bool knownToBeOpaque = false
#if ENABLE(ASSERT)
        , UnderInvalidationCheckingMode underInvalidationCheckingMode = CheckPicture
#endif
        )
        : DisplayItem(client, type, sizeof(*this))
        , m_picture(picture && picture->approximateOpCount() ? picture : nullptr)
        , m_knownToBeOpaque(knownToBeOpaque)
#if ENABLE(ASSERT)
        , m_underInvalidationCheckingMode(underInvalidationCheckingMode)
#endif
    {
        ASSERT(isDrawingType(type));
    }

    void replay(GraphicsContext&) const override;
    void appendToWebDisplayItemList(const IntRect&, WebDisplayItemList*) const override;
    bool drawsContent() const override;

    const SkPicture* picture() const { return m_picture.get(); }

    bool knownToBeOpaque() const { ASSERT(RuntimeEnabledFeatures::slimmingPaintV2Enabled()); return m_knownToBeOpaque; }

    void analyzeForGpuRasterization(SkPictureGpuAnalyzer&) const override;

#if ENABLE(ASSERT)
    UnderInvalidationCheckingMode getUnderInvalidationCheckingMode() const { return m_underInvalidationCheckingMode; }
    bool equals(const DisplayItem& other) const final;
#endif

private:
#ifndef NDEBUG
    void dumpPropertiesAsDebugString(WTF::StringBuilder&) const override;
#endif

    RefPtr<const SkPicture> m_picture;

    // True if there are no transparent areas. Only used for SlimmingPaintV2.
    const bool m_knownToBeOpaque;

#if ENABLE(ASSERT)
    UnderInvalidationCheckingMode m_underInvalidationCheckingMode;
#endif
};

} // namespace blink

#endif // DrawingDisplayItem_h
