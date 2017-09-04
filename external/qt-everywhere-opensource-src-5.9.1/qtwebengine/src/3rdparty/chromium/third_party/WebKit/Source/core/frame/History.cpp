/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#include "core/frame/History.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/HistoryItem.h"
#include "core/loader/NavigationScheduler.h"
#include "core/page/Page.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/text/StringView.h"

namespace blink {

namespace {

bool equalIgnoringPathQueryAndFragment(const KURL& a, const KURL& b) {
  return StringView(a.getString(), 0, a.pathStart()) ==
         StringView(b.getString(), 0, b.pathStart());
}

bool equalIgnoringQueryAndFragment(const KURL& a, const KURL& b) {
  return StringView(a.getString(), 0, a.pathEnd()) ==
         StringView(b.getString(), 0, b.pathEnd());
}

}  // namespace

History::History(LocalFrame* frame)
    : DOMWindowProperty(frame), m_lastStateObjectRequested(nullptr) {}

DEFINE_TRACE(History) {
  DOMWindowProperty::trace(visitor);
}

unsigned History::length() const {
  if (!frame() || !frame()->loader().client())
    return 0;
  return frame()->loader().client()->backForwardLength();
}

SerializedScriptValue* History::state() {
  m_lastStateObjectRequested = stateInternal();
  return m_lastStateObjectRequested.get();
}

SerializedScriptValue* History::stateInternal() const {
  if (!frame())
    return 0;

  if (HistoryItem* historyItem = frame()->loader().currentItem())
    return historyItem->stateObject();

  return 0;
}

void History::setScrollRestoration(const String& value) {
  ASSERT(value == "manual" || value == "auto");
  if (!frame() || !frame()->loader().client())
    return;

  HistoryScrollRestorationType scrollRestoration =
      value == "manual" ? ScrollRestorationManual : ScrollRestorationAuto;
  if (scrollRestoration == scrollRestorationInternal())
    return;

  if (HistoryItem* historyItem = frame()->loader().currentItem()) {
    historyItem->setScrollRestorationType(scrollRestoration);
    frame()->loader().client()->didUpdateCurrentHistoryItem();
  }
}

String History::scrollRestoration() {
  return scrollRestorationInternal() == ScrollRestorationManual ? "manual"
                                                                : "auto";
}

HistoryScrollRestorationType History::scrollRestorationInternal() const {
  if (frame()) {
    if (HistoryItem* historyItem = frame()->loader().currentItem())
      return historyItem->scrollRestorationType();
  }

  return ScrollRestorationAuto;
}

bool History::stateChanged() const {
  return m_lastStateObjectRequested != stateInternal();
}

bool History::isSameAsCurrentState(SerializedScriptValue* state) const {
  return state == stateInternal();
}

void History::back(ExecutionContext* context) {
  go(context, -1);
}

void History::forward(ExecutionContext* context) {
  go(context, 1);
}

void History::go(ExecutionContext* context, int delta) {
  if (!frame() || !frame()->loader().client())
    return;

  ASSERT(isMainThread());
  Document* activeDocument = toDocument(context);
  if (!activeDocument)
    return;

  if (!activeDocument->frame() ||
      !activeDocument->frame()->canNavigate(*frame()) ||
      !activeDocument->frame()->isNavigationAllowed() ||
      !NavigationDisablerForBeforeUnload::isNavigationAllowed()) {
    return;
  }

  // We intentionally call reload() for the current frame if delta is zero.
  // Otherwise, navigation happens on the root frame.
  // This behavior is designed in the following spec.
  // https://html.spec.whatwg.org/multipage/browsers.html#dom-history-go
  if (delta)
    frame()->loader().client()->navigateBackForward(delta);
  else
    frame()->reload(FrameLoadTypeReload, ClientRedirectPolicy::ClientRedirect);
}

KURL History::urlForState(const String& urlString) {
  Document* document = frame()->document();

  if (urlString.isNull())
    return document->url();
  if (urlString.isEmpty())
    return document->baseURL();

  return KURL(document->baseURL(), urlString);
}

bool History::canChangeToUrl(const KURL& url,
                             SecurityOrigin* documentOrigin,
                             const KURL& documentURL) {
  if (!url.isValid())
    return false;

  if (documentOrigin->isGrantedUniversalAccess())
    return true;

  // We allow sandboxed documents, `data:`/`file:` URLs, etc. to use
  // 'pushState'/'replaceState' to modify the URL fragment: see
  // https://crbug.com/528681 for the compatibility concerns.
  if (documentOrigin->isUnique() || documentOrigin->isLocal())
    return equalIgnoringQueryAndFragment(url, documentURL);

  if (!equalIgnoringPathQueryAndFragment(url, documentURL))
    return false;

  RefPtr<SecurityOrigin> requestedOrigin = SecurityOrigin::create(url);
  if (requestedOrigin->isUnique() ||
      !requestedOrigin->isSameSchemeHostPort(documentOrigin))
    return false;

  return true;
}

void History::stateObjectAdded(PassRefPtr<SerializedScriptValue> data,
                               const String& /* title */,
                               const String& urlString,
                               HistoryScrollRestorationType restorationType,
                               FrameLoadType type,
                               ExceptionState& exceptionState) {
  if (!frame() || !frame()->page() || !frame()->loader().documentLoader())
    return;

  KURL fullURL = urlForState(urlString);
  if (!canChangeToUrl(fullURL, frame()->document()->getSecurityOrigin(),
                      frame()->document()->url())) {
    // We can safely expose the URL to JavaScript, as a) no redirection takes
    // place: JavaScript already had this URL, b) JavaScript can only access a
    // same-origin History object.
    exceptionState.throwSecurityError(
        "A history state object with URL '" + fullURL.elidedString() +
        "' cannot be created in a document with origin '" +
        frame()->document()->getSecurityOrigin()->toString() + "' and URL '" +
        frame()->document()->url().elidedString() + "'.");
    return;
  }

  frame()->loader().updateForSameDocumentNavigation(
      fullURL, SameDocumentNavigationHistoryApi, std::move(data),
      restorationType, type, frame()->document());
}

}  // namespace blink
