// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/WebGLTimerQueryEXT.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "modules/webgl/WebGLRenderingContextBase.h"
#include "public/platform/Platform.h"

namespace blink {

WebGLTimerQueryEXT* WebGLTimerQueryEXT::create(WebGLRenderingContextBase* ctx)
{
    return new WebGLTimerQueryEXT(ctx);
}

WebGLTimerQueryEXT::~WebGLTimerQueryEXT()
{
    unregisterTaskObserver();

    // See the comment in WebGLObject::detachAndDeleteObject().
    detachAndDeleteObject();
}

WebGLTimerQueryEXT::WebGLTimerQueryEXT(WebGLRenderingContextBase* ctx)
    : WebGLContextObject(ctx)
    , m_target(0)
    , m_queryId(0)
    , m_taskObserverRegistered(false)
    , m_canUpdateAvailability(false)
    , m_queryResultAvailable(false)
    , m_queryResult(0)
{
    context()->contextGL()->GenQueriesEXT(1, &m_queryId);
}

void WebGLTimerQueryEXT::resetCachedResult()
{
    m_canUpdateAvailability = false;
    m_queryResultAvailable = false;
    m_queryResult = 0;
    // When this is called, the implication is that we should start
    // keeping track of whether we can update the cached availability
    // and result.
    registerTaskObserver();
}

void WebGLTimerQueryEXT::updateCachedResult(gpu::gles2::GLES2Interface* gl)
{
    if (m_queryResultAvailable)
        return;

    if (!m_canUpdateAvailability)
        return;

    if (!hasTarget())
        return;

    // If this is a timestamp query, set the result to 0 and make it available as we don't support timestamps in WebGL due to very poor driver support for them
    if (m_target == GL_TIMESTAMP_EXT) {
        m_queryResult = 0;
        m_queryResultAvailable = true;
        return;
    }

    // We can only update the cached result when control returns to the browser.
    m_canUpdateAvailability = false;
    GLuint available = 0;
    gl->GetQueryObjectuivEXT(object(), GL_QUERY_RESULT_AVAILABLE_EXT, &available);
    m_queryResultAvailable = !!available;
    if (m_queryResultAvailable) {
        GLuint64 result = 0;
        gl->GetQueryObjectui64vEXT(object(), GL_QUERY_RESULT_EXT, &result);
        m_queryResult = result;
        unregisterTaskObserver();
    }
}

bool WebGLTimerQueryEXT::isQueryResultAvailable()
{
    return m_queryResultAvailable;
}

GLuint64 WebGLTimerQueryEXT::getQueryResult()
{
    return m_queryResult;
}

void WebGLTimerQueryEXT::deleteObjectImpl(gpu::gles2::GLES2Interface* gl)
{
    gl->DeleteQueriesEXT(1, &m_queryId);
    m_queryId = 0;
}

void WebGLTimerQueryEXT::registerTaskObserver()
{
    if (!m_taskObserverRegistered) {
        m_taskObserverRegistered = true;
        Platform::current()->currentThread()->addTaskObserver(this);
    }
}

void WebGLTimerQueryEXT::unregisterTaskObserver()
{
    if (m_taskObserverRegistered) {
        m_taskObserverRegistered = false;
        Platform::current()->currentThread()->removeTaskObserver(this);
    }
}

void WebGLTimerQueryEXT::didProcessTask()
{
    m_canUpdateAvailability = true;
}

} // namespace blink
