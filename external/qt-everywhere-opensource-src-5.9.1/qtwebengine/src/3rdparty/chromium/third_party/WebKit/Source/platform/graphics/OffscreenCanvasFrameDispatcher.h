// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OffscreenCanvasFrameDispatcher_h
#define OffscreenCanvasFrameDispatcher_h

#include "platform/PlatformExport.h"
#include "wtf/RefPtr.h"
#include "wtf/WeakPtr.h"

namespace blink {

class StaticBitmapImage;

class PLATFORM_EXPORT OffscreenCanvasFrameDispatcher {
 public:
  OffscreenCanvasFrameDispatcher() : m_weakPtrFactory(this) {}
  virtual ~OffscreenCanvasFrameDispatcher() {}
  virtual void dispatchFrame(RefPtr<StaticBitmapImage>,
                             double commitStartTime,
                             bool isWebGLSoftwareRendering = false) = 0;
  virtual void reclaimResource(unsigned resourceId) = 0;

  WeakPtr<OffscreenCanvasFrameDispatcher> createWeakPtr() {
    return m_weakPtrFactory.createWeakPtr();
  }

 private:
  WeakPtrFactory<OffscreenCanvasFrameDispatcher> m_weakPtrFactory;
};

}  // namespace blink

#endif  // OffscreenCanvasFrameDispatcher_h
