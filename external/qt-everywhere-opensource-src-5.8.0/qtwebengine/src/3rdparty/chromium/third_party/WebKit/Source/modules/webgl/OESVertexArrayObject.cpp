/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/webgl/OESVertexArrayObject.h"

#include "bindings/core/v8/ExceptionState.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "modules/webgl/WebGLRenderingContextBase.h"
#include "modules/webgl/WebGLVertexArrayObjectOES.h"

namespace blink {

OESVertexArrayObject::OESVertexArrayObject(WebGLRenderingContextBase* context)
    : WebGLExtension(context)
{
    context->extensionsUtil()->ensureExtensionEnabled("GL_OES_vertex_array_object");
}

OESVertexArrayObject::~OESVertexArrayObject()
{
}

WebGLExtensionName OESVertexArrayObject::name() const
{
    return OESVertexArrayObjectName;
}

OESVertexArrayObject* OESVertexArrayObject::create(WebGLRenderingContextBase* context)
{
    return new OESVertexArrayObject(context);
}

WebGLVertexArrayObjectOES* OESVertexArrayObject::createVertexArrayOES()
{
    WebGLExtensionScopedContext scoped(this);
    if (scoped.isLost())
        return nullptr;

    WebGLVertexArrayObjectOES* o = WebGLVertexArrayObjectOES::create(scoped.context(), WebGLVertexArrayObjectOES::VaoTypeUser);
    scoped.context()->addContextObject(o);
    return o;
}

void OESVertexArrayObject::deleteVertexArrayOES(ScriptState* scriptState, WebGLVertexArrayObjectOES* arrayObject)
{
    WebGLExtensionScopedContext scoped(this);
    if (!arrayObject || scoped.isLost())
        return;

    if (!arrayObject->isDefaultObject() && arrayObject == scoped.context()->m_boundVertexArrayObject)
        scoped.context()->setBoundVertexArrayObject(scriptState, nullptr);

    arrayObject->deleteObject(scoped.context()->contextGL());
}

GLboolean OESVertexArrayObject::isVertexArrayOES(WebGLVertexArrayObjectOES* arrayObject)
{
    WebGLExtensionScopedContext scoped(this);
    if (!arrayObject || scoped.isLost())
        return 0;

    if (!arrayObject->hasEverBeenBound())
        return 0;

    return scoped.context()->contextGL()->IsVertexArrayOES(arrayObject->object());
}

void OESVertexArrayObject::bindVertexArrayOES(ScriptState* scriptState, WebGLVertexArrayObjectOES* arrayObject)
{
    WebGLExtensionScopedContext scoped(this);
    if (scoped.isLost())
        return;

    if (arrayObject && (arrayObject->isDeleted() || !arrayObject->validate(0, scoped.context()))) {
        scoped.context()->synthesizeGLError(GL_INVALID_OPERATION, "bindVertexArrayOES", "invalid arrayObject");
        return;
    }

    if (arrayObject && !arrayObject->isDefaultObject() && arrayObject->object()) {
        scoped.context()->contextGL()->BindVertexArrayOES(arrayObject->object());

        arrayObject->setHasEverBeenBound();
        scoped.context()->setBoundVertexArrayObject(scriptState, arrayObject);
    } else {
        scoped.context()->contextGL()->BindVertexArrayOES(0);
        scoped.context()->setBoundVertexArrayObject(scriptState, nullptr);
    }
}

bool OESVertexArrayObject::supported(WebGLRenderingContextBase* context)
{
    return context->extensionsUtil()->supportsExtension("GL_OES_vertex_array_object");
}

const char* OESVertexArrayObject::extensionName()
{
    return "OES_vertex_array_object";
}

} // namespace blink
