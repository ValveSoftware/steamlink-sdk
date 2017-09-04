// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/ConsoleMessageStorage.h"

#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorInstrumentation.h"

namespace blink {

static const unsigned maxConsoleMessageCount = 1000;

ConsoleMessageStorage::ConsoleMessageStorage() : m_expiredCount(0) {}

void ConsoleMessageStorage::addConsoleMessage(ExecutionContext* context,
                                              ConsoleMessage* message) {
  InspectorInstrumentation::consoleMessageAdded(context, message);
  DCHECK(m_messages.size() <= maxConsoleMessageCount);
  if (m_messages.size() == maxConsoleMessageCount) {
    ++m_expiredCount;
    m_messages.removeFirst();
  }
  m_messages.append(message);
}

void ConsoleMessageStorage::clear() {
  m_messages.clear();
  m_expiredCount = 0;
}

size_t ConsoleMessageStorage::size() const {
  return m_messages.size();
}

ConsoleMessage* ConsoleMessageStorage::at(size_t index) const {
  return m_messages[index].get();
}

int ConsoleMessageStorage::expiredCount() const {
  return m_expiredCount;
}

DEFINE_TRACE(ConsoleMessageStorage) {
  visitor->trace(m_messages);
}

}  // namespace blink
