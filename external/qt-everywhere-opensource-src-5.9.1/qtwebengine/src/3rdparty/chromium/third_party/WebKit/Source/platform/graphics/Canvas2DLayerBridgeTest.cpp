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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "platform/graphics/Canvas2DLayerBridge.h"

#include "cc/resources/single_release_callback.h"
#include "cc/resources/texture_mailbox.h"
#include "cc/test/test_gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/WebTaskRunner.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "platform/graphics/test/FakeGLES2Interface.h"
#include "platform/graphics/test/FakeWebGraphicsContext3DProvider.h"
#include "public/platform/Platform.h"
#include "public/platform/WebExternalBitmap.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"
#include "skia/ext/texture_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/platform/testing/TestingPlatformSupport.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"

#include <memory>

using testing::AnyNumber;
using testing::AtLeast;
using testing::InSequence;
using testing::Return;
using testing::Test;
using testing::_;

namespace blink {

namespace {

class Canvas2DLayerBridgePtr {
 public:
  Canvas2DLayerBridgePtr() {}
  Canvas2DLayerBridgePtr(PassRefPtr<Canvas2DLayerBridge> layerBridge)
      : m_layerBridge(layerBridge) {}

  ~Canvas2DLayerBridgePtr() { clear(); }

  void clear() {
    if (m_layerBridge) {
      m_layerBridge->beginDestruction();
      m_layerBridge.clear();
    }
  }

  void operator=(PassRefPtr<Canvas2DLayerBridge> layerBridge) {
    ASSERT(!m_layerBridge);
    m_layerBridge = layerBridge;
  }

  Canvas2DLayerBridge* operator->() { return m_layerBridge.get(); }
  Canvas2DLayerBridge* get() { return m_layerBridge.get(); }

 private:
  RefPtr<Canvas2DLayerBridge> m_layerBridge;
};

class FakeGLES2InterfaceWithImageSupport : public FakeGLES2Interface {
 public:
  GLuint CreateImageCHROMIUM(ClientBuffer, GLsizei, GLsizei, GLenum) override {
    return ++m_createImageCount;
  }
  void DestroyImageCHROMIUM(GLuint) override { ++m_destroyImageCount; }

  GLuint createImageCount() { return m_createImageCount; }
  GLuint destroyImageCount() { return m_destroyImageCount; }

 private:
  GLuint m_createImageCount = 0;
  GLuint m_destroyImageCount = 0;
};

class FakePlatformSupport : public TestingPlatformSupport {
  gpu::GpuMemoryBufferManager* getGpuMemoryBufferManager() override {
    return &m_testGpuMemoryBufferManager;
  }

 private:
  cc::TestGpuMemoryBufferManager m_testGpuMemoryBufferManager;
};

}  // anonymous namespace

class Canvas2DLayerBridgeTest : public Test {
 public:
  PassRefPtr<Canvas2DLayerBridge> makeBridge(
      std::unique_ptr<FakeWebGraphicsContext3DProvider> provider,
      const IntSize& size,
      Canvas2DLayerBridge::AccelerationMode accelerationMode) {
    RefPtr<Canvas2DLayerBridge> bridge = adoptRef(
        new Canvas2DLayerBridge(std::move(provider), size, 0, NonOpaque,
                                accelerationMode, nullptr, kN32_SkColorType));
    bridge->dontUseIdleSchedulingForTesting();
    return bridge.release();
  }

 protected:
  void fullLifecycleTest() {
    FakeGLES2Interface gl;
    std::unique_ptr<FakeWebGraphicsContext3DProvider> contextProvider =
        wrapUnique(new FakeWebGraphicsContext3DProvider(&gl));

    Canvas2DLayerBridgePtr bridge(adoptRef(new Canvas2DLayerBridge(
        std::move(contextProvider), IntSize(300, 150), 0, NonOpaque,
        Canvas2DLayerBridge::DisableAcceleration, nullptr, kN32_SkColorType)));

    const GrGLTextureInfo* textureInfo = skia::GrBackendObjectToGrGLTextureInfo(
        bridge->newImageSnapshot(PreferAcceleration, SnapshotReasonUnitTests)
            ->getTextureHandle(true));
    EXPECT_EQ(textureInfo, nullptr);
    bridge.clear();
  }

  void fallbackToSoftwareIfContextLost() {
    FakeGLES2Interface gl;
    std::unique_ptr<FakeWebGraphicsContext3DProvider> contextProvider =
        wrapUnique(new FakeWebGraphicsContext3DProvider(&gl));

    gl.setIsContextLost(true);
    Canvas2DLayerBridgePtr bridge(adoptRef(new Canvas2DLayerBridge(
        std::move(contextProvider), IntSize(300, 150), 0, NonOpaque,
        Canvas2DLayerBridge::EnableAcceleration, nullptr, kN32_SkColorType)));
    EXPECT_TRUE(bridge->checkSurfaceValid());
    EXPECT_FALSE(bridge->isAccelerated());
  }

  void fallbackToSoftwareOnFailedTextureAlloc() {
    {
      // No fallback case.
      FakeGLES2Interface gl;
      std::unique_ptr<FakeWebGraphicsContext3DProvider> contextProvider =
          wrapUnique(new FakeWebGraphicsContext3DProvider(&gl));
      Canvas2DLayerBridgePtr bridge(adoptRef(new Canvas2DLayerBridge(
          std::move(contextProvider), IntSize(300, 150), 0, NonOpaque,
          Canvas2DLayerBridge::EnableAcceleration, nullptr, kN32_SkColorType)));
      EXPECT_TRUE(bridge->checkSurfaceValid());
      EXPECT_TRUE(bridge->isAccelerated());
      sk_sp<SkImage> snapshot =
          bridge->newImageSnapshot(PreferAcceleration, SnapshotReasonUnitTests);
      EXPECT_TRUE(bridge->isAccelerated());
      EXPECT_TRUE(snapshot->isTextureBacked());
    }

    {
      // Fallback case.
      FakeGLES2Interface gl;
      std::unique_ptr<FakeWebGraphicsContext3DProvider> contextProvider =
          wrapUnique(new FakeWebGraphicsContext3DProvider(&gl));
      GrContext* gr = contextProvider->grContext();
      Canvas2DLayerBridgePtr bridge(adoptRef(new Canvas2DLayerBridge(
          std::move(contextProvider), IntSize(300, 150), 0, NonOpaque,
          Canvas2DLayerBridge::EnableAcceleration, nullptr, kN32_SkColorType)));
      EXPECT_TRUE(bridge->checkSurfaceValid());
      EXPECT_TRUE(bridge->isAccelerated());  // We don't yet know that
                                             // allocation will fail.
      // This will cause SkSurface_Gpu creation to fail without
      // Canvas2DLayerBridge otherwise detecting that anything was disabled.
      gr->abandonContext();
      sk_sp<SkImage> snapshot =
          bridge->newImageSnapshot(PreferAcceleration, SnapshotReasonUnitTests);
      EXPECT_FALSE(bridge->isAccelerated());
      EXPECT_FALSE(snapshot->isTextureBacked());
    }
  }

  void noDrawOnContextLostTest() {
    FakeGLES2Interface gl;
    std::unique_ptr<FakeWebGraphicsContext3DProvider> contextProvider =
        wrapUnique(new FakeWebGraphicsContext3DProvider(&gl));

    Canvas2DLayerBridgePtr bridge(adoptRef(new Canvas2DLayerBridge(
        std::move(contextProvider), IntSize(300, 150), 0, NonOpaque,
        Canvas2DLayerBridge::ForceAccelerationForTesting, nullptr,
        kN32_SkColorType)));
    EXPECT_TRUE(bridge->checkSurfaceValid());
    SkPaint paint;
    uint32_t genID = bridge->getOrCreateSurface()->generationID();
    bridge->canvas()->drawRect(SkRect::MakeXYWH(0, 0, 1, 1), paint);
    EXPECT_EQ(genID, bridge->getOrCreateSurface()->generationID());
    gl.setIsContextLost(true);
    EXPECT_EQ(genID, bridge->getOrCreateSurface()->generationID());
    bridge->canvas()->drawRect(SkRect::MakeXYWH(0, 0, 1, 1), paint);
    EXPECT_EQ(genID, bridge->getOrCreateSurface()->generationID());
    // This results in the internal surface being torn down in response to the
    // context loss.
    EXPECT_FALSE(bridge->checkSurfaceValid());
    EXPECT_EQ(nullptr, bridge->getOrCreateSurface());
    // The following passes by not crashing
    bridge->canvas()->drawRect(SkRect::MakeXYWH(0, 0, 1, 1), paint);
    bridge->flush();
  }

  void prepareMailboxWhenContextIsLost() {
    FakeGLES2Interface gl;
    std::unique_ptr<FakeWebGraphicsContext3DProvider> contextProvider =
        wrapUnique(new FakeWebGraphicsContext3DProvider(&gl));
    Canvas2DLayerBridgePtr bridge(adoptRef(new Canvas2DLayerBridge(
        std::move(contextProvider), IntSize(300, 150), 0, NonOpaque,
        Canvas2DLayerBridge::ForceAccelerationForTesting, nullptr,
        kN32_SkColorType)));

    // TODO(junov): The PrepareTextureMailbox() method will fail a DCHECK if we
    // don't do this before calling it the first time when the context is lost.
    bridge->prepareSurfaceForPaintingIfNeeded();

    // When the context is lost we are not sure if we should still be producing
    // GL frames for the compositor or not, so fail to generate frames.
    gl.setIsContextLost(true);

    cc::TextureMailbox textureMailbox;
    std::unique_ptr<cc::SingleReleaseCallback> releaseCallback;
    EXPECT_FALSE(
        bridge->PrepareTextureMailbox(&textureMailbox, &releaseCallback));
  }

  void prepareMailboxAndLoseResourceTest() {
    // Prepare a mailbox, then report the resource as lost.
    // This test passes by not crashing and not triggering assertions.
    {
      FakeGLES2Interface gl;
      std::unique_ptr<FakeWebGraphicsContext3DProvider> contextProvider =
          wrapUnique(new FakeWebGraphicsContext3DProvider(&gl));
      Canvas2DLayerBridgePtr bridge(adoptRef(new Canvas2DLayerBridge(
          std::move(contextProvider), IntSize(300, 150), 0, NonOpaque,
          Canvas2DLayerBridge::ForceAccelerationForTesting, nullptr,
          kN32_SkColorType)));

      cc::TextureMailbox textureMailbox;
      std::unique_ptr<cc::SingleReleaseCallback> releaseCallback;
      EXPECT_TRUE(
          bridge->PrepareTextureMailbox(&textureMailbox, &releaseCallback));

      bool lostResource = true;
      releaseCallback->Run(gpu::SyncToken(), lostResource);
    }

    // Retry with mailbox released while bridge destruction is in progress.
    {
      FakeGLES2Interface gl;
      std::unique_ptr<FakeWebGraphicsContext3DProvider> contextProvider =
          wrapUnique(new FakeWebGraphicsContext3DProvider(&gl));

      cc::TextureMailbox textureMailbox;
      std::unique_ptr<cc::SingleReleaseCallback> releaseCallback;

      {
        Canvas2DLayerBridgePtr bridge(adoptRef(new Canvas2DLayerBridge(
            std::move(contextProvider), IntSize(300, 150), 0, NonOpaque,
            Canvas2DLayerBridge::ForceAccelerationForTesting, nullptr,
            kN32_SkColorType)));
        bridge->PrepareTextureMailbox(&textureMailbox, &releaseCallback);
        // |bridge| goes out of scope and would normally be destroyed, but
        // object is kept alive by self references.
      }

      // This should cause the bridge to be destroyed.
      bool lostResource = true;
      // Before fixing crbug.com/411864, the following line would cause a memory
      // use after free that sometimes caused a crash in normal builds and
      // crashed consistently with ASAN.
      releaseCallback->Run(gpu::SyncToken(), lostResource);
    }
  }

  void accelerationHintTest() {
    {
      FakeGLES2Interface gl;
      std::unique_ptr<FakeWebGraphicsContext3DProvider> contextProvider =
          wrapUnique(new FakeWebGraphicsContext3DProvider(&gl));
      Canvas2DLayerBridgePtr bridge(adoptRef(new Canvas2DLayerBridge(
          std::move(contextProvider), IntSize(300, 300), 0, NonOpaque,
          Canvas2DLayerBridge::EnableAcceleration, nullptr, kN32_SkColorType)));
      SkPaint paint;
      bridge->canvas()->drawRect(SkRect::MakeXYWH(0, 0, 1, 1), paint);
      sk_sp<SkImage> image =
          bridge->newImageSnapshot(PreferAcceleration, SnapshotReasonUnitTests);
      EXPECT_TRUE(bridge->checkSurfaceValid());
      EXPECT_TRUE(bridge->isAccelerated());
    }

    {
      FakeGLES2Interface gl;
      std::unique_ptr<FakeWebGraphicsContext3DProvider> contextProvider =
          wrapUnique(new FakeWebGraphicsContext3DProvider(&gl));
      Canvas2DLayerBridgePtr bridge(adoptRef(new Canvas2DLayerBridge(
          std::move(contextProvider), IntSize(300, 300), 0, NonOpaque,
          Canvas2DLayerBridge::EnableAcceleration, nullptr, kN32_SkColorType)));
      SkPaint paint;
      bridge->canvas()->drawRect(SkRect::MakeXYWH(0, 0, 1, 1), paint);
      sk_sp<SkImage> image = bridge->newImageSnapshot(PreferNoAcceleration,
                                                      SnapshotReasonUnitTests);
      EXPECT_TRUE(bridge->checkSurfaceValid());
      EXPECT_FALSE(bridge->isAccelerated());
    }
  }
};

TEST_F(Canvas2DLayerBridgeTest, FullLifecycleSingleThreaded) {
  fullLifecycleTest();
}

TEST_F(Canvas2DLayerBridgeTest, NoDrawOnContextLost) {
  noDrawOnContextLostTest();
}

TEST_F(Canvas2DLayerBridgeTest, PrepareMailboxWhenContextIsLost) {
  prepareMailboxWhenContextIsLost();
}

TEST_F(Canvas2DLayerBridgeTest, PrepareMailboxAndLoseResource) {
  prepareMailboxAndLoseResourceTest();
}

TEST_F(Canvas2DLayerBridgeTest, AccelerationHint) {
  accelerationHintTest();
}

TEST_F(Canvas2DLayerBridgeTest, FallbackToSoftwareIfContextLost) {
  fallbackToSoftwareIfContextLost();
}

TEST_F(Canvas2DLayerBridgeTest, FallbackToSoftwareOnFailedTextureAlloc) {
  fallbackToSoftwareOnFailedTextureAlloc();
}

class MockLogger : public Canvas2DLayerBridge::Logger {
 public:
  MOCK_METHOD1(reportHibernationEvent,
               void(Canvas2DLayerBridge::HibernationEvent));
  MOCK_METHOD0(didStartHibernating, void());
  virtual ~MockLogger() {}
};

void runCreateBridgeTask(Canvas2DLayerBridgePtr* bridgePtr,
                         gpu::gles2::GLES2Interface* gl,
                         Canvas2DLayerBridgeTest* testHost,
                         WaitableEvent* doneEvent) {
  std::unique_ptr<FakeWebGraphicsContext3DProvider> contextProvider =
      makeUnique<FakeWebGraphicsContext3DProvider>(gl);
  *bridgePtr =
      testHost->makeBridge(std::move(contextProvider), IntSize(300, 300),
                           Canvas2DLayerBridge::EnableAcceleration);
  // draw+flush to trigger the creation of a GPU surface
  (*bridgePtr)->didDraw(FloatRect(0, 0, 1, 1));
  (*bridgePtr)->finalizeFrame(FloatRect(0, 0, 1, 1));
  (*bridgePtr)->flush();
  doneEvent->signal();
}

void postAndWaitCreateBridgeTask(const WebTraceLocation& location,
                                 WebThread* testThread,
                                 Canvas2DLayerBridgePtr* bridgePtr,
                                 gpu::gles2::GLES2Interface* gl,
                                 Canvas2DLayerBridgeTest* testHost) {
  std::unique_ptr<WaitableEvent> bridgeCreatedEvent =
      makeUnique<WaitableEvent>();
  testThread->getWebTaskRunner()->postTask(
      location, crossThreadBind(
                    &runCreateBridgeTask, crossThreadUnretained(bridgePtr),
                    crossThreadUnretained(gl), crossThreadUnretained(testHost),
                    crossThreadUnretained(bridgeCreatedEvent.get())));
  bridgeCreatedEvent->wait();
}

void runDestroyBridgeTask(Canvas2DLayerBridgePtr* bridgePtr,
                          WaitableEvent* doneEvent) {
  bridgePtr->clear();
  if (doneEvent)
    doneEvent->signal();
}

void postDestroyBridgeTask(const WebTraceLocation& location,
                           WebThread* testThread,
                           Canvas2DLayerBridgePtr* bridgePtr) {
  testThread->getWebTaskRunner()->postTask(
      location, crossThreadBind(&runDestroyBridgeTask,
                                crossThreadUnretained(bridgePtr), nullptr));
}

void postAndWaitDestroyBridgeTask(const WebTraceLocation& location,
                                  WebThread* testThread,
                                  Canvas2DLayerBridgePtr* bridgePtr) {
  std::unique_ptr<WaitableEvent> bridgeDestroyedEvent =
      makeUnique<WaitableEvent>();
  testThread->getWebTaskRunner()->postTask(
      location,
      crossThreadBind(&runDestroyBridgeTask, crossThreadUnretained(bridgePtr),
                      crossThreadUnretained(bridgeDestroyedEvent.get())));
  bridgeDestroyedEvent->wait();
}

void runSetIsHiddenTask(Canvas2DLayerBridge* bridge,
                        bool value,
                        WaitableEvent* doneEvent) {
  bridge->setIsHidden(value);
  if (doneEvent)
    doneEvent->signal();
}

void postSetIsHiddenTask(const WebTraceLocation& location,
                         WebThread* testThread,
                         Canvas2DLayerBridge* bridge,
                         bool value,
                         WaitableEvent* doneEvent = nullptr) {
  testThread->getWebTaskRunner()->postTask(
      location,
      crossThreadBind(&runSetIsHiddenTask, crossThreadUnretained(bridge), value,
                      crossThreadUnretained(doneEvent)));
}

void postAndWaitSetIsHiddenTask(const WebTraceLocation& location,
                                WebThread* testThread,
                                Canvas2DLayerBridge* bridge,
                                bool value) {
  std::unique_ptr<WaitableEvent> doneEvent = makeUnique<WaitableEvent>();
  postSetIsHiddenTask(location, testThread, bridge, value, doneEvent.get());
  doneEvent->wait();
}

class MockImageBuffer : public ImageBuffer {
 public:
  MockImageBuffer()
      : ImageBuffer(
            wrapUnique(new UnacceleratedImageBufferSurface(IntSize(1, 1)))) {}

  MOCK_CONST_METHOD1(resetCanvas, void(SkCanvas*));

  virtual ~MockImageBuffer() {}
};

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, HibernationLifeCycle)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_HibernationLifeCycle)
#endif
{
  FakeGLES2Interface gl;
  std::unique_ptr<WebThread> testThread =
      wrapUnique(Platform::current()->createThread("TestThread"));

  // The Canvas2DLayerBridge has to be created on the thread that will use it
  // to avoid WeakPtr thread check issues.
  Canvas2DLayerBridgePtr bridge;
  postAndWaitCreateBridgeTask(BLINK_FROM_HERE, testThread.get(), &bridge, &gl,
                              this);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mockLogger = wrapUnique(new MockLogger);
  MockLogger* mockLoggerPtr = mockLogger.get();
  bridge->setLoggerForTesting(std::move(mockLogger));

  // Test entering hibernation
  std::unique_ptr<WaitableEvent> hibernationStartedEvent =
      makeUnique<WaitableEvent>();
  EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(
                                  Canvas2DLayerBridge::HibernationScheduled));
  EXPECT_CALL(*mockLoggerPtr, didStartHibernating())
      .WillOnce(testing::Invoke(hibernationStartedEvent.get(),
                                &WaitableEvent::signal));
  postSetIsHiddenTask(BLINK_FROM_HERE, testThread.get(), bridge.get(), true);
  hibernationStartedEvent->wait();
  ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
  EXPECT_FALSE(bridge->isAccelerated());
  EXPECT_TRUE(bridge->isHibernating());
  EXPECT_TRUE(bridge->checkSurfaceValid());

  // Test exiting hibernation
  EXPECT_CALL(
      *mockLoggerPtr,
      reportHibernationEvent(Canvas2DLayerBridge::HibernationEndedNormally));
  postAndWaitSetIsHiddenTask(BLINK_FROM_HERE, testThread.get(), bridge.get(),
                             false);
  ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
  EXPECT_TRUE(bridge->isAccelerated());
  EXPECT_FALSE(bridge->isHibernating());
  EXPECT_TRUE(bridge->checkSurfaceValid());

  // Tear down the bridge on the thread so that 'bridge' can go out of scope
  // without crashing due to thread checks
  postAndWaitDestroyBridgeTask(BLINK_FROM_HERE, testThread.get(), &bridge);
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, HibernationReEntry)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_HibernationReEntry)
#endif
{
  FakeGLES2Interface gl;
  std::unique_ptr<WebThread> testThread =
      wrapUnique(Platform::current()->createThread("TestThread"));

  // The Canvas2DLayerBridge has to be created on the thread that will use it
  // to avoid WeakPtr thread check issues.
  Canvas2DLayerBridgePtr bridge;
  postAndWaitCreateBridgeTask(BLINK_FROM_HERE, testThread.get(), &bridge, &gl,
                              this);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mockLogger = wrapUnique(new MockLogger);
  MockLogger* mockLoggerPtr = mockLogger.get();
  bridge->setLoggerForTesting(std::move(mockLogger));

  // Test entering hibernation
  std::unique_ptr<WaitableEvent> hibernationStartedEvent =
      makeUnique<WaitableEvent>();
  EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(
                                  Canvas2DLayerBridge::HibernationScheduled));
  EXPECT_CALL(*mockLoggerPtr, didStartHibernating())
      .WillOnce(testing::Invoke(hibernationStartedEvent.get(),
                                &WaitableEvent::signal));
  postSetIsHiddenTask(BLINK_FROM_HERE, testThread.get(), bridge.get(), true);
  // Toggle visibility before the task that enters hibernation gets a
  // chance to run.
  postSetIsHiddenTask(BLINK_FROM_HERE, testThread.get(), bridge.get(), false);
  postSetIsHiddenTask(BLINK_FROM_HERE, testThread.get(), bridge.get(), true);

  hibernationStartedEvent->wait();
  ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
  EXPECT_FALSE(bridge->isAccelerated());
  EXPECT_TRUE(bridge->isHibernating());
  EXPECT_TRUE(bridge->checkSurfaceValid());

  // Test exiting hibernation
  EXPECT_CALL(
      *mockLoggerPtr,
      reportHibernationEvent(Canvas2DLayerBridge::HibernationEndedNormally));
  postAndWaitSetIsHiddenTask(BLINK_FROM_HERE, testThread.get(), bridge.get(),
                             false);
  ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
  EXPECT_TRUE(bridge->isAccelerated());
  EXPECT_FALSE(bridge->isHibernating());
  EXPECT_TRUE(bridge->checkSurfaceValid());

  // Tear down the bridge on the thread so that 'bridge' can go out of scope
  // without crashing due to thread checks
  postAndWaitDestroyBridgeTask(BLINK_FROM_HERE, testThread.get(), &bridge);
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest,
       HibernationLifeCycleWithDeferredRenderingDisabled)
#else
TEST_F(Canvas2DLayerBridgeTest,
       DISABLED_HibernationLifeCycleWithDeferredRenderingDisabled)
#endif
{
  FakeGLES2Interface gl;
  std::unique_ptr<WebThread> testThread =
      wrapUnique(Platform::current()->createThread("TestThread"));

  // The Canvas2DLayerBridge has to be created on the thread that will use it
  // to avoid WeakPtr thread check issues.
  Canvas2DLayerBridgePtr bridge;
  postAndWaitCreateBridgeTask(BLINK_FROM_HERE, testThread.get(), &bridge, &gl,
                              this);
  bridge->disableDeferral(DisableDeferralReasonUnknown);
  MockImageBuffer mockImageBuffer;
  EXPECT_CALL(mockImageBuffer, resetCanvas(_)).Times(AnyNumber());
  bridge->setImageBuffer(&mockImageBuffer);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mockLogger = wrapUnique(new MockLogger);
  MockLogger* mockLoggerPtr = mockLogger.get();
  bridge->setLoggerForTesting(std::move(mockLogger));

  // Test entering hibernation
  std::unique_ptr<WaitableEvent> hibernationStartedEvent =
      makeUnique<WaitableEvent>();
  EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(
                                  Canvas2DLayerBridge::HibernationScheduled));
  EXPECT_CALL(*mockLoggerPtr, didStartHibernating())
      .WillOnce(testing::Invoke(hibernationStartedEvent.get(),
                                &WaitableEvent::signal));
  postSetIsHiddenTask(BLINK_FROM_HERE, testThread.get(), bridge.get(), true);
  hibernationStartedEvent->wait();
  ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
  ::testing::Mock::VerifyAndClearExpectations(&mockImageBuffer);
  EXPECT_FALSE(bridge->isAccelerated());
  EXPECT_TRUE(bridge->isHibernating());
  EXPECT_TRUE(bridge->checkSurfaceValid());

  // Test exiting hibernation
  EXPECT_CALL(
      *mockLoggerPtr,
      reportHibernationEvent(Canvas2DLayerBridge::HibernationEndedNormally));
  EXPECT_CALL(mockImageBuffer, resetCanvas(_))
      .Times(AtLeast(1));  // Because deferred rendering is disabled
  postAndWaitSetIsHiddenTask(BLINK_FROM_HERE, testThread.get(), bridge.get(),
                             false);
  ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
  ::testing::Mock::VerifyAndClearExpectations(&mockImageBuffer);
  EXPECT_TRUE(bridge->isAccelerated());
  EXPECT_FALSE(bridge->isHibernating());
  EXPECT_TRUE(bridge->checkSurfaceValid());

  // Tear down the bridge on the thread so that 'bridge' can go out of scope
  // without crashing due to thread checks
  postAndWaitDestroyBridgeTask(BLINK_FROM_HERE, testThread.get(), &bridge);
}

void runRenderingTask(Canvas2DLayerBridge* bridge, WaitableEvent* doneEvent) {
  bridge->didDraw(FloatRect(0, 0, 1, 1));
  bridge->finalizeFrame(FloatRect(0, 0, 1, 1));
  bridge->flush();
  doneEvent->signal();
}

void postAndWaitRenderingTask(const WebTraceLocation& location,
                              WebThread* testThread,
                              Canvas2DLayerBridge* bridge) {
  std::unique_ptr<WaitableEvent> doneEvent = makeUnique<WaitableEvent>();
  testThread->getWebTaskRunner()->postTask(
      location,
      crossThreadBind(&runRenderingTask, crossThreadUnretained(bridge),
                      crossThreadUnretained(doneEvent.get())));
  doneEvent->wait();
}

#if CANVAS2D_HIBERNATION_ENABLED && CANVAS2D_BACKGROUND_RENDER_SWITCH_TO_CPU
TEST_F(Canvas2DLayerBridgeTest, BackgroundRenderingWhileHibernating)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_BackgroundRenderingWhileHibernating)
#endif
{
  FakeGLES2Interface gl;
  std::unique_ptr<WebThread> testThread =
      wrapUnique(Platform::current()->createThread("TestThread"));

  // The Canvas2DLayerBridge has to be created on the thread that will use it
  // to avoid WeakPtr thread check issues.
  Canvas2DLayerBridgePtr bridge;
  postAndWaitCreateBridgeTask(BLINK_FROM_HERE, testThread.get(), &bridge, &gl,
                              this);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mockLogger = wrapUnique(new MockLogger);
  MockLogger* mockLoggerPtr = mockLogger.get();
  bridge->setLoggerForTesting(std::move(mockLogger));

  // Test entering hibernation
  std::unique_ptr<WaitableEvent> hibernationStartedEvent =
      makeUnique<WaitableEvent>();
  EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(
                                  Canvas2DLayerBridge::HibernationScheduled));
  EXPECT_CALL(*mockLoggerPtr, didStartHibernating())
      .WillOnce(testing::Invoke(hibernationStartedEvent.get(),
                                &WaitableEvent::signal));
  postSetIsHiddenTask(BLINK_FROM_HERE, testThread.get(), bridge.get(), true);
  hibernationStartedEvent->wait();
  ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
  EXPECT_FALSE(bridge->isAccelerated());
  EXPECT_TRUE(bridge->isHibernating());
  EXPECT_TRUE(bridge->checkSurfaceValid());

  // Rendering in the background -> temp switch to SW
  EXPECT_CALL(*mockLoggerPtr,
              reportHibernationEvent(
                  Canvas2DLayerBridge::
                      HibernationEndedWithSwitchToBackgroundRendering));
  postAndWaitRenderingTask(BLINK_FROM_HERE, testThread.get(), bridge.get());
  ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
  EXPECT_FALSE(bridge->isAccelerated());
  EXPECT_FALSE(bridge->isHibernating());
  EXPECT_TRUE(bridge->checkSurfaceValid());

  // Unhide
  postAndWaitSetIsHiddenTask(BLINK_FROM_HERE, testThread.get(), bridge.get(),
                             false);
  ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
  EXPECT_TRUE(
      bridge->isAccelerated());  // Becoming visible causes switch back to GPU
  EXPECT_FALSE(bridge->isHibernating());
  EXPECT_TRUE(bridge->checkSurfaceValid());

  // Tear down the bridge on the thread so that 'bridge' can go out of scope
  // without crashing due to thread checks
  postAndWaitDestroyBridgeTask(BLINK_FROM_HERE, testThread.get(), &bridge);
}

#if CANVAS2D_HIBERNATION_ENABLED && CANVAS2D_BACKGROUND_RENDER_SWITCH_TO_CPU
TEST_F(Canvas2DLayerBridgeTest,
       BackgroundRenderingWhileHibernatingWithDeferredRenderingDisabled)
#else
TEST_F(
    Canvas2DLayerBridgeTest,
    DISABLED_BackgroundRenderingWhileHibernatingWithDeferredRenderingDisabled)
#endif
{
  FakeGLES2Interface gl;
  std::unique_ptr<WebThread> testThread =
      wrapUnique(Platform::current()->createThread("TestThread"));

  // The Canvas2DLayerBridge has to be created on the thread that will use it
  // to avoid WeakPtr thread check issues.
  Canvas2DLayerBridgePtr bridge;
  postAndWaitCreateBridgeTask(BLINK_FROM_HERE, testThread.get(), &bridge, &gl,
                              this);
  MockImageBuffer mockImageBuffer;
  EXPECT_CALL(mockImageBuffer, resetCanvas(_)).Times(AnyNumber());
  bridge->setImageBuffer(&mockImageBuffer);
  bridge->disableDeferral(DisableDeferralReasonUnknown);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mockLogger = wrapUnique(new MockLogger);
  MockLogger* mockLoggerPtr = mockLogger.get();
  bridge->setLoggerForTesting(std::move(mockLogger));

  // Test entering hibernation
  std::unique_ptr<WaitableEvent> hibernationStartedEvent =
      makeUnique<WaitableEvent>();
  EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(
                                  Canvas2DLayerBridge::HibernationScheduled));
  EXPECT_CALL(*mockLoggerPtr, didStartHibernating())
      .WillOnce(testing::Invoke(hibernationStartedEvent.get(),
                                &WaitableEvent::signal));
  postSetIsHiddenTask(BLINK_FROM_HERE, testThread.get(), bridge.get(), true);
  hibernationStartedEvent->wait();
  ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
  ::testing::Mock::VerifyAndClearExpectations(&mockImageBuffer);
  EXPECT_FALSE(bridge->isAccelerated());
  EXPECT_TRUE(bridge->isHibernating());
  EXPECT_TRUE(bridge->checkSurfaceValid());

  // Rendering in the background -> temp switch to SW
  EXPECT_CALL(*mockLoggerPtr,
              reportHibernationEvent(
                  Canvas2DLayerBridge::
                      HibernationEndedWithSwitchToBackgroundRendering));
  EXPECT_CALL(mockImageBuffer, resetCanvas(_)).Times(AtLeast(1));
  postAndWaitRenderingTask(BLINK_FROM_HERE, testThread.get(), bridge.get());
  ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
  ::testing::Mock::VerifyAndClearExpectations(&mockImageBuffer);
  EXPECT_FALSE(bridge->isAccelerated());
  EXPECT_FALSE(bridge->isHibernating());
  EXPECT_TRUE(bridge->checkSurfaceValid());

  // Unhide
  EXPECT_CALL(mockImageBuffer, resetCanvas(_)).Times(AtLeast(1));
  postAndWaitSetIsHiddenTask(BLINK_FROM_HERE, testThread.get(), bridge.get(),
                             false);
  ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
  ::testing::Mock::VerifyAndClearExpectations(&mockImageBuffer);
  EXPECT_TRUE(
      bridge->isAccelerated());  // Becoming visible causes switch back to GPU
  EXPECT_FALSE(bridge->isHibernating());
  EXPECT_TRUE(bridge->checkSurfaceValid());

  // Tear down the bridge on the thread so that 'bridge' can go out of scope
  // without crashing due to thread checks
  EXPECT_CALL(mockImageBuffer, resetCanvas(_)).Times(AnyNumber());
  postAndWaitDestroyBridgeTask(BLINK_FROM_HERE, testThread.get(), &bridge);
}

#if CANVAS2D_HIBERNATION_ENABLED && CANVAS2D_BACKGROUND_RENDER_SWITCH_TO_CPU
TEST_F(Canvas2DLayerBridgeTest, DisableDeferredRenderingWhileHibernating)
#else
TEST_F(Canvas2DLayerBridgeTest,
       DISABLED_DisableDeferredRenderingWhileHibernating)
#endif
{
  FakeGLES2Interface gl;
  std::unique_ptr<WebThread> testThread =
      wrapUnique(Platform::current()->createThread("TestThread"));

  // The Canvas2DLayerBridge has to be created on the thread that will use it
  // to avoid WeakPtr thread check issues.
  Canvas2DLayerBridgePtr bridge;
  postAndWaitCreateBridgeTask(BLINK_FROM_HERE, testThread.get(), &bridge, &gl,
                              this);
  MockImageBuffer mockImageBuffer;
  EXPECT_CALL(mockImageBuffer, resetCanvas(_)).Times(AnyNumber());
  bridge->setImageBuffer(&mockImageBuffer);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mockLogger = wrapUnique(new MockLogger);
  MockLogger* mockLoggerPtr = mockLogger.get();
  bridge->setLoggerForTesting(std::move(mockLogger));

  // Test entering hibernation
  std::unique_ptr<WaitableEvent> hibernationStartedEvent =
      makeUnique<WaitableEvent>();
  EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(
                                  Canvas2DLayerBridge::HibernationScheduled));
  EXPECT_CALL(*mockLoggerPtr, didStartHibernating())
      .WillOnce(testing::Invoke(hibernationStartedEvent.get(),
                                &WaitableEvent::signal));
  postSetIsHiddenTask(BLINK_FROM_HERE, testThread.get(), bridge.get(), true);
  hibernationStartedEvent->wait();
  ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
  ::testing::Mock::VerifyAndClearExpectations(&mockImageBuffer);
  EXPECT_FALSE(bridge->isAccelerated());
  EXPECT_TRUE(bridge->isHibernating());
  EXPECT_TRUE(bridge->checkSurfaceValid());

  // Disable deferral while background rendering
  EXPECT_CALL(*mockLoggerPtr,
              reportHibernationEvent(
                  Canvas2DLayerBridge::
                      HibernationEndedWithSwitchToBackgroundRendering));
  EXPECT_CALL(mockImageBuffer, resetCanvas(_)).Times(AtLeast(1));
  bridge->disableDeferral(DisableDeferralReasonUnknown);
  ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
  ::testing::Mock::VerifyAndClearExpectations(&mockImageBuffer);
  EXPECT_FALSE(bridge->isAccelerated());
  EXPECT_FALSE(bridge->isHibernating());
  EXPECT_TRUE(bridge->checkSurfaceValid());

  // Unhide
  EXPECT_CALL(mockImageBuffer, resetCanvas(_)).Times(AtLeast(1));
  postAndWaitSetIsHiddenTask(BLINK_FROM_HERE, testThread.get(), bridge.get(),
                             false);
  ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
  ::testing::Mock::VerifyAndClearExpectations(&mockImageBuffer);
  EXPECT_TRUE(
      bridge->isAccelerated());  // Becoming visible causes switch back to GPU
  EXPECT_FALSE(bridge->isHibernating());
  EXPECT_TRUE(bridge->checkSurfaceValid());

  // Tear down the bridge on the thread so that 'bridge' can go out of scope
  // without crashing due to thread checks
  EXPECT_CALL(mockImageBuffer, resetCanvas(_)).Times(AnyNumber());
  postAndWaitDestroyBridgeTask(BLINK_FROM_HERE, testThread.get(), &bridge);
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, TeardownWhileHibernating)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_TeardownWhileHibernating)
#endif
{
  FakeGLES2Interface gl;
  std::unique_ptr<WebThread> testThread =
      wrapUnique(Platform::current()->createThread("TestThread"));

  // The Canvas2DLayerBridge has to be created on the thread that will use it
  // to avoid WeakPtr thread check issues.
  Canvas2DLayerBridgePtr bridge;
  postAndWaitCreateBridgeTask(BLINK_FROM_HERE, testThread.get(), &bridge, &gl,
                              this);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mockLogger = wrapUnique(new MockLogger);
  MockLogger* mockLoggerPtr = mockLogger.get();
  bridge->setLoggerForTesting(std::move(mockLogger));

  // Test entering hibernation
  std::unique_ptr<WaitableEvent> hibernationStartedEvent =
      makeUnique<WaitableEvent>();
  EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(
                                  Canvas2DLayerBridge::HibernationScheduled));
  EXPECT_CALL(*mockLoggerPtr, didStartHibernating())
      .WillOnce(testing::Invoke(hibernationStartedEvent.get(),
                                &WaitableEvent::signal));
  postSetIsHiddenTask(BLINK_FROM_HERE, testThread.get(), bridge.get(), true);
  hibernationStartedEvent->wait();
  ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
  EXPECT_FALSE(bridge->isAccelerated());
  EXPECT_TRUE(bridge->isHibernating());
  EXPECT_TRUE(bridge->checkSurfaceValid());

  // Tear down the bridge while hibernating
  EXPECT_CALL(*mockLoggerPtr,
              reportHibernationEvent(
                  Canvas2DLayerBridge::HibernationEndedWithTeardown));
  postAndWaitDestroyBridgeTask(BLINK_FROM_HERE, testThread.get(), &bridge);
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, SnapshotWhileHibernating)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_SnapshotWhileHibernating)
#endif
{
  FakeGLES2Interface gl;
  std::unique_ptr<WebThread> testThread =
      wrapUnique(Platform::current()->createThread("TestThread"));

  // The Canvas2DLayerBridge has to be created on the thread that will use it
  // to avoid WeakPtr thread check issues.
  Canvas2DLayerBridgePtr bridge;
  postAndWaitCreateBridgeTask(BLINK_FROM_HERE, testThread.get(), &bridge, &gl,
                              this);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mockLogger = wrapUnique(new MockLogger);
  MockLogger* mockLoggerPtr = mockLogger.get();
  bridge->setLoggerForTesting(std::move(mockLogger));

  // Test entering hibernation
  std::unique_ptr<WaitableEvent> hibernationStartedEvent =
      makeUnique<WaitableEvent>();
  EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(
                                  Canvas2DLayerBridge::HibernationScheduled));
  EXPECT_CALL(*mockLoggerPtr, didStartHibernating())
      .WillOnce(testing::Invoke(hibernationStartedEvent.get(),
                                &WaitableEvent::signal));
  postSetIsHiddenTask(BLINK_FROM_HERE, testThread.get(), bridge.get(), true);
  hibernationStartedEvent->wait();
  ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
  EXPECT_FALSE(bridge->isAccelerated());
  EXPECT_TRUE(bridge->isHibernating());
  EXPECT_TRUE(bridge->checkSurfaceValid());

  // Take a snapshot and verify that it is not accelerated due to hibernation
  sk_sp<SkImage> image =
      bridge->newImageSnapshot(PreferAcceleration, SnapshotReasonUnitTests);
  EXPECT_FALSE(image->isTextureBacked());
  image.reset();

  // Verify that taking a snapshot did not affect the state of bridge
  EXPECT_FALSE(bridge->isAccelerated());
  EXPECT_TRUE(bridge->isHibernating());
  EXPECT_TRUE(bridge->checkSurfaceValid());

  // End hibernation normally
  std::unique_ptr<WaitableEvent> hibernationEndedEvent =
      makeUnique<WaitableEvent>();
  EXPECT_CALL(
      *mockLoggerPtr,
      reportHibernationEvent(Canvas2DLayerBridge::HibernationEndedNormally))
      .WillOnce(testing::InvokeWithoutArgs(hibernationEndedEvent.get(),
                                           &WaitableEvent::signal));
  postSetIsHiddenTask(BLINK_FROM_HERE, testThread.get(), bridge.get(), false);
  hibernationEndedEvent->wait();

  // Tear down the bridge while hibernating
  postAndWaitDestroyBridgeTask(BLINK_FROM_HERE, testThread.get(), &bridge);
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, TeardownWhileHibernationIsPending)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_TeardownWhileHibernationIsPending)
#endif
{
  FakeGLES2Interface gl;
  std::unique_ptr<WebThread> testThread =
      wrapUnique(Platform::current()->createThread("TestThread"));

  // The Canvas2DLayerBridge has to be created on the thread that will use it
  // to avoid WeakPtr thread check issues.
  Canvas2DLayerBridgePtr bridge;
  postAndWaitCreateBridgeTask(BLINK_FROM_HERE, testThread.get(), &bridge, &gl,
                              this);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mockLogger = wrapUnique(new MockLogger);
  MockLogger* mockLoggerPtr = mockLogger.get();
  bridge->setLoggerForTesting(std::move(mockLogger));

  // Test entering hibernation
  std::unique_ptr<WaitableEvent> hibernationScheduledEvent =
      makeUnique<WaitableEvent>();
  EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(
                                  Canvas2DLayerBridge::HibernationScheduled));
  postSetIsHiddenTask(BLINK_FROM_HERE, testThread.get(), bridge.get(), true,
                      hibernationScheduledEvent.get());
  postDestroyBridgeTask(BLINK_FROM_HERE, testThread.get(), &bridge);
  // In production, we would expect a
  // HibernationAbortedDueToDestructionWhileHibernatePending event to be
  // fired, but that signal is lost in the unit test due to no longer having
  // a bridge to hold the mockLogger.
  hibernationScheduledEvent->wait();
  // Once we know the hibernation task is scheduled, we can schedule a fence.
  // Assuming tasks are guaranteed to run in the order they were
  // submitted, this fence will guarantee the attempt to hibernate runs to
  // completion before the thread is destroyed.
  // This test passes by not crashing, which proves that the WeakPtr logic
  // is sound.
  std::unique_ptr<WaitableEvent> fenceEvent = makeUnique<WaitableEvent>();
  testThread->getWebTaskRunner()->postTask(
      BLINK_FROM_HERE,
      WTF::bind(&WaitableEvent::signal, unretained(fenceEvent.get())));
  fenceEvent->wait();
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, HibernationAbortedDueToPendingTeardown)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_HibernationAbortedDueToPendingTeardown)
#endif
{
  FakeGLES2Interface gl;
  std::unique_ptr<WebThread> testThread =
      wrapUnique(Platform::current()->createThread("TestThread"));

  // The Canvas2DLayerBridge has to be created on the thread that will use it
  // to avoid WeakPtr thread check issues.
  Canvas2DLayerBridgePtr bridge;
  postAndWaitCreateBridgeTask(BLINK_FROM_HERE, testThread.get(), &bridge, &gl,
                              this);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mockLogger = wrapUnique(new MockLogger);
  MockLogger* mockLoggerPtr = mockLogger.get();
  bridge->setLoggerForTesting(std::move(mockLogger));

  // Test entering hibernation
  std::unique_ptr<WaitableEvent> hibernationAbortedEvent =
      makeUnique<WaitableEvent>();
  EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(
                                  Canvas2DLayerBridge::HibernationScheduled));
  EXPECT_CALL(
      *mockLoggerPtr,
      reportHibernationEvent(
          Canvas2DLayerBridge::HibernationAbortedDueToPendingDestruction))
      .WillOnce(testing::InvokeWithoutArgs(hibernationAbortedEvent.get(),
                                           &WaitableEvent::signal));
  postSetIsHiddenTask(BLINK_FROM_HERE, testThread.get(), bridge.get(), true);
  testThread->getWebTaskRunner()->postTask(
      BLINK_FROM_HERE, crossThreadBind(&Canvas2DLayerBridge::beginDestruction,
                                       crossThreadUnretained(bridge.get())));
  hibernationAbortedEvent->wait();

  ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);

  // Tear down bridge on thread
  postAndWaitDestroyBridgeTask(BLINK_FROM_HERE, testThread.get(), &bridge);
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, HibernationAbortedDueToVisibilityChange)
#else
TEST_F(Canvas2DLayerBridgeTest,
       DISABLED_HibernationAbortedDueToVisibilityChange)
#endif
{
  FakeGLES2Interface gl;
  std::unique_ptr<WebThread> testThread =
      wrapUnique(Platform::current()->createThread("TestThread"));

  // The Canvas2DLayerBridge has to be created on the thread that will use it
  // to avoid WeakPtr thread check issues.
  Canvas2DLayerBridgePtr bridge;
  postAndWaitCreateBridgeTask(BLINK_FROM_HERE, testThread.get(), &bridge, &gl,
                              this);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mockLogger = wrapUnique(new MockLogger);
  MockLogger* mockLoggerPtr = mockLogger.get();
  bridge->setLoggerForTesting(std::move(mockLogger));

  // Test entering hibernation
  std::unique_ptr<WaitableEvent> hibernationAbortedEvent =
      makeUnique<WaitableEvent>();
  EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(
                                  Canvas2DLayerBridge::HibernationScheduled));
  EXPECT_CALL(*mockLoggerPtr,
              reportHibernationEvent(
                  Canvas2DLayerBridge::HibernationAbortedDueToVisibilityChange))
      .WillOnce(testing::InvokeWithoutArgs(hibernationAbortedEvent.get(),
                                           &WaitableEvent::signal));
  postSetIsHiddenTask(BLINK_FROM_HERE, testThread.get(), bridge.get(), true);
  postSetIsHiddenTask(BLINK_FROM_HERE, testThread.get(), bridge.get(), false);
  hibernationAbortedEvent->wait();
  ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
  EXPECT_TRUE(bridge->isAccelerated());
  EXPECT_FALSE(bridge->isHibernating());
  EXPECT_TRUE(bridge->checkSurfaceValid());

  // Tear down the bridge on the thread so that 'bridge' can go out of scope
  // without crashing due to thread checks
  postAndWaitDestroyBridgeTask(BLINK_FROM_HERE, testThread.get(), &bridge);
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, HibernationAbortedDueToLostContext)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_HibernationAbortedDueToLostContext)
#endif
{
  FakeGLES2Interface gl;
  std::unique_ptr<WebThread> testThread =
      wrapUnique(Platform::current()->createThread("TestThread"));

  // The Canvas2DLayerBridge has to be created on the thread that will use it
  // to avoid WeakPtr thread check issues.
  Canvas2DLayerBridgePtr bridge;
  postAndWaitCreateBridgeTask(BLINK_FROM_HERE, testThread.get(), &bridge, &gl,
                              this);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mockLogger = wrapUnique(new MockLogger);
  MockLogger* mockLoggerPtr = mockLogger.get();
  bridge->setLoggerForTesting(std::move(mockLogger));

  gl.setIsContextLost(true);
  // Test entering hibernation
  std::unique_ptr<WaitableEvent> hibernationAbortedEvent =
      makeUnique<WaitableEvent>();
  EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(
                                  Canvas2DLayerBridge::HibernationScheduled));
  EXPECT_CALL(*mockLoggerPtr,
              reportHibernationEvent(
                  Canvas2DLayerBridge::HibernationAbortedDueGpuContextLoss))
      .WillOnce(testing::InvokeWithoutArgs(hibernationAbortedEvent.get(),
                                           &WaitableEvent::signal));
  postSetIsHiddenTask(BLINK_FROM_HERE, testThread.get(), bridge.get(), true);
  hibernationAbortedEvent->wait();
  ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
  EXPECT_FALSE(bridge->isHibernating());

  // Tear down the bridge on the thread so that 'bridge' can go out of scope
  // without crashing due to thread checks
  postAndWaitDestroyBridgeTask(BLINK_FROM_HERE, testThread.get(), &bridge);
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, PrepareMailboxWhileHibernating)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_PrepareMailboxWhileHibernating)
#endif
{
  FakeGLES2Interface gl;
  std::unique_ptr<WebThread> testThread =
      wrapUnique(Platform::current()->createThread("TestThread"));

  // The Canvas2DLayerBridge has to be created on the thread that will use it
  // to avoid WeakPtr thread check issues.
  Canvas2DLayerBridgePtr bridge;
  postAndWaitCreateBridgeTask(BLINK_FROM_HERE, testThread.get(), &bridge, &gl,
                              this);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mockLogger = wrapUnique(new MockLogger);
  MockLogger* mockLoggerPtr = mockLogger.get();
  bridge->setLoggerForTesting(std::move(mockLogger));

  // Test entering hibernation
  std::unique_ptr<WaitableEvent> hibernationStartedEvent =
      makeUnique<WaitableEvent>();
  EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(
                                  Canvas2DLayerBridge::HibernationScheduled));
  EXPECT_CALL(*mockLoggerPtr, didStartHibernating())
      .WillOnce(testing::Invoke(hibernationStartedEvent.get(),
                                &WaitableEvent::signal));
  postSetIsHiddenTask(BLINK_FROM_HERE, testThread.get(), bridge.get(), true);
  hibernationStartedEvent->wait();
  ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);

  // Test prepareMailbox while hibernating
  cc::TextureMailbox textureMailbox;
  std::unique_ptr<cc::SingleReleaseCallback> releaseCallback;
  EXPECT_FALSE(
      bridge->PrepareTextureMailbox(&textureMailbox, &releaseCallback));
  EXPECT_TRUE(bridge->checkSurfaceValid());

  // Tear down the bridge on the thread so that 'bridge' can go out of scope
  // without crashing due to thread checks
  EXPECT_CALL(*mockLoggerPtr,
              reportHibernationEvent(
                  Canvas2DLayerBridge::HibernationEndedWithTeardown));
  postAndWaitDestroyBridgeTask(BLINK_FROM_HERE, testThread.get(), &bridge);
}

#if CANVAS2D_HIBERNATION_ENABLED && CANVAS2D_BACKGROUND_RENDER_SWITCH_TO_CPU
TEST_F(Canvas2DLayerBridgeTest, PrepareMailboxWhileBackgroundRendering)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_PrepareMailboxWhileBackgroundRendering)
#endif
{
  FakeGLES2Interface gl;
  std::unique_ptr<WebThread> testThread =
      wrapUnique(Platform::current()->createThread("TestThread"));

  // The Canvas2DLayerBridge has to be created on the thread that will use it
  // to avoid WeakPtr thread check issues.
  Canvas2DLayerBridgePtr bridge;
  postAndWaitCreateBridgeTask(BLINK_FROM_HERE, testThread.get(), &bridge, &gl,
                              this);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mockLogger = wrapUnique(new MockLogger);
  MockLogger* mockLoggerPtr = mockLogger.get();
  bridge->setLoggerForTesting(std::move(mockLogger));

  // Test entering hibernation
  std::unique_ptr<WaitableEvent> hibernationStartedEvent =
      makeUnique<WaitableEvent>();
  EXPECT_CALL(*mockLoggerPtr, reportHibernationEvent(
                                  Canvas2DLayerBridge::HibernationScheduled));
  EXPECT_CALL(*mockLoggerPtr, didStartHibernating())
      .WillOnce(testing::Invoke(hibernationStartedEvent.get(),
                                &WaitableEvent::signal));
  postSetIsHiddenTask(BLINK_FROM_HERE, testThread.get(), bridge.get(), true);
  hibernationStartedEvent->wait();
  ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);

  // Rendering in the background -> temp switch to SW
  EXPECT_CALL(*mockLoggerPtr,
              reportHibernationEvent(
                  Canvas2DLayerBridge::
                      HibernationEndedWithSwitchToBackgroundRendering));
  postAndWaitRenderingTask(BLINK_FROM_HERE, testThread.get(), bridge.get());
  ::testing::Mock::VerifyAndClearExpectations(mockLoggerPtr);
  EXPECT_FALSE(bridge->isAccelerated());
  EXPECT_FALSE(bridge->isHibernating());
  EXPECT_TRUE(bridge->checkSurfaceValid());

  // Test prepareMailbox while background rendering
  cc::TextureMailbox textureMailbox;
  std::unique_ptr<cc::SingleReleaseCallback> releaseCallback;
  EXPECT_FALSE(
      bridge->PrepareTextureMailbox(&textureMailbox, &releaseCallback));
  EXPECT_TRUE(bridge->checkSurfaceValid());

  // Tear down the bridge on the thread so that 'bridge' can go out of scope
  // without crashing due to thread checks
  postAndWaitDestroyBridgeTask(BLINK_FROM_HERE, testThread.get(), &bridge);
}

#if USE_IOSURFACE_FOR_2D_CANVAS
TEST_F(Canvas2DLayerBridgeTest, DeleteIOSurfaceAfterTeardown)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_DeleteIOSurfaceAfterTeardown)
#endif
{
  FakeGLES2InterfaceWithImageSupport gl;
  FakePlatformSupport testingPlatformSupport;
  std::unique_ptr<FakeWebGraphicsContext3DProvider> contextProvider =
      wrapUnique(new FakeWebGraphicsContext3DProvider(&gl));

  cc::TextureMailbox textureMailbox;
  std::unique_ptr<cc::SingleReleaseCallback> releaseCallback;

  {
    Canvas2DLayerBridgePtr bridge(adoptRef(new Canvas2DLayerBridge(
        std::move(contextProvider), IntSize(300, 150), 0, NonOpaque,
        Canvas2DLayerBridge::ForceAccelerationForTesting, nullptr,
        kN32_SkColorType)));
    bridge->PrepareTextureMailbox(&textureMailbox, &releaseCallback);
  }

  bool lostResource = false;
  releaseCallback->Run(gpu::SyncToken(), lostResource);

  EXPECT_EQ(1u, gl.createImageCount());
  EXPECT_EQ(1u, gl.destroyImageCount());
}

}  // namespace blink
