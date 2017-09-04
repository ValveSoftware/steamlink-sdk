// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/style/ClipPathOperation.h"

namespace blink {

void ReferenceClipPathOperation::addClient(SVGResourceClient* client) {
  m_elementProxy->addClient(client);
}

void ReferenceClipPathOperation::removeClient(SVGResourceClient* client) {
  m_elementProxy->removeClient(client);
}

SVGElement* ReferenceClipPathOperation::findElement(
    TreeScope& treeScope) const {
  return m_elementProxy->findElement(treeScope);
}

bool ReferenceClipPathOperation::operator==(const ClipPathOperation& o) const {
  if (!isSameType(o))
    return false;
  const ReferenceClipPathOperation& other = toReferenceClipPathOperation(o);
  return m_elementProxy == other.m_elementProxy && m_url == other.m_url;
}

}  // namespace blink
