// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ElementVisibilityObserver.h"

#include "core/dom/Element.h"
#include "core/dom/IntersectionObserverEntry.h"
#include "core/frame/LocalFrame.h"
#include "wtf/Functional.h"

namespace blink {

namespace {

bool isInRemoteFrame(const Document& document) {
  DCHECK(document.frame());
  Frame* mainFrame = document.frame()->tree().top();
  return !mainFrame || mainFrame->isRemoteFrame();
}

}  // anonymous namespace

ElementVisibilityObserver::ElementVisibilityObserver(
    Element* element,
    std::unique_ptr<VisibilityCallback> callback)
    : m_element(element), m_callback(std::move(callback)) {}

ElementVisibilityObserver::~ElementVisibilityObserver() = default;

void ElementVisibilityObserver::start() {
  ExecutionContext* context = m_element->getExecutionContext();
  DCHECK(context->isDocument());
  Document& document = toDocument(*context);

  // TODO(zqzhang): IntersectionObserver does not work for RemoteFrame.
  // Remove this early return when it's fixed. See https://crbug.com/615156
  if (isInRemoteFrame(document)) {
    m_element.release();
    return;
  }

  DCHECK(!m_intersectionObserver);
  m_intersectionObserver = IntersectionObserver::create(
      Vector<Length>(), Vector<float>({std::numeric_limits<float>::min()}),
      &document, WTF::bind(&ElementVisibilityObserver::onVisibilityChanged,
                           wrapWeakPersistent(this)));
  DCHECK(m_intersectionObserver);
  m_intersectionObserver->setInitialState(
      IntersectionObserver::InitialState::kAuto);
  m_intersectionObserver->observe(m_element.release());
}

void ElementVisibilityObserver::stop() {
  // TODO(zqzhang): IntersectionObserver does not work for RemoteFrame,
  // so |m_intersectionObserver| may be null at this point after start().
  // Replace this early return with DCHECK when this has been fixed. See
  // https://crbug.com/615156
  if (!m_intersectionObserver)
    return;

  m_intersectionObserver->disconnect();
  m_intersectionObserver = nullptr;
}

void ElementVisibilityObserver::deliverObservationsForTesting() {
  m_intersectionObserver->deliver();
}

DEFINE_TRACE(ElementVisibilityObserver) {
  visitor->trace(m_element);
  visitor->trace(m_intersectionObserver);
}

void ElementVisibilityObserver::onVisibilityChanged(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  bool isVisible = entries.last()->intersectionRatio() > 0.f;
  (*m_callback.get())(isVisible);
}

}  // namespace blink
