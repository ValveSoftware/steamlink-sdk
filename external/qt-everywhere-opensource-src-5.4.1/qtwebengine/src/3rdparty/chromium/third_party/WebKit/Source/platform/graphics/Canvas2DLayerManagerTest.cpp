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

#include "config.h"

#include "platform/graphics/Canvas2DLayerManager.h"

#include "SkDevice.h"
#include "SkSurface.h"
#include "platform/graphics/test/MockWebGraphicsContext3D.h"
#include "public/platform/Platform.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "public/platform/WebThread.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace WebCore;
using testing::InSequence;
using testing::Return;
using testing::Test;

namespace {

class MockWebGraphicsContext3DProvider : public blink::WebGraphicsContext3DProvider {
public:
    MockWebGraphicsContext3DProvider(blink::WebGraphicsContext3D* context3d)
        : m_context3d(context3d) { }

    blink::WebGraphicsContext3D* context3d()
    {
        return m_context3d;
    }

    GrContext* grContext()
    {
        return 0;
    }

private:
    blink::WebGraphicsContext3D* m_context3d;
};

class FakeCanvas2DLayerBridge : public Canvas2DLayerBridge {
public:
    FakeCanvas2DLayerBridge(blink::WebGraphicsContext3D* context, PassOwnPtr<SkDeferredCanvas> canvas)
        : Canvas2DLayerBridge(adoptPtr(new MockWebGraphicsContext3DProvider(context)), canvas, 0, NonOpaque)
        , m_freeableBytes(0)
        , m_freeMemoryIfPossibleCount(0)
        , m_flushCount(0)
    {
    }

    virtual size_t storageAllocatedForRecording() OVERRIDE
    {
        // Because the fake layer has no canvas to query, just
        // return status quo. Allocation changes that would normally be
        // initiated by the canvas can be faked by invoking
        // storageAllocatedForRecordingChanged directly from the test code.
        return m_bytesAllocated;
    }

    void fakeFreeableBytes(size_t size)
    {
        m_freeableBytes = size;
    }

    virtual size_t freeMemoryIfPossible(size_t size) OVERRIDE
    {
        m_freeMemoryIfPossibleCount++;
        size_t bytesFreed = size < m_freeableBytes ? size : m_freeableBytes;
        m_freeableBytes -= bytesFreed;
        if (bytesFreed)
            storageAllocatedForRecordingChanged(m_bytesAllocated - bytesFreed);
        return bytesFreed;
    }

    virtual void flush() OVERRIDE
    {
        flushedDrawCommands();
        m_freeableBytes = bytesAllocated();
        m_flushCount++;
    }

public:
    size_t m_freeableBytes;
    int m_freeMemoryIfPossibleCount;
    int m_flushCount;
};

class FakeCanvas2DLayerBridgePtr {
public:
    FakeCanvas2DLayerBridgePtr(PassRefPtr<FakeCanvas2DLayerBridge> layerBridge)
        : m_layerBridge(layerBridge) { }

    ~FakeCanvas2DLayerBridgePtr()
    {
        m_layerBridge->beginDestruction();
    }

    FakeCanvas2DLayerBridge* operator->() { return m_layerBridge.get(); }
    FakeCanvas2DLayerBridge* get() { return m_layerBridge.get(); }

private:
    RefPtr<FakeCanvas2DLayerBridge> m_layerBridge;
};

static PassOwnPtr<SkDeferredCanvas> createCanvas()
{
    RefPtr<SkSurface> surface = adoptRef(SkSurface::NewRasterPMColor(1, 1));
    return adoptPtr(SkDeferredCanvas::Create(surface.get()));
}

} // unnamed namespace

class Canvas2DLayerManagerTest : public Test {
protected:
    void storageAllocationTrackingTest()
    {
        Canvas2DLayerManager& manager = Canvas2DLayerManager::get();
        manager.init(10, 10);
        {
            OwnPtr<blink::MockWebGraphicsContext3D> webContext = adoptPtr(new blink::MockWebGraphicsContext3D);
            OwnPtr<SkDeferredCanvas> canvas1 = createCanvas();
            FakeCanvas2DLayerBridgePtr layer1(adoptRef(new FakeCanvas2DLayerBridge(webContext.get(), canvas1.release())));
            EXPECT_EQ((size_t)0, manager.m_bytesAllocated);
            layer1->storageAllocatedForRecordingChanged(1);
            EXPECT_EQ((size_t)1, manager.m_bytesAllocated);
            // Test allocation increase
            layer1->storageAllocatedForRecordingChanged(2);
            EXPECT_EQ((size_t)2, manager.m_bytesAllocated);
            // Test allocation decrease
            layer1->storageAllocatedForRecordingChanged(1);
            EXPECT_EQ((size_t)1, manager.m_bytesAllocated);
            {
                OwnPtr<SkDeferredCanvas> canvas2 = createCanvas();
                FakeCanvas2DLayerBridgePtr layer2(adoptRef(new FakeCanvas2DLayerBridge(webContext.get(), canvas2.release())));
                EXPECT_EQ((size_t)1, manager.m_bytesAllocated);
                // verify multi-layer allocation tracking
                layer2->storageAllocatedForRecordingChanged(2);
                EXPECT_EQ((size_t)3, manager.m_bytesAllocated);
            }
            // Verify tracking after destruction
            EXPECT_EQ((size_t)1, manager.m_bytesAllocated);
        }
    }

    void evictionTest()
    {
        OwnPtr<blink::MockWebGraphicsContext3D> webContext = adoptPtr(new blink::MockWebGraphicsContext3D);
        Canvas2DLayerManager& manager = Canvas2DLayerManager::get();
        manager.init(10, 5);
        OwnPtr<SkDeferredCanvas> canvas = createCanvas();
        FakeCanvas2DLayerBridgePtr layer(adoptRef(new FakeCanvas2DLayerBridge(webContext.get(), canvas.release())));
        layer->fakeFreeableBytes(10);
        layer->storageAllocatedForRecordingChanged(8); // under the max
        EXPECT_EQ(0, layer->m_freeMemoryIfPossibleCount);
        layer->storageAllocatedForRecordingChanged(12); // over the max
        EXPECT_EQ(1, layer->m_freeMemoryIfPossibleCount);
        EXPECT_EQ((size_t)3, layer->m_freeableBytes);
        EXPECT_EQ(0, layer->m_flushCount); // eviction succeeded without triggering a flush
        EXPECT_EQ((size_t)5, layer->bytesAllocated());
    }

    void hiddenCanvasTest()
    {
        OwnPtr<blink::MockWebGraphicsContext3D> webContext = adoptPtr(new blink::MockWebGraphicsContext3D);
        Canvas2DLayerManager& manager = Canvas2DLayerManager::get();
        manager.init(20, 5);
        OwnPtr<SkDeferredCanvas> canvas = createCanvas();
        FakeCanvas2DLayerBridgePtr layer(adoptRef(new FakeCanvas2DLayerBridge(webContext.get(), canvas.release())));
        layer->fakeFreeableBytes(5);
        layer->storageAllocatedForRecordingChanged(10);
        EXPECT_EQ(0, layer->m_freeMemoryIfPossibleCount);
        EXPECT_EQ(0, layer->m_flushCount);
        EXPECT_EQ((size_t)10, layer->bytesAllocated());
        layer->setIsHidden(true);
        EXPECT_EQ(1, layer->m_freeMemoryIfPossibleCount);
        EXPECT_EQ((size_t)0, layer->m_freeableBytes);
        EXPECT_EQ((size_t)0, layer->bytesAllocated());
        EXPECT_EQ(1, layer->m_flushCount);
    }

    void addRemoveLayerTest()
    {
        OwnPtr<blink::MockWebGraphicsContext3D> webContext = adoptPtr(new blink::MockWebGraphicsContext3D);
        Canvas2DLayerManager& manager = Canvas2DLayerManager::get();
        manager.init(10, 5);
        OwnPtr<SkDeferredCanvas> canvas = createCanvas();
        FakeCanvas2DLayerBridgePtr layer(adoptRef(new FakeCanvas2DLayerBridge(webContext.get(), canvas.release())));
        EXPECT_FALSE(manager.isInList(layer.get()));
        layer->storageAllocatedForRecordingChanged(5);
        EXPECT_TRUE(manager.isInList(layer.get()));
        layer->storageAllocatedForRecordingChanged(0);
        EXPECT_FALSE(manager.isInList(layer.get()));
    }

    void flushEvictionTest()
    {
        OwnPtr<blink::MockWebGraphicsContext3D> webContext = adoptPtr(new blink::MockWebGraphicsContext3D);
        Canvas2DLayerManager& manager = Canvas2DLayerManager::get();
        manager.init(10, 5);
        OwnPtr<SkDeferredCanvas> canvas = createCanvas();
        FakeCanvas2DLayerBridgePtr layer(adoptRef(new FakeCanvas2DLayerBridge(webContext.get(), canvas.release())));
        layer->fakeFreeableBytes(1); // Not enough freeable bytes, will cause aggressive eviction by flushing
        layer->storageAllocatedForRecordingChanged(8); // under the max
        EXPECT_EQ(0, layer->m_freeMemoryIfPossibleCount);
        layer->storageAllocatedForRecordingChanged(12); // over the max
        EXPECT_EQ(2, layer->m_freeMemoryIfPossibleCount); // Two tries, one before flush, one after flush
        EXPECT_EQ((size_t)5, layer->m_freeableBytes);
        EXPECT_EQ(1, layer->m_flushCount); // flush was attempted
        EXPECT_EQ((size_t)5, layer->bytesAllocated());
        EXPECT_TRUE(manager.isInList(layer.get()));
    }

    void doDeferredFrameTestTask(FakeCanvas2DLayerBridge* layer, bool skipCommands)
    {
        EXPECT_FALSE(Canvas2DLayerManager::get().m_taskObserverActive);
        layer->willUse();
        layer->storageAllocatedForRecordingChanged(1);
        EXPECT_TRUE(Canvas2DLayerManager::get().m_taskObserverActive);
        if (skipCommands) {
            layer->willUse();
            layer->skippedPendingDrawCommands();
        }
        blink::Platform::current()->currentThread()->exitRunLoop();
    }

    class DeferredFrameTestTask : public blink::WebThread::Task {
    public:
        DeferredFrameTestTask(Canvas2DLayerManagerTest* test, FakeCanvas2DLayerBridge* layer, bool skipCommands)
        {
            m_test = test;
            m_layer = layer;
            m_skipCommands = skipCommands;
        }

        virtual void run() OVERRIDE
        {
            m_test->doDeferredFrameTestTask(m_layer, m_skipCommands);
        }
    private:
        Canvas2DLayerManagerTest* m_test;
        FakeCanvas2DLayerBridge* m_layer;
        bool m_skipCommands;
    };

    void deferredFrameTest()
    {
        OwnPtr<blink::MockWebGraphicsContext3D> webContext = adoptPtr(new blink::MockWebGraphicsContext3D);
        Canvas2DLayerManager::get().init(10, 10);
        OwnPtr<SkDeferredCanvas> canvas = createCanvas();
        FakeCanvas2DLayerBridgePtr layer(adoptRef(new FakeCanvas2DLayerBridge(webContext.get(), canvas.release())));
        blink::Platform::current()->currentThread()->postTask(new DeferredFrameTestTask(this, layer.get(), true));
        blink::Platform::current()->currentThread()->enterRunLoop();
        // Verify that didProcessTask was called upon completion
        EXPECT_FALSE(Canvas2DLayerManager::get().m_taskObserverActive);
        // Verify that no flush was performed because frame is fresh
        EXPECT_EQ(0, layer->m_flushCount);

        // Verify that no flushes are triggered as long as frame are fresh
        blink::Platform::current()->currentThread()->postTask(new DeferredFrameTestTask(this, layer.get(), true));
        blink::Platform::current()->currentThread()->enterRunLoop();
        EXPECT_FALSE(Canvas2DLayerManager::get().m_taskObserverActive);
        EXPECT_EQ(0, layer->m_flushCount);

        blink::Platform::current()->currentThread()->postTask(new DeferredFrameTestTask(this, layer.get(), true));
        blink::Platform::current()->currentThread()->enterRunLoop();
        EXPECT_FALSE(Canvas2DLayerManager::get().m_taskObserverActive);
        EXPECT_EQ(0, layer->m_flushCount);

        // Verify that a flush is triggered when queue is accumulating a multi-frame backlog.
        blink::Platform::current()->currentThread()->postTask(new DeferredFrameTestTask(this, layer.get(), false));
        blink::Platform::current()->currentThread()->enterRunLoop();
        EXPECT_FALSE(Canvas2DLayerManager::get().m_taskObserverActive);
        EXPECT_EQ(1, layer->m_flushCount);

        blink::Platform::current()->currentThread()->postTask(new DeferredFrameTestTask(this, layer.get(), false));
        blink::Platform::current()->currentThread()->enterRunLoop();
        EXPECT_FALSE(Canvas2DLayerManager::get().m_taskObserverActive);
        EXPECT_EQ(2, layer->m_flushCount);
    }
};

namespace {

TEST_F(Canvas2DLayerManagerTest, testStorageAllocationTracking)
{
    storageAllocationTrackingTest();
}

TEST_F(Canvas2DLayerManagerTest, testEviction)
{
    evictionTest();
}

TEST_F(Canvas2DLayerManagerTest, testFlushEviction)
{
    flushEvictionTest();
}

TEST_F(Canvas2DLayerManagerTest, testDeferredFrame)
{
    deferredFrameTest();
}

TEST_F(Canvas2DLayerManagerTest, testHiddenCanvas)
{
    hiddenCanvasTest();
}

TEST_F(Canvas2DLayerManagerTest, testAddRemoveLayer)
{
    addRemoveLayerTest();
}

} // unnamed namespace

