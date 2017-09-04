// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/WebGLQuery.h"

#include "core/dom/TaskRunnerHelper.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "modules/webgl/WebGL2RenderingContextBase.h"
#include "public/platform/Platform.h"

namespace blink {

WebGLQuery* WebGLQuery::create(WebGL2RenderingContextBase* ctx) {
  return new WebGLQuery(ctx);
}

WebGLQuery::~WebGLQuery() {
  // See the comment in WebGLObject::detachAndDeleteObject().
  detachAndDeleteObject();
}

WebGLQuery::WebGLQuery(WebGL2RenderingContextBase* ctx)
    : WebGLSharedPlatform3DObject(ctx),
      m_target(0),
      m_canUpdateAvailability(false),
      m_queryResultAvailable(false),
      m_queryResult(0),
      m_taskRunner(TaskRunnerHelper::get(TaskType::Unthrottled,
                                         &ctx->canvas()->document())
                       ->clone()),
      m_cancellableTaskFactory(CancellableTaskFactory::create(
          this,
          &WebGLQuery::allowAvailabilityUpdate)) {
  GLuint query;
  ctx->contextGL()->GenQueriesEXT(1, &query);
  setObject(query);
}

void WebGLQuery::setTarget(GLenum target) {
  ASSERT(object());
  ASSERT(!m_target);
  m_target = target;
}

void WebGLQuery::deleteObjectImpl(gpu::gles2::GLES2Interface* gl) {
  gl->DeleteQueriesEXT(1, &m_object);
  m_object = 0;
}

void WebGLQuery::resetCachedResult() {
  m_canUpdateAvailability = false;
  m_queryResultAvailable = false;
  m_queryResult = 0;
  // When this is called, the implication is that we should start
  // keeping track of whether we can update the cached availability
  // and result.
  scheduleAllowAvailabilityUpdate();
}

void WebGLQuery::updateCachedResult(gpu::gles2::GLES2Interface* gl) {
  if (m_queryResultAvailable)
    return;

  if (!m_canUpdateAvailability)
    return;

  if (!hasTarget())
    return;

  // We can only update the cached result when control returns to the browser.
  m_canUpdateAvailability = false;
  GLuint available = 0;
  gl->GetQueryObjectuivEXT(object(), GL_QUERY_RESULT_AVAILABLE_EXT, &available);
  m_queryResultAvailable = !!available;
  if (m_queryResultAvailable) {
    GLuint result = 0;
    gl->GetQueryObjectuivEXT(object(), GL_QUERY_RESULT_EXT, &result);
    m_queryResult = result;
    m_cancellableTaskFactory->cancel();
  } else {
    scheduleAllowAvailabilityUpdate();
  }
}

bool WebGLQuery::isQueryResultAvailable() {
  return m_queryResultAvailable;
}

GLuint WebGLQuery::getQueryResult() {
  return m_queryResult;
}

void WebGLQuery::scheduleAllowAvailabilityUpdate() {
  if (!m_cancellableTaskFactory->isPending())
    m_taskRunner->postTask(BLINK_FROM_HERE,
                           m_cancellableTaskFactory->cancelAndCreate());
}

void WebGLQuery::allowAvailabilityUpdate() {
  m_canUpdateAvailability = true;
}

}  // namespace blink
