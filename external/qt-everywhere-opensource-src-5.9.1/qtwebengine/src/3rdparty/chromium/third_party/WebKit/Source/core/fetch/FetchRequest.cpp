/*
 * Copyright (C) 2012 Google, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
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

#include "core/fetch/FetchRequest.h"

#include "core/fetch/CrossOriginAccessControl.h"
#include "core/fetch/ResourceFetcher.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/Suborigin.h"

namespace blink {

FetchRequest::FetchRequest(const ResourceRequest& resourceRequest,
                           const AtomicString& initiator,
                           const String& charset)
    : m_resourceRequest(resourceRequest),
      m_charset(charset),
      m_options(ResourceFetcher::defaultResourceOptions()),
      m_forPreload(false),
      m_linkPreload(false),
      m_preloadDiscoveryTime(0.0),
      m_defer(NoDefer),
      m_originRestriction(UseDefaultOriginRestrictionForType),
      m_placeholderImageRequestType(DisallowPlaceholder) {
  m_options.initiatorInfo.name = initiator;
}

FetchRequest::FetchRequest(const ResourceRequest& resourceRequest,
                           const AtomicString& initiator,
                           const ResourceLoaderOptions& options)
    : m_resourceRequest(resourceRequest),
      m_options(options),
      m_forPreload(false),
      m_linkPreload(false),
      m_preloadDiscoveryTime(0.0),
      m_defer(NoDefer),
      m_originRestriction(UseDefaultOriginRestrictionForType),
      m_placeholderImageRequestType(
          PlaceholderImageRequestType::DisallowPlaceholder) {
  m_options.initiatorInfo.name = initiator;
}

FetchRequest::FetchRequest(const ResourceRequest& resourceRequest,
                           const FetchInitiatorInfo& initiator)
    : m_resourceRequest(resourceRequest),
      m_options(ResourceFetcher::defaultResourceOptions()),
      m_forPreload(false),
      m_linkPreload(false),
      m_preloadDiscoveryTime(0.0),
      m_defer(NoDefer),
      m_originRestriction(UseDefaultOriginRestrictionForType),
      m_placeholderImageRequestType(
          PlaceholderImageRequestType::DisallowPlaceholder) {
  m_options.initiatorInfo = initiator;
}

FetchRequest::~FetchRequest() {}

void FetchRequest::setCrossOriginAccessControl(
    SecurityOrigin* origin,
    CrossOriginAttributeValue crossOrigin) {
  DCHECK_NE(crossOrigin, CrossOriginAttributeNotSet);
  // Per https://w3c.github.io/webappsec-suborigins/#security-model-opt-outs,
  // credentials are forced when credentials mode is "same-origin", the
  // 'unsafe-credentials' option is set, and the request's physical origin is
  // the same as the URL's.
  const bool suboriginPolicyForcesCredentials =
      origin->hasSuborigin() &&
      origin->suborigin()->policyContains(
          Suborigin::SuboriginPolicyOptions::UnsafeCredentials) &&
      SecurityOrigin::create(url())->isSameSchemeHostPort(origin);
  const bool useCredentials =
      crossOrigin == CrossOriginAttributeUseCredentials ||
      suboriginPolicyForcesCredentials;
  const bool isSameOriginRequest =
      origin && origin->canRequestNoSuborigin(m_resourceRequest.url());

  // Currently FetchRequestMode and FetchCredentialsMode are only used when the
  // request goes to Service Worker.
  m_resourceRequest.setFetchRequestMode(WebURLRequest::FetchRequestModeCORS);
  m_resourceRequest.setFetchCredentialsMode(
      useCredentials ? WebURLRequest::FetchCredentialsModeInclude
                     : WebURLRequest::FetchCredentialsModeSameOrigin);

  if (isSameOriginRequest || useCredentials) {
    m_options.allowCredentials = AllowStoredCredentials;
    m_resourceRequest.setAllowStoredCredentials(true);
  } else {
    m_options.allowCredentials = DoNotAllowStoredCredentials;
    m_resourceRequest.setAllowStoredCredentials(false);
  }
  m_options.corsEnabled = IsCORSEnabled;
  m_options.securityOrigin = origin;
  m_options.credentialsRequested = useCredentials
                                       ? ClientRequestedCredentials
                                       : ClientDidNotRequestCredentials;

  // TODO: Credentials should be removed only when the request is cross origin.
  m_resourceRequest.removeCredentials();

  if (origin)
    m_resourceRequest.setHTTPOrigin(origin);
}

void FetchRequest::setResourceWidth(ResourceWidth resourceWidth) {
  if (resourceWidth.isSet) {
    m_resourceWidth.width = resourceWidth.width;
    m_resourceWidth.isSet = true;
  }
}

void FetchRequest::setForPreload(bool forPreload, double discoveryTime) {
  m_forPreload = forPreload;
  m_preloadDiscoveryTime = discoveryTime;
}

void FetchRequest::makeSynchronous() {
  // Synchronous requests should always be max priority, lest they hang the
  // renderer.
  m_resourceRequest.setPriority(ResourceLoadPriorityHighest);
  m_resourceRequest.setTimeoutInterval(10);
  m_options.synchronousPolicy = RequestSynchronously;
}

void FetchRequest::setAllowImagePlaceholder() {
  DCHECK_EQ(DisallowPlaceholder, m_placeholderImageRequestType);
  if (!m_resourceRequest.url().protocolIsInHTTPFamily() ||
      m_resourceRequest.httpMethod() != "GET" ||
      !m_resourceRequest.httpHeaderField("range").isNull()) {
    return;
  }

  m_placeholderImageRequestType = AllowPlaceholder;

  // Fetch the first few bytes of the image. This number is tuned to both (a)
  // likely capture the entire image for small images and (b) likely contain
  // the dimensions for larger images.
  // TODO(sclittle): Calculate the optimal value for this number.
  m_resourceRequest.setHTTPHeaderField("range", "bytes=0-2047");

  // TODO(sclittle): Indicate somehow (e.g. through a new request bit) to the
  // embedder that it should return the full resource if the entire resource is
  // fresh in the cache.
}

}  // namespace blink
