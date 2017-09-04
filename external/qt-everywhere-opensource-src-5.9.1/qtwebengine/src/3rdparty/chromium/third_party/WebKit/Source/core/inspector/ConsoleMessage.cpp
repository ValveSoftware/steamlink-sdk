// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/ConsoleMessage.h"

#include "bindings/core/v8/SourceLocation.h"
#include "wtf/CurrentTime.h"

namespace blink {

// static
ConsoleMessage* ConsoleMessage::createForRequest(
    MessageSource source,
    MessageLevel level,
    const String& message,
    const String& url,
    unsigned long requestIdentifier) {
  ConsoleMessage* consoleMessage = ConsoleMessage::create(
      source, level, message, SourceLocation::capture(url, 0, 0));
  consoleMessage->m_requestIdentifier = requestIdentifier;
  return consoleMessage;
}

// static
ConsoleMessage* ConsoleMessage::create(
    MessageSource source,
    MessageLevel level,
    const String& message,
    std::unique_ptr<SourceLocation> location) {
  return new ConsoleMessage(source, level, message, std::move(location));
}

// static
ConsoleMessage* ConsoleMessage::create(MessageSource source,
                                       MessageLevel level,
                                       const String& message) {
  return ConsoleMessage::create(source, level, message,
                                SourceLocation::capture());
}

// static
ConsoleMessage* ConsoleMessage::createFromWorker(
    MessageLevel level,
    const String& message,
    std::unique_ptr<SourceLocation> location,
    const String& workerId) {
  ConsoleMessage* consoleMessage = ConsoleMessage::create(
      WorkerMessageSource, level, message, std::move(location));
  consoleMessage->m_workerId = workerId;
  return consoleMessage;
}

ConsoleMessage::ConsoleMessage(MessageSource source,
                               MessageLevel level,
                               const String& message,
                               std::unique_ptr<SourceLocation> location)
    : m_source(source),
      m_level(level),
      m_message(message),
      m_location(std::move(location)),
      m_requestIdentifier(0),
      m_timestamp(WTF::currentTimeMS()) {}

ConsoleMessage::~ConsoleMessage() {}

SourceLocation* ConsoleMessage::location() const {
  return m_location.get();
}

unsigned long ConsoleMessage::requestIdentifier() const {
  return m_requestIdentifier;
}

double ConsoleMessage::timestamp() const {
  return m_timestamp;
}

MessageSource ConsoleMessage::source() const {
  return m_source;
}

MessageLevel ConsoleMessage::level() const {
  return m_level;
}

const String& ConsoleMessage::message() const {
  return m_message;
}

const String& ConsoleMessage::workerId() const {
  return m_workerId;
}

DEFINE_TRACE(ConsoleMessage) {}

}  // namespace blink
