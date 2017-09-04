/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/frame/FrameConsole.h"

#include "bindings/core/v8/SourceLocation.h"
#include "core/frame/FrameHost.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/ConsoleMessageStorage.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceResponse.h"
#include "wtf/text/StringBuilder.h"
#include <memory>

namespace blink {

FrameConsole::FrameConsole(LocalFrame& frame) : m_frame(&frame) {}

void FrameConsole::addMessage(ConsoleMessage* consoleMessage) {
  if (addMessageToStorage(consoleMessage))
    reportMessageToClient(consoleMessage->source(), consoleMessage->level(),
                          consoleMessage->message(),
                          consoleMessage->location());
}

bool FrameConsole::addMessageToStorage(ConsoleMessage* consoleMessage) {
  if (!m_frame->document() || !m_frame->host())
    return false;
  m_frame->host()->consoleMessageStorage().addConsoleMessage(
      m_frame->document(), consoleMessage);
  return true;
}

void FrameConsole::reportMessageToClient(MessageSource source,
                                         MessageLevel level,
                                         const String& message,
                                         SourceLocation* location) {
  if (source == NetworkMessageSource)
    return;

  String url = location->url();
  String stackTrace;
  if (source == ConsoleAPIMessageSource) {
    if (!m_frame->host())
      return;
    if (m_frame->chromeClient().shouldReportDetailedMessageForSource(*m_frame,
                                                                     url)) {
      std::unique_ptr<SourceLocation> fullLocation =
          SourceLocation::captureWithFullStackTrace();
      if (!fullLocation->isUnknown())
        stackTrace = fullLocation->toString();
    }
  } else {
    if (!location->isUnknown() &&
        m_frame->chromeClient().shouldReportDetailedMessageForSource(*m_frame,
                                                                     url))
      stackTrace = location->toString();
  }

  m_frame->chromeClient().addMessageToConsole(
      m_frame, source, level, message, location->lineNumber(), url, stackTrace);
}

void FrameConsole::addMessageFromWorker(
    MessageLevel level,
    const String& message,
    std::unique_ptr<SourceLocation> location,
    const String& workerId) {
  reportMessageToClient(WorkerMessageSource, level, message, location.get());
  addMessageToStorage(ConsoleMessage::createFromWorker(
      level, message, std::move(location), workerId));
}

void FrameConsole::addSingletonMessage(ConsoleMessage* consoleMessage) {
  if (m_singletonMessages.contains(consoleMessage->message()))
    return;
  m_singletonMessages.add(consoleMessage->message());
  addMessage(consoleMessage);
}

void FrameConsole::reportResourceResponseReceived(
    DocumentLoader* loader,
    unsigned long requestIdentifier,
    const ResourceResponse& response) {
  if (!loader)
    return;
  if (response.httpStatusCode() < 400)
    return;
  if (response.wasFallbackRequiredByServiceWorker())
    return;
  String message =
      "Failed to load resource: the server responded with a status of " +
      String::number(response.httpStatusCode()) + " (" +
      response.httpStatusText() + ')';
  ConsoleMessage* consoleMessage = ConsoleMessage::createForRequest(
      NetworkMessageSource, ErrorMessageLevel, message,
      response.url().getString(), requestIdentifier);
  addMessage(consoleMessage);
}

void FrameConsole::didFailLoading(unsigned long requestIdentifier,
                                  const ResourceError& error) {
  if (error.isCancellation())  // Report failures only.
    return;
  StringBuilder message;
  message.append("Failed to load resource");
  if (!error.localizedDescription().isEmpty()) {
    message.append(": ");
    message.append(error.localizedDescription());
  }
  addMessageToStorage(ConsoleMessage::createForRequest(
      NetworkMessageSource, ErrorMessageLevel, message.toString(),
      error.failingURL(), requestIdentifier));
}

DEFINE_TRACE(FrameConsole) {
  visitor->trace(m_frame);
}

}  // namespace blink
