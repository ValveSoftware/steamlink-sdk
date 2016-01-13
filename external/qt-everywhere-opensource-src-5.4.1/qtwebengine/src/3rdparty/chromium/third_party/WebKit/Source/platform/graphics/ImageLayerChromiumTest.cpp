/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "config.h"
#include "platform/graphics/Image.h"

#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/skia/NativeImageSkia.h"
#include "public/platform/WebImageLayer.h"
#include "wtf/PassOwnPtr.h"

#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class MockGraphicsLayerClient : public GraphicsLayerClient {
public:
    virtual void notifyAnimationStarted(const GraphicsLayer*, double monotonicTime) OVERRIDE { }
    virtual void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const IntRect& inClip) OVERRIDE { }
    virtual String debugName(const GraphicsLayer*) OVERRIDE { return String(); }
};

class TestImage : public Image {
public:

    static PassRefPtr<TestImage> create(const IntSize& size, bool isOpaque)
    {
        return adoptRef(new TestImage(size, isOpaque));
    }

    explicit TestImage(const IntSize& size, bool isOpaque)
        : Image(0)
        , m_size(size)
    {
        SkBitmap bitmap;
        EXPECT_TRUE(bitmap.allocN32Pixels(size.width(), size.height(), isOpaque));
        m_nativeImage = NativeImageSkia::create(bitmap);
    }

    virtual bool isBitmapImage() const OVERRIDE
    {
        return true;
    }

    virtual bool currentFrameKnownToBeOpaque() OVERRIDE
    {
        return m_nativeImage->bitmap().isOpaque();
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

    virtual void draw(GraphicsContext*, const FloatRect&, const FloatRect&, CompositeOperator, blink::WebBlendMode) OVERRIDE
    {
    }

private:

    IntSize m_size;

    RefPtr<NativeImageSkia> m_nativeImage;
};

class GraphicsLayerForTesting : public GraphicsLayer {
public:
    explicit GraphicsLayerForTesting(GraphicsLayerClient* client)
        : GraphicsLayer(client) { };

    virtual blink::WebLayer* contentsLayer() const { return GraphicsLayer::contentsLayer(); }
};

TEST(ImageLayerChromiumTest, opaqueImages)
{
    MockGraphicsLayerClient client;
    OwnPtr<GraphicsLayerForTesting> graphicsLayer = adoptPtr(new GraphicsLayerForTesting(&client));
    ASSERT_TRUE(graphicsLayer.get());

    RefPtr<Image> opaqueImage = TestImage::create(IntSize(100, 100), true /* opaque */);
    ASSERT_TRUE(opaqueImage.get());
    RefPtr<Image> nonOpaqueImage = TestImage::create(IntSize(100, 100), false /* opaque */);
    ASSERT_TRUE(nonOpaqueImage.get());

    ASSERT_FALSE(graphicsLayer->contentsLayer());

    graphicsLayer->setContentsToImage(opaqueImage.get());
    ASSERT_TRUE(graphicsLayer->contentsLayer()->opaque());

    graphicsLayer->setContentsToImage(nonOpaqueImage.get());
    ASSERT_FALSE(graphicsLayer->contentsLayer()->opaque());
}

} // namespace
