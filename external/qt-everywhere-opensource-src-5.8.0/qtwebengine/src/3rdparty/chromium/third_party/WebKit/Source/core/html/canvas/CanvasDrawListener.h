// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanvasDrawListener_h
#define CanvasDrawListener_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebCanvasCaptureHandler.h"
#include "wtf/PassRefPtr.h"
#include <memory>

class SkImage;

namespace blink {

class CORE_EXPORT CanvasDrawListener : public GarbageCollectedMixin {
public:
    virtual ~CanvasDrawListener();
    virtual void sendNewFrame(const WTF::PassRefPtr<SkImage>&);
    bool needsNewFrame() const;
    void requestFrame();

protected:
    explicit CanvasDrawListener(std::unique_ptr<WebCanvasCaptureHandler>);

    bool m_frameCaptureRequested;
    std::unique_ptr<WebCanvasCaptureHandler> m_handler;
};

} // namespace blink

#endif
