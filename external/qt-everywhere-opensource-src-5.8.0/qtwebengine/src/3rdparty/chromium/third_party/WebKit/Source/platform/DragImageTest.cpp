/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "platform/DragImage.h"

#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontTraits.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/BitmapImage.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/weborigin/KURL.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

class TestImage : public Image {
public:
    static PassRefPtr<TestImage> create(PassRefPtr<SkImage> image)
    {
        return adoptRef(new TestImage(image));
    }

    static PassRefPtr<TestImage> create(const IntSize& size)
    {
        return adoptRef(new TestImage(size));
    }

    IntSize size() const override
    {
        ASSERT(m_image);

        return IntSize(m_image->width(), m_image->height());
    }

    PassRefPtr<SkImage> imageForCurrentFrame() override
    {
        return m_image;
    }

    bool currentFrameKnownToBeOpaque(MetadataMode = UseCurrentMetadata) override
    {
        return false;
    }

    void destroyDecodedData() override
    {
        // Image pure virtual stub.
    }

    void draw(SkCanvas*, const SkPaint&, const FloatRect&, const FloatRect&, RespectImageOrientationEnum, ImageClampingMode) override
    {
        // Image pure virtual stub.
    }

private:
    explicit TestImage(PassRefPtr<SkImage> image)
        : m_image(image)
    {
    }

    explicit TestImage(IntSize size)
        : m_image(nullptr)
    {
        sk_sp<SkSurface> surface = createSkSurface(size);
        if (!surface)
            return;

        surface->getCanvas()->clear(SK_ColorTRANSPARENT);
        m_image = fromSkSp(surface->makeImageSnapshot());
    }

    static sk_sp<SkSurface> createSkSurface(IntSize size)
    {
        return SkSurface::MakeRaster(SkImageInfo::MakeN32(size.width(), size.height(), kPremul_SkAlphaType));
    }

    RefPtr<SkImage> m_image;
};

TEST(DragImageTest, NullHandling)
{
    EXPECT_FALSE(DragImage::create(0));

    RefPtr<TestImage> nullTestImage(TestImage::create(IntSize()));
    EXPECT_FALSE(DragImage::create(nullTestImage.get()));
}

TEST(DragImageTest, NonNullHandling)
{
    RefPtr<TestImage> testImage(TestImage::create(IntSize(2, 2)));
    std::unique_ptr<DragImage> dragImage = DragImage::create(testImage.get());
    ASSERT_TRUE(dragImage);

    dragImage->scale(0.5, 0.5);
    IntSize size = dragImage->size();
    EXPECT_EQ(1, size.width());
    EXPECT_EQ(1, size.height());
}

TEST(DragImageTest, CreateDragImage)
{
    // Tests that the DrageImage implementation doesn't choke on null values
    // of imageForCurrentFrame().
    // FIXME: how is this test any different from test NullHandling?
    RefPtr<TestImage> testImage(TestImage::create(IntSize()));
    EXPECT_FALSE(DragImage::create(testImage.get()));
}

TEST(DragImageTest, TrimWhitespace)
{
    KURL url(ParsedURLString, "http://www.example.com/");
    String testLabel = "          Example Example Example      \n    ";
    String expectedLabel = "Example Example Example";
    float deviceScaleFactor = 1.0f;

    FontDescription fontDescription;
    fontDescription.firstFamily().setFamily("Arial");
    fontDescription.setSpecifiedSize(16);
    fontDescription.setIsAbsoluteSize(true);
    fontDescription.setGenericFamily(FontDescription::NoFamily);
    fontDescription.setWeight(FontWeightNormal);
    fontDescription.setStyle(FontStyleNormal);

    std::unique_ptr<DragImage> testImage =
        DragImage::create(url, testLabel, fontDescription, deviceScaleFactor);
    std::unique_ptr<DragImage> expectedImage =
        DragImage::create(url, expectedLabel, fontDescription, deviceScaleFactor);

    EXPECT_EQ(testImage->size().width(), expectedImage->size().width());
}

// SkPixelRef which fails to lock, as a lazy pixel ref might if its pixels
// cannot be generated.
class InvalidPixelRef : public SkPixelRef {
public:
    InvalidPixelRef(const SkImageInfo& info) : SkPixelRef(info) { }
private:
    bool onNewLockPixels(LockRec*) override { return false; }
    void onUnlockPixels() override { ASSERT_NOT_REACHED(); }
};

TEST(DragImageTest, InvalidRotatedBitmapImage)
{
    // This test is mostly useful with MSAN builds, which can actually detect
    // the use of uninitialized memory.

    // Create a BitmapImage which will fail to produce pixels, and hence not
    // draw.
    SkImageInfo info = SkImageInfo::MakeN32Premul(100, 100);
    RefPtr<SkPixelRef> pixelRef = adoptRef(new InvalidPixelRef(info));
    SkBitmap invalidBitmap;
    invalidBitmap.setInfo(info);
    invalidBitmap.setPixelRef(pixelRef.get());
    RefPtr<BitmapImage> image = BitmapImage::createWithOrientationForTesting(invalidBitmap, OriginRightTop);

    // Create a DragImage from it. In MSAN builds, this will cause a failure if
    // the pixel memory is not initialized, if we have to respect non-default
    // orientation.
    std::unique_ptr<DragImage> dragImage = DragImage::create(image.get(), RespectImageOrientation);

    // With an invalid pixel ref, BitmapImage should have no backing SkImage => we don't allocate
    // a DragImage.
    ASSERT_FALSE(dragImage);
}

TEST(DragImageTest, InterpolationNone)
{
    SkBitmap expectedBitmap;
    expectedBitmap.allocN32Pixels(4, 4);
    {
        SkAutoLockPixels lock(expectedBitmap);
        expectedBitmap.eraseArea(SkIRect::MakeXYWH(0, 0, 2, 2), 0xFFFFFFFF);
        expectedBitmap.eraseArea(SkIRect::MakeXYWH(0, 2, 2, 2), 0xFF000000);
        expectedBitmap.eraseArea(SkIRect::MakeXYWH(2, 0, 2, 2), 0xFF000000);
        expectedBitmap.eraseArea(SkIRect::MakeXYWH(2, 2, 2, 2), 0xFFFFFFFF);
    }

    SkBitmap testBitmap;
    testBitmap.allocN32Pixels(2, 2);
    {
        SkAutoLockPixels lock(testBitmap);
        testBitmap.eraseArea(SkIRect::MakeXYWH(0, 0, 1, 1), 0xFFFFFFFF);
        testBitmap.eraseArea(SkIRect::MakeXYWH(0, 1, 1, 1), 0xFF000000);
        testBitmap.eraseArea(SkIRect::MakeXYWH(1, 0, 1, 1), 0xFF000000);
        testBitmap.eraseArea(SkIRect::MakeXYWH(1, 1, 1, 1), 0xFFFFFFFF);
    }

    RefPtr<TestImage> testImage = TestImage::create(fromSkSp(SkImage::MakeFromBitmap(testBitmap)));
    std::unique_ptr<DragImage> dragImage = DragImage::create(testImage.get(), DoNotRespectImageOrientation, 1, InterpolationNone);
    ASSERT_TRUE(dragImage);
    dragImage->scale(2, 2);
    const SkBitmap& dragBitmap = dragImage->bitmap();
    {
        SkAutoLockPixels lock1(dragBitmap);
        SkAutoLockPixels lock2(expectedBitmap);
        for (int x = 0; x < dragBitmap.width(); ++x)
            for (int y = 0; y < dragBitmap.height(); ++y)
                EXPECT_EQ(expectedBitmap.getColor(x, y), dragBitmap.getColor(x, y));
    }
}

} // namespace blink
