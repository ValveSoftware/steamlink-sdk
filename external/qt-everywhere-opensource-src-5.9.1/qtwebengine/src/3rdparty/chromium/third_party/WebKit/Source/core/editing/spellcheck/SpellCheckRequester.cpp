/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "core/editing/spellcheck/SpellCheckRequester.h"

#include "core/dom/Document.h"
#include "core/dom/Node.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "platform/text/TextCheckerClient.h"

namespace blink {

SpellCheckRequest::SpellCheckRequest(
    Range* checkingRange,
    const String& text,
    const Vector<uint32_t>& documentMarkersInRange,
    const Vector<unsigned>& documentMarkerOffsets,
    int requestNumber)
    : m_requester(nullptr),
      m_checkingRange(checkingRange),
      m_rootEditableElement(
          blink::rootEditableElement(*m_checkingRange->startContainer())),
      m_requestData(unrequestedTextCheckingSequence,
                    text,
                    documentMarkersInRange,
                    documentMarkerOffsets),
      m_requestNumber(requestNumber) {
  DCHECK(m_checkingRange);
  DCHECK(m_checkingRange->isConnected());
  DCHECK(m_rootEditableElement);
}

SpellCheckRequest::~SpellCheckRequest() {}

DEFINE_TRACE(SpellCheckRequest) {
  visitor->trace(m_requester);
  visitor->trace(m_checkingRange);
  visitor->trace(m_rootEditableElement);
  TextCheckingRequest::trace(visitor);
}

void SpellCheckRequest::dispose() {
  if (m_checkingRange)
    m_checkingRange->dispose();
}

// static
SpellCheckRequest* SpellCheckRequest::create(
    const EphemeralRange& checkingRange,
    int requestNumber) {
  if (checkingRange.isNull())
    return nullptr;
  if (!blink::rootEditableElement(
          *checkingRange.startPosition().computeContainerNode()))
    return nullptr;

  String text =
      plainText(checkingRange, TextIteratorEmitsObjectReplacementCharacter);
  if (text.isEmpty())
    return nullptr;

  Range* checkingRangeObject = createRange(checkingRange);

  const DocumentMarkerVector& markers =
      checkingRangeObject->ownerDocument().markers().markersInRange(
          checkingRange, DocumentMarker::SpellCheckClientMarkers());
  Vector<uint32_t> hashes(markers.size());
  Vector<unsigned> offsets(markers.size());
  for (size_t i = 0; i < markers.size(); ++i) {
    hashes[i] = markers[i]->hash();
    offsets[i] = markers[i]->startOffset();
  }

  return new SpellCheckRequest(checkingRangeObject, text, hashes, offsets,
                               requestNumber);
}

const TextCheckingRequestData& SpellCheckRequest::data() const {
  return m_requestData;
}

bool SpellCheckRequest::isValid() const {
  return m_checkingRange->isConnected() && m_rootEditableElement->isConnected();
}

void SpellCheckRequest::didSucceed(const Vector<TextCheckingResult>& results) {
  if (!m_requester)
    return;
  SpellCheckRequester* requester = m_requester;
  m_requester = nullptr;
  requester->didCheckSucceed(m_requestData.sequence(), results);
}

void SpellCheckRequest::didCancel() {
  if (!m_requester)
    return;
  SpellCheckRequester* requester = m_requester;
  m_requester = nullptr;
  requester->didCheckCancel(m_requestData.sequence());
}

void SpellCheckRequest::setCheckerAndSequence(SpellCheckRequester* requester,
                                              int sequence) {
  DCHECK(!m_requester);
  DCHECK_EQ(m_requestData.sequence(), unrequestedTextCheckingSequence);
  m_requester = requester;
  m_requestData.m_sequence = sequence;
}

SpellCheckRequester::SpellCheckRequester(LocalFrame& frame)
    : m_frame(&frame),
      m_lastRequestSequence(0),
      m_lastProcessedSequence(0),
      m_timerToProcessQueuedRequest(
          this,
          &SpellCheckRequester::timerFiredToProcessQueuedRequest) {}

SpellCheckRequester::~SpellCheckRequester() {}

TextCheckerClient& SpellCheckRequester::client() const {
  return frame().spellChecker().textChecker();
}

void SpellCheckRequester::timerFiredToProcessQueuedRequest(TimerBase*) {
  DCHECK(!m_requestQueue.isEmpty());
  if (m_requestQueue.isEmpty())
    return;

  invokeRequest(m_requestQueue.takeFirst());
}

void SpellCheckRequester::requestCheckingFor(SpellCheckRequest* request) {
  if (!request)
    return;

  DCHECK_EQ(request->data().sequence(), unrequestedTextCheckingSequence);
  int sequence = ++m_lastRequestSequence;
  if (sequence == unrequestedTextCheckingSequence)
    sequence = ++m_lastRequestSequence;

  request->setCheckerAndSequence(this, sequence);

  if (m_timerToProcessQueuedRequest.isActive() || m_processingRequest) {
    enqueueRequest(request);
    return;
  }

  invokeRequest(request);
}

void SpellCheckRequester::cancelCheck() {
  if (m_processingRequest)
    m_processingRequest->didCancel();
}

void SpellCheckRequester::prepareForLeakDetection() {
  m_timerToProcessQueuedRequest.stop();
  // Empty the queue of pending requests to prevent it being a leak source.
  // Pending spell checker requests are cancellable requests not representing
  // leaks, just async work items waiting to be processed.
  //
  // Rather than somehow wait for this async queue to drain before running
  // the leak detector, they're all cancelled to prevent flaky leaks being
  // reported.
  m_requestQueue.clear();
  // WebSpellCheckClient stores a set of WebTextCheckingCompletion objects,
  // which may store references to already invoked requests. We should clear
  // these references to prevent them from being a leak source.
  client().cancelAllPendingRequests();
}

void SpellCheckRequester::invokeRequest(SpellCheckRequest* request) {
  DCHECK(!m_processingRequest);
  m_processingRequest = request;
  client().requestCheckingOfString(m_processingRequest);
}

void SpellCheckRequester::clearProcessingRequest() {
  if (!m_processingRequest)
    return;

  m_processingRequest->dispose();
  m_processingRequest.clear();
}

void SpellCheckRequester::enqueueRequest(SpellCheckRequest* request) {
  DCHECK(request);
  bool continuation = false;
  if (!m_requestQueue.isEmpty()) {
    SpellCheckRequest* lastRequest = m_requestQueue.last();
    // It's a continuation if the number of the last request got incremented in
    // the new one and both apply to the same editable.
    continuation =
        request->rootEditableElement() == lastRequest->rootEditableElement() &&
        request->requestNumber() == lastRequest->requestNumber() + 1;
  }

  // Spellcheck requests for chunks of text in the same element should not
  // overwrite each other.
  if (!continuation) {
    for (auto& requestQueue : m_requestQueue) {
      if (request->rootEditableElement() != requestQueue->rootEditableElement())
        continue;

      requestQueue = request;
      return;
    }
  }

  m_requestQueue.append(request);
}

void SpellCheckRequester::didCheck(int sequence,
                                   const Vector<TextCheckingResult>& results) {
  DCHECK(m_processingRequest);
  DCHECK_EQ(m_processingRequest->data().sequence(), sequence);
  if (m_processingRequest->data().sequence() != sequence) {
    m_requestQueue.clear();
    return;
  }

  frame().spellChecker().markAndReplaceFor(m_processingRequest, results);

  if (m_lastProcessedSequence < sequence)
    m_lastProcessedSequence = sequence;

  clearProcessingRequest();
  if (!m_requestQueue.isEmpty())
    m_timerToProcessQueuedRequest.startOneShot(0, BLINK_FROM_HERE);
}

void SpellCheckRequester::didCheckSucceed(
    int sequence,
    const Vector<TextCheckingResult>& results) {
  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  frame().document()->updateStyleAndLayoutIgnorePendingStylesheets();

  TextCheckingRequestData requestData = m_processingRequest->data();
  if (requestData.sequence() == sequence) {
    DocumentMarker::MarkerTypes markers =
        DocumentMarker::SpellCheckClientMarkers();
    if (m_processingRequest->isValid()) {
      Range* checkingRange = m_processingRequest->checkingRange();
      frame().document()->markers().removeMarkers(EphemeralRange(checkingRange),
                                                  markers);
    }
  }
  didCheck(sequence, results);
}

void SpellCheckRequester::didCheckCancel(int sequence) {
  Vector<TextCheckingResult> results;
  didCheck(sequence, results);
}

DEFINE_TRACE(SpellCheckRequester) {
  visitor->trace(m_frame);
  visitor->trace(m_processingRequest);
  visitor->trace(m_requestQueue);
}

}  // namespace blink
