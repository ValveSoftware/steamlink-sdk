// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/remoteplayback/HTMLMediaElementRemotePlayback.h"

#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/QualifiedName.h"
#include "core/html/HTMLMediaElement.h"
#include "modules/remoteplayback/RemotePlayback.h"

namespace blink {

// static
bool HTMLMediaElementRemotePlayback::fastHasAttribute(
    const QualifiedName& name,
    const HTMLMediaElement& element) {
  ASSERT(name == HTMLNames::disableremoteplaybackAttr);
  return element.fastHasAttribute(name);
}

// static
void HTMLMediaElementRemotePlayback::setBooleanAttribute(
    const QualifiedName& name,
    HTMLMediaElement& element,
    bool value) {
  ASSERT(name == HTMLNames::disableremoteplaybackAttr);
  element.setBooleanAttribute(name, value);

  HTMLMediaElementRemotePlayback& self =
      HTMLMediaElementRemotePlayback::from(element);
  if (self.m_remote && value)
    self.m_remote->remotePlaybackDisabled();
}

// static
HTMLMediaElementRemotePlayback& HTMLMediaElementRemotePlayback::from(
    HTMLMediaElement& element) {
  HTMLMediaElementRemotePlayback* supplement =
      static_cast<HTMLMediaElementRemotePlayback*>(
          Supplement<HTMLMediaElement>::from(element, supplementName()));
  if (!supplement) {
    supplement = new HTMLMediaElementRemotePlayback();
    provideTo(element, supplementName(), supplement);
  }
  return *supplement;
}

// static
RemotePlayback* HTMLMediaElementRemotePlayback::remote(
    HTMLMediaElement& element) {
  HTMLMediaElementRemotePlayback& self =
      HTMLMediaElementRemotePlayback::from(element);
  Document& document = element.document();
  if (!document.frame())
    return nullptr;

  if (!self.m_remote)
    self.m_remote = RemotePlayback::create(element);

  return self.m_remote;
}

// static
const char* HTMLMediaElementRemotePlayback::supplementName() {
  return "HTMLMediaElementRemotePlayback";
}

DEFINE_TRACE(HTMLMediaElementRemotePlayback) {
  visitor->trace(m_remote);
  Supplement<HTMLMediaElement>::trace(visitor);
}

}  // namespace blink
