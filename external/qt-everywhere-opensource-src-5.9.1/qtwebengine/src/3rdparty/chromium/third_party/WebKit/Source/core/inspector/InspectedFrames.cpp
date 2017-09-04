// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/InspectedFrames.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"

namespace blink {

InspectedFrames::InspectedFrames(LocalFrame* root) : m_root(root) {}

InspectedFrames::Iterator InspectedFrames::begin() {
  return Iterator(m_root, m_root);
}

InspectedFrames::Iterator InspectedFrames::end() {
  return Iterator(m_root, nullptr);
}

bool InspectedFrames::contains(LocalFrame* frame) const {
  return frame->instrumentingAgents() == m_root->instrumentingAgents();
}

LocalFrame* InspectedFrames::frameWithSecurityOrigin(
    const String& originRawString) {
  for (LocalFrame* frame : *this) {
    if (frame->document()->getSecurityOrigin()->toRawString() ==
        originRawString)
      return frame;
  }
  return nullptr;
}

InspectedFrames::Iterator::Iterator(LocalFrame* root, LocalFrame* current)
    : m_root(root), m_current(current) {}

InspectedFrames::Iterator& InspectedFrames::Iterator::operator++() {
  if (!m_current)
    return *this;
  Frame* frame = m_current->tree().traverseNext(m_root);
  m_current = nullptr;
  for (; frame; frame = frame->tree().traverseNext(m_root)) {
    if (!frame->isLocalFrame())
      continue;
    LocalFrame* local = toLocalFrame(frame);
    if (local->instrumentingAgents() == m_root->instrumentingAgents()) {
      m_current = local;
      break;
    }
  }
  return *this;
}

InspectedFrames::Iterator InspectedFrames::Iterator::operator++(int) {
  LocalFrame* old = m_current;
  ++*this;
  return Iterator(m_root, old);
}

bool InspectedFrames::Iterator::operator==(const Iterator& other) {
  return m_current == other.m_current && m_root == other.m_root;
}

bool InspectedFrames::Iterator::operator!=(const Iterator& other) {
  return !(*this == other);
}

DEFINE_TRACE(InspectedFrames) {
  visitor->trace(m_root);
}

}  // namespace blink
