/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "bindings/core/v8/BindingSecurity.h"

#include "bindings/core/v8/V8Binding.h"
#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Location.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/workers/MainThreadWorkletGlobalScope.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

namespace {

bool canAccessFrameInternal(const LocalDOMWindow* accessingWindow,
                            const SecurityOrigin* targetFrameOrigin,
                            const DOMWindow* targetWindow) {
  SECURITY_CHECK(!(targetWindow && targetWindow->frame()) ||
                 targetWindow == targetWindow->frame()->domWindow());

  // It's important to check that targetWindow is a LocalDOMWindow: it's
  // possible for a remote frame and local frame to have the same security
  // origin, depending on the model being used to allocate Frames between
  // processes. See https://crbug.com/601629.
  if (!(accessingWindow && targetWindow && targetWindow->isLocalDOMWindow()))
    return false;

  const SecurityOrigin* accessingOrigin =
      accessingWindow->document()->getSecurityOrigin();
  if (!accessingOrigin->canAccessCheckSuborigins(targetFrameOrigin))
    return false;

  // Notify the loader's client if the initial document has been accessed.
  LocalFrame* targetFrame = toLocalDOMWindow(targetWindow)->frame();
  if (targetFrame &&
      targetFrame->loader().stateMachine()->isDisplayingInitialEmptyDocument())
    targetFrame->loader().didAccessInitialDocument();

  return true;
}

bool canAccessFrame(const LocalDOMWindow* accessingWindow,
                    const SecurityOrigin* targetFrameOrigin,
                    const DOMWindow* targetWindow,
                    ExceptionState& exceptionState) {
  if (canAccessFrameInternal(accessingWindow, targetFrameOrigin, targetWindow))
    return true;

  if (targetWindow)
    exceptionState.throwSecurityError(
        targetWindow->sanitizedCrossDomainAccessErrorMessage(accessingWindow),
        targetWindow->crossDomainAccessErrorMessage(accessingWindow));
  return false;
}

bool canAccessFrame(const LocalDOMWindow* accessingWindow,
                    SecurityOrigin* targetFrameOrigin,
                    const DOMWindow* targetWindow,
                    BindingSecurity::ErrorReportOption reportingOption) {
  if (canAccessFrameInternal(accessingWindow, targetFrameOrigin, targetWindow))
    return true;

  if (accessingWindow && targetWindow &&
      reportingOption == BindingSecurity::ErrorReportOption::Report)
    accessingWindow->printErrorMessage(
        targetWindow->crossDomainAccessErrorMessage(accessingWindow));
  return false;
}

}  // namespace

bool BindingSecurity::shouldAllowAccessTo(const LocalDOMWindow* accessingWindow,
                                          const DOMWindow* target,
                                          ExceptionState& exceptionState) {
  ASSERT(target);
  const Frame* frame = target->frame();
  if (!frame || !frame->securityContext())
    return false;
  return canAccessFrame(accessingWindow,
                        frame->securityContext()->getSecurityOrigin(), target,
                        exceptionState);
}

bool BindingSecurity::shouldAllowAccessTo(const LocalDOMWindow* accessingWindow,
                                          const DOMWindow* target,
                                          ErrorReportOption reportingOption) {
  ASSERT(target);
  const Frame* frame = target->frame();
  if (!frame || !frame->securityContext())
    return false;
  return canAccessFrame(accessingWindow,
                        frame->securityContext()->getSecurityOrigin(), target,
                        reportingOption);
}

bool BindingSecurity::shouldAllowAccessTo(const LocalDOMWindow* accessingWindow,
                                          const EventTarget* target,
                                          ExceptionState& exceptionState) {
  ASSERT(target);
  const DOMWindow* window = target->toDOMWindow();
  if (!window) {
    // We only need to check the access to Window objects which are
    // cross-origin accessible.  If it's not a Window, the object's
    // origin must always be the same origin (or it already leaked).
    return true;
  }
  const Frame* frame = window->frame();
  if (!frame || !frame->securityContext())
    return false;
  return canAccessFrame(accessingWindow,
                        frame->securityContext()->getSecurityOrigin(), window,
                        exceptionState);
}

bool BindingSecurity::shouldAllowAccessTo(const LocalDOMWindow* accessingWindow,
                                          const Location* target,
                                          ExceptionState& exceptionState) {
  ASSERT(target);
  const Frame* frame = target->frame();
  if (!frame || !frame->securityContext())
    return false;
  return canAccessFrame(accessingWindow,
                        frame->securityContext()->getSecurityOrigin(),
                        frame->domWindow(), exceptionState);
}

bool BindingSecurity::shouldAllowAccessTo(const LocalDOMWindow* accessingWindow,
                                          const Location* target,
                                          ErrorReportOption reportingOption) {
  ASSERT(target);
  const Frame* frame = target->frame();
  if (!frame || !frame->securityContext())
    return false;
  return canAccessFrame(accessingWindow,
                        frame->securityContext()->getSecurityOrigin(),
                        frame->domWindow(), reportingOption);
}

bool BindingSecurity::shouldAllowAccessTo(const LocalDOMWindow* accessingWindow,
                                          const Node* target,
                                          ExceptionState& exceptionState) {
  if (!target)
    return false;
  return canAccessFrame(accessingWindow, target->document().getSecurityOrigin(),
                        target->document().domWindow(), exceptionState);
}

bool BindingSecurity::shouldAllowAccessTo(const LocalDOMWindow* accessingWindow,
                                          const Node* target,
                                          ErrorReportOption reportingOption) {
  if (!target)
    return false;
  return canAccessFrame(accessingWindow, target->document().getSecurityOrigin(),
                        target->document().domWindow(), reportingOption);
}

bool BindingSecurity::shouldAllowAccessToFrame(
    const LocalDOMWindow* accessingWindow,
    const Frame* target,
    ExceptionState& exceptionState) {
  if (!target || !target->securityContext())
    return false;
  return canAccessFrame(accessingWindow,
                        target->securityContext()->getSecurityOrigin(),
                        target->domWindow(), exceptionState);
}

bool BindingSecurity::shouldAllowAccessToFrame(
    const LocalDOMWindow* accessingWindow,
    const Frame* target,
    ErrorReportOption reportingOption) {
  if (!target || !target->securityContext())
    return false;
  return canAccessFrame(accessingWindow,
                        target->securityContext()->getSecurityOrigin(),
                        target->domWindow(), reportingOption);
}

bool BindingSecurity::shouldAllowAccessToDetachedWindow(
    const LocalDOMWindow* accessingWindow,
    const DOMWindow* target,
    ExceptionState& exceptionState) {
  CHECK(target && !target->frame())
      << "This version of shouldAllowAccessToFrame() must be used only for "
      << "detached windows.";
  if (!target->document())
    return false;
  return canAccessFrame(accessingWindow,
                        target->document()->getSecurityOrigin(), target,
                        exceptionState);
}

}  // namespace blink
