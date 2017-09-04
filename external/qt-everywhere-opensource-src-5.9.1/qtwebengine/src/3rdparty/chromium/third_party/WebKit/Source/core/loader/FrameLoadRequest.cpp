// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/FrameLoadRequest.h"

#include "platform/network/ResourceRequest.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/text/AtomicString.h"

namespace blink {

FrameLoadRequest::FrameLoadRequest(Document* originDocument)
    : FrameLoadRequest(originDocument, ResourceRequest()) {}

FrameLoadRequest::FrameLoadRequest(Document* originDocument,
                                   const ResourceRequest& resourceRequest)
    : FrameLoadRequest(originDocument, resourceRequest, AtomicString()) {}

FrameLoadRequest::FrameLoadRequest(Document* originDocument,
                                   const ResourceRequest& resourceRequest,
                                   const AtomicString& frameName)
    : FrameLoadRequest(originDocument,
                       resourceRequest,
                       frameName,
                       CheckContentSecurityPolicy) {}

FrameLoadRequest::FrameLoadRequest(Document* originDocument,
                                   const ResourceRequest& resourceRequest,
                                   const SubstituteData& substituteData)
    : FrameLoadRequest(originDocument,
                       resourceRequest,
                       AtomicString(),
                       substituteData,
                       CheckContentSecurityPolicy) {}

FrameLoadRequest::FrameLoadRequest(
    Document* originDocument,
    const ResourceRequest& resourceRequest,
    const AtomicString& frameName,
    ContentSecurityPolicyDisposition shouldCheckMainWorldContentSecurityPolicy)
    : FrameLoadRequest(originDocument,
                       resourceRequest,
                       frameName,
                       SubstituteData(),
                       shouldCheckMainWorldContentSecurityPolicy) {}

FrameLoadRequest::FrameLoadRequest(
    Document* originDocument,
    const ResourceRequest& resourceRequest,
    const AtomicString& frameName,
    const SubstituteData& substituteData,
    ContentSecurityPolicyDisposition shouldCheckMainWorldContentSecurityPolicy)
    : m_originDocument(originDocument),
      m_resourceRequest(resourceRequest),
      m_frameName(frameName),
      m_substituteData(substituteData),
      m_replacesCurrentItem(false),
      m_clientRedirect(ClientRedirectPolicy::NotClientRedirect),
      m_shouldSendReferrer(MaybeSendReferrer),
      m_shouldSetOpener(MaybeSetOpener),
      m_shouldCheckMainWorldContentSecurityPolicy(
          shouldCheckMainWorldContentSecurityPolicy) {
  // These flags are passed to a service worker which controls the page.
  m_resourceRequest.setFetchRequestMode(
      WebURLRequest::FetchRequestModeNavigate);
  m_resourceRequest.setFetchCredentialsMode(
      WebURLRequest::FetchCredentialsModeInclude);
  m_resourceRequest.setFetchRedirectMode(
      WebURLRequest::FetchRedirectModeManual);

  if (originDocument) {
    m_resourceRequest.setRequestorOrigin(
        SecurityOrigin::create(originDocument->url()));
    return;
  }

  // If we don't have an origin document, and we're going to throw away the
  // response data regardless, set the requestor to a unique origin.
  if (m_substituteData.isValid()) {
    m_resourceRequest.setRequestorOrigin(SecurityOrigin::createUnique());
    return;
  }

  // If we're dealing with a top-level request, use the origin of the requested
  // URL as the initiator.
  // TODO(mkwst): This should be `nullptr`. https://crbug.com/625969
  if (m_resourceRequest.frameType() == WebURLRequest::FrameTypeTopLevel) {
    m_resourceRequest.setRequestorOrigin(
        SecurityOrigin::create(resourceRequest.url()));
    return;
  }
}

}  // namespace blink
