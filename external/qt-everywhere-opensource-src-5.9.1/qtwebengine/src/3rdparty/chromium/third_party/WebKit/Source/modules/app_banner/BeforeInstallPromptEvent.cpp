// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/app_banner/BeforeInstallPromptEvent.h"

#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/UseCounter.h"
#include "modules/app_banner/BeforeInstallPromptEventInit.h"

namespace blink {

BeforeInstallPromptEvent::BeforeInstallPromptEvent(
    const AtomicString& name,
    LocalFrame& frame,
    mojom::blink::AppBannerServicePtr servicePtr,
    mojom::blink::AppBannerEventRequest eventRequest,
    const Vector<String>& platforms)
    : Event(name, false, true),
      m_bannerService(std::move(servicePtr)),
      m_binding(this, std::move(eventRequest)),
      m_platforms(platforms),
      m_userChoice(new UserChoiceProperty(frame.document(),
                                          this,
                                          UserChoiceProperty::UserChoice)),
      m_promptCalled(false) {
  DCHECK(m_bannerService);
  DCHECK(m_binding.is_bound());
  UseCounter::count(&frame, UseCounter::BeforeInstallPromptEvent);
}

BeforeInstallPromptEvent::BeforeInstallPromptEvent(
    const AtomicString& name,
    const BeforeInstallPromptEventInit& init)
    : Event(name, false, true), m_binding(this), m_promptCalled(false) {
  if (init.hasPlatforms())
    m_platforms = init.platforms();
}

BeforeInstallPromptEvent::~BeforeInstallPromptEvent() {}

void BeforeInstallPromptEvent::dispose() {
  m_bannerService.reset();
  m_binding.Close();
}

Vector<String> BeforeInstallPromptEvent::platforms() const {
  return m_platforms;
}

ScriptPromise BeforeInstallPromptEvent::userChoice(ScriptState* scriptState) {
  UseCounter::count(scriptState->getExecutionContext(),
                    UseCounter::BeforeInstallPromptEventUserChoice);
  // |m_binding| must be bound to allow the AppBannerService to resolve the
  // userChoice promise.
  if (m_userChoice && m_binding.is_bound())
    return m_userChoice->promise(scriptState->world());
  return ScriptPromise::rejectWithDOMException(
      scriptState,
      DOMException::create(InvalidStateError,
                           "userChoice cannot be accessed on this event."));
}

ScriptPromise BeforeInstallPromptEvent::prompt(ScriptState* scriptState) {
  // |m_bannerService| must be bound to allow us to inform the AppBannerService
  // to display the banner now.
  if (!defaultPrevented() || m_promptCalled || !m_bannerService.is_bound()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(InvalidStateError,
                             "The prompt() method may only be called once, "
                             "following preventDefault()."));
  }

  UseCounter::count(scriptState->getExecutionContext(),
                    UseCounter::BeforeInstallPromptEventPrompt);

  m_promptCalled = true;
  m_bannerService->DisplayAppBanner();
  return ScriptPromise::castUndefined(scriptState);
}

const AtomicString& BeforeInstallPromptEvent::interfaceName() const {
  return EventNames::BeforeInstallPromptEvent;
}

void BeforeInstallPromptEvent::preventDefault() {
  Event::preventDefault();
  if (target()) {
    UseCounter::count(target()->getExecutionContext(),
                      UseCounter::BeforeInstallPromptEventPreventDefault);
  }
}

void BeforeInstallPromptEvent::BannerAccepted(const String& platform) {
  m_userChoice->resolve(AppBannerPromptResult::create(
      platform, AppBannerPromptResult::Outcome::Accepted));
}

void BeforeInstallPromptEvent::BannerDismissed() {
  m_userChoice->resolve(AppBannerPromptResult::create(
      emptyAtom, AppBannerPromptResult::Outcome::Dismissed));
}

DEFINE_TRACE(BeforeInstallPromptEvent) {
  visitor->trace(m_userChoice);
  Event::trace(visitor);
}

}  // namespace blink
