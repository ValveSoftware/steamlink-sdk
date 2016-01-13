/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "platform/graphics/GraphicsContextAnnotation.h"

#include "platform/graphics/GraphicsContext.h"

namespace {

const char* AnnotationKeyRendererName  = "RENDERER";
const char* AnnotationKeyPaintPhase    = "PHASE";
const char* AnnotationKeyElementId     = "ID";
const char* AnnotationKeyElementClass  = "CLASS";
const char* AnnotationKeyElementTag    = "TAG";

}

namespace WebCore {

GraphicsContextAnnotation::GraphicsContextAnnotation(const char* rendererName, const char* paintPhase, const String& elementId, const String& elementClass, const String& elementTag)
    : m_rendererName(rendererName)
    , m_paintPhase(paintPhase)
    , m_elementId(elementId)
    , m_elementClass(elementClass)
    , m_elementTag(elementTag)
{
}

void GraphicsContextAnnotation::asAnnotationList(AnnotationList &list) const
{
    list.clear();

    if (m_rendererName)
        list.append(std::make_pair(AnnotationKeyRendererName, m_rendererName));

    if (m_paintPhase)
        list.append(std::make_pair(AnnotationKeyPaintPhase, m_paintPhase));

    if (!m_elementId.isEmpty())
        list.append(std::make_pair(AnnotationKeyElementId, m_elementId));

    if (!m_elementClass.isEmpty())
        list.append(std::make_pair(AnnotationKeyElementClass, m_elementClass));

    if (!m_elementTag.isEmpty())
        list.append(std::make_pair(AnnotationKeyElementTag, m_elementTag));
}

}
