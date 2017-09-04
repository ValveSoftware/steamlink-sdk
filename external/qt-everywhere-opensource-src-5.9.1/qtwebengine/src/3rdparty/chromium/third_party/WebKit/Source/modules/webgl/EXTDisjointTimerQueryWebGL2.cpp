// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/EXTDisjointTimerQueryWebGL2.h"

#include "bindings/modules/v8/WebGLAny.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "modules/webgl/WebGLRenderingContextBase.h"

namespace blink {

EXTDisjointTimerQueryWebGL2::~EXTDisjointTimerQueryWebGL2() {}

WebGLExtensionName EXTDisjointTimerQueryWebGL2::name() const {
  return EXTDisjointTimerQueryWebGL2Name;
}

EXTDisjointTimerQueryWebGL2* EXTDisjointTimerQueryWebGL2::create(
    WebGLRenderingContextBase* context) {
  EXTDisjointTimerQueryWebGL2* o = new EXTDisjointTimerQueryWebGL2(context);
  return o;
}

bool EXTDisjointTimerQueryWebGL2::supported(
    WebGLRenderingContextBase* context) {
  return context->extensionsUtil()->supportsExtension(
      "GL_EXT_disjoint_timer_query");
}

const char* EXTDisjointTimerQueryWebGL2::extensionName() {
  return "EXT_disjoint_timer_query_webgl2";
}

void EXTDisjointTimerQueryWebGL2::queryCounterEXT(WebGLQuery* query,
                                                  GLenum target) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.isLost())
    return;

  DCHECK(query);
  if (query->isDeleted() ||
      !query->validate(scoped.context()->contextGroup(), scoped.context())) {
    scoped.context()->synthesizeGLError(GL_INVALID_OPERATION, "queryCounterEXT",
                                        "invalid query");
    return;
  }

  if (target != GL_TIMESTAMP_EXT) {
    scoped.context()->synthesizeGLError(GL_INVALID_ENUM, "queryCounterEXT",
                                        "invalid target");
    return;
  }

  if (query->hasTarget() && query->getTarget() != target) {
    scoped.context()->synthesizeGLError(GL_INVALID_OPERATION, "queryCounterEXT",
                                        "target does not match query");
    return;
  }

  // Timestamps are disabled in WebGL due to lack of driver support on multiple
  // platforms, so we don't actually perform a GL call.
  query->setTarget(target);
  query->resetCachedResult();
}

DEFINE_TRACE(EXTDisjointTimerQueryWebGL2) {
  WebGLExtension::trace(visitor);
}

EXTDisjointTimerQueryWebGL2::EXTDisjointTimerQueryWebGL2(
    WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->extensionsUtil()->ensureExtensionEnabled(
      "GL_EXT_disjoint_timer_query_webgl2");
}

}  // namespace blink
