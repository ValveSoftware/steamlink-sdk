// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLTimerQueryEXT_h
#define WebGLTimerQueryEXT_h

#include "modules/webgl/WebGLContextObject.h"

#include "public/platform/WebThread.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace blink {

class WebGLTimerQueryEXT : public WebGLContextObject, public WebThread::TaskObserver {
    DEFINE_WRAPPERTYPEINFO();

public:
    static WebGLTimerQueryEXT* create(WebGLRenderingContextBase*);
    ~WebGLTimerQueryEXT() override;

    void setTarget(GLenum target) { m_target = target; }

    GLuint object() const { return m_queryId; }
    bool hasTarget() const { return m_target != 0; }
    GLenum target() const { return m_target; }

    void resetCachedResult();
    void updateCachedResult(gpu::gles2::GLES2Interface*);

    bool isQueryResultAvailable();
    GLuint64 getQueryResult();

protected:
    WebGLTimerQueryEXT(WebGLRenderingContextBase*);

private:
    bool hasObject() const override { return m_queryId != 0; }
    void deleteObjectImpl(gpu::gles2::GLES2Interface*) override;

    void registerTaskObserver();
    void unregisterTaskObserver();

    // TaskObserver implementation.
    void didProcessTask() override;
    void willProcessTask() override { }

    GLenum m_target;
    GLuint m_queryId;

    bool m_taskObserverRegistered;
    bool m_canUpdateAvailability;
    bool m_queryResultAvailable;
    GLuint64 m_queryResult;
};

} // namespace blink

#endif // WebGLTimerQueryEXT_h
