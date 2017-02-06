// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/InspectorResourceContainer.h"

#include "core/inspector/InspectedFrames.h"

namespace blink {

InspectorResourceContainer::InspectorResourceContainer(InspectedFrames* inspectedFrames)
    : m_inspectedFrames(inspectedFrames)
{
}

InspectorResourceContainer::~InspectorResourceContainer()
{
}

DEFINE_TRACE(InspectorResourceContainer)
{
    visitor->trace(m_inspectedFrames);
}

void InspectorResourceContainer::didCommitLoadForLocalFrame(LocalFrame* frame)
{
    if (frame != m_inspectedFrames->root())
        return;
    m_styleSheetContents.clear();
    m_styleElementContents.clear();
}

void InspectorResourceContainer::storeStyleSheetContent(const String& url, const String& content)
{
    m_styleSheetContents.set(url, content);
}

bool InspectorResourceContainer::loadStyleSheetContent(const String& url, String* content)
{
    if (!m_styleSheetContents.contains(url))
        return false;
    *content = m_styleSheetContents.get(url);
    return true;
}

void InspectorResourceContainer::storeStyleElementContent(int backendNodeId, const String& content)
{
    m_styleElementContents.set(backendNodeId, content);
}

bool InspectorResourceContainer::loadStyleElementContent(int backendNodeId, String* content)
{
    if (!m_styleElementContents.contains(backendNodeId))
        return false;
    *content = m_styleElementContents.get(backendNodeId);
    return true;
}

} // namespace blink
