// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PreloadRequest_h
#define PreloadRequest_h

#include "core/fetch/ClientHintsPreferences.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/IntegrityMetadata.h"
#include "core/fetch/Resource.h"
#include "platform/CrossOriginAttributeValue.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "wtf/Allocator.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/TextPosition.h"
#include <memory>

namespace blink {

class Document;

class PreloadRequest {
    USING_FAST_MALLOC(PreloadRequest);
public:
    enum RequestType { RequestTypePreload, RequestTypePreconnect, RequestTypeLinkRelPreload };

    static std::unique_ptr<PreloadRequest> create(const String& initiatorName, const TextPosition& initiatorPosition, const String& resourceURL, const KURL& baseURL, Resource::Type resourceType, const ReferrerPolicy referrerPolicy, const FetchRequest::ResourceWidth& resourceWidth = FetchRequest::ResourceWidth(), const ClientHintsPreferences& clientHintsPreferences = ClientHintsPreferences(), RequestType requestType = RequestTypePreload)
    {
        return wrapUnique(new PreloadRequest(initiatorName, initiatorPosition, resourceURL, baseURL, resourceType, resourceWidth, clientHintsPreferences, requestType, referrerPolicy));
    }

    bool isSafeToSendToAnotherThread() const;

    FetchRequest resourceRequest(Document*);

    const String& charset() const { return m_charset; }
    double discoveryTime() const { return m_discoveryTime; }
    void setDefer(FetchRequest::DeferOption defer) { m_defer = defer; }
    void setCharset(const String& charset) { m_charset = charset.isolatedCopy(); }
    void setCrossOrigin(CrossOriginAttributeValue crossOrigin)
    {
        m_crossOrigin = crossOrigin;
    }
    CrossOriginAttributeValue crossOrigin() const
    {
        return m_crossOrigin;
    }

    void setNonce(const String& nonce) { m_nonce = nonce.isolatedCopy(); }
    const String& nonce() const { return m_nonce; }

    Resource::Type resourceType() const { return m_resourceType; }

    const String& resourceURL() const { return m_resourceURL; }
    float resourceWidth() const { return m_resourceWidth.isSet ? m_resourceWidth.width : 0; }
    const KURL& baseURL() const { return m_baseURL; }
    bool isPreconnect() const { return m_requestType == RequestTypePreconnect; }
    const ClientHintsPreferences& preferences() const { return m_clientHintsPreferences; }
    ReferrerPolicy getReferrerPolicy() const { return m_referrerPolicy; }
    void setIntegrityMetadata(const IntegrityMetadataSet& metadataSet)
    {
        m_integrityMetadata = metadataSet;
    }
    const IntegrityMetadataSet& integrityMetadata() const
    {
        return m_integrityMetadata;
    }

private:
    PreloadRequest(const String& initiatorName,
        const TextPosition& initiatorPosition,
        const String& resourceURL,
        const KURL& baseURL,
        Resource::Type resourceType,
        const FetchRequest::ResourceWidth& resourceWidth,
        const ClientHintsPreferences& clientHintsPreferences,
        RequestType requestType,
        const ReferrerPolicy referrerPolicy)
        : m_initiatorName(initiatorName)
        , m_initiatorPosition(initiatorPosition)
        , m_resourceURL(resourceURL.isolatedCopy())
        , m_baseURL(baseURL.copy())
        , m_resourceType(resourceType)
        , m_crossOrigin(CrossOriginAttributeNotSet)
        , m_discoveryTime(monotonicallyIncreasingTime())
        , m_defer(FetchRequest::NoDefer)
        , m_resourceWidth(resourceWidth)
        , m_clientHintsPreferences(clientHintsPreferences)
        , m_requestType(requestType)
        , m_referrerPolicy(referrerPolicy)
    {
    }

    KURL completeURL(Document*);

    String m_initiatorName;
    TextPosition m_initiatorPosition;
    String m_resourceURL;
    KURL m_baseURL;
    String m_charset;
    Resource::Type m_resourceType;
    CrossOriginAttributeValue m_crossOrigin;
    String m_nonce;
    double m_discoveryTime;
    FetchRequest::DeferOption m_defer;
    FetchRequest::ResourceWidth m_resourceWidth;
    ClientHintsPreferences m_clientHintsPreferences;
    RequestType m_requestType;
    ReferrerPolicy m_referrerPolicy;
    IntegrityMetadataSet m_integrityMetadata;
};

typedef Vector<std::unique_ptr<PreloadRequest>> PreloadRequestStream;

} // namespace blink

#endif
