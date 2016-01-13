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

#include "config.h"

#include "platform/DragImage.h"

#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontTraits.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/skia/NativeImageSkia.h"
#include "platform/weborigin/KURL.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class TestImage : public Image {
public:

    static PassRefPtr<TestImage> create(const IntSize& size)
    {
        return adoptRef(new TestImage(size));
    }

    explicit TestImage(const IntSize& size)
        : Image(0)
        , m_size(size)
    {
        SkBitmap bitmap;
        EXPECT_TRUE(bitmap.allocN32Pixels(size.width(), size.height()));
        m_nativeImage = NativeImageSkia::create(bitmap);
    }

    virtual IntSize size() const OVERRIDE
    {
        return m_size;
    }

    virtual PassRefPtr<NativeImageSkia> nativeImageForCurrentFrame() OVERRIDE
    {
        if (m_size.isZero())
            return nullptr;

        return m_nativeImage;
    }

    // Stub implementations of pure virtual Image functions.
    virtual void destroyDecodedData(bool) OVERRIDE
    {
    }

    virtual bool currentFrameKnownToBeOpaque() OVERRIDE
    {
        return false;
    }

    virtual void draw(GraphicsContext*, const FloatRect&, const FloatRect&, CompositeOperator, blink::WebBlendMode) OVERRIDE
    {
    }

private:

    IntSize m_size;

    RefPtr<NativeImageSkia> m_nativeImage;
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
    OwnPtr<DragImage> dragImage = DragImage::create(testImage.get());
    ASSERT_TRUE(dragImage);

    dragImage->scale(0.5, 0.5);
    IntSize size = dragImage->size();
    EXPECT_EQ(1, size.width());
    EXPECT_EQ(1, size.height());

    dragImage->dissolveToFraction(0.5);
}

TEST(DragImageTest, CreateDragImage)
{
    {
        // Tests that the DrageImage implementation doesn't choke on null values
        // of nativeImageForCurrentFrame().
        RefPtr<TestImage> testImage(TestImage::create(IntSize()));
        EXPECT_FALSE(DragImage::create(testImage.get()));
    }

    {
        // Tests that the drag image is a deep copy.
        RefPtr<TestImage> testImage(TestImage::create(IntSize(1, 1)));
        OwnPtr<DragImage> dragImage = DragImage::create(testImage.get());
        ASSERT_TRUE(dragImage);
        SkAutoLockPixels lock1(dragImage->bitmap()), lock2(testImage->nativeImageForCurrentFrame()->bitmap());
        EXPECT_NE(dragImage->bitmap().getPixels(), testImage->nativeImageForCurrentFrame()->bitmap().getPixels());
    }
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

    OwnPtr<DragImage> testImage =
        DragImage::create(url, testLabel, fontDescription, deviceScaleFactor);
    OwnPtr<DragImage> expectedImage =
        DragImage::create(url, expectedLabel, fontDescription, deviceScaleFactor);

    EXPECT_EQ(testImage->size().width(), expectedImage->size().width());
}

} // anonymous namespace
