// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/loader/BeaconLoader.h"

#include "core/FetchInitiatorTypeNames.h"
#include "core/dom/Document.h"
#include "core/fetch/CrossOriginAccessControl.h"
#include "core/fetch/FetchContext.h"
#include "core/fileapi/File.h"
#include "core/frame/LocalFrame.h"
#include "core/html/DOMFormData.h"
#include "platform/network/FormData.h"
#include "platform/network/ParsedContentType.h"
#include "platform/network/ResourceRequest.h"
#include "wtf/ArrayBufferView.h"

namespace WebCore {

void BeaconLoader::prepareRequest(LocalFrame* frame, ResourceRequest& request)
{
    // NOTE: do not distinguish Beacon by target type.
    request.setTargetType(ResourceRequest::TargetIsPing);
    request.setHTTPMethod("POST");
    request.setHTTPHeaderField("Cache-Control", "max-age=0");
    request.setAllowStoredCredentials(true);
    frame->loader().fetchContext().addAdditionalRequestHeaders(frame->document(), request, FetchSubresource);
    frame->loader().fetchContext().setFirstPartyForCookies(request);
}

void BeaconLoader::issueRequest(LocalFrame* frame, ResourceRequest& request)
{
    FetchInitiatorInfo initiatorInfo;
    initiatorInfo.name = FetchInitiatorTypeNames::beacon;

    PingLoader::start(frame, request, initiatorInfo);
}

bool BeaconLoader::sendBeacon(LocalFrame* frame, int allowance, const KURL& beaconURL, const String& data, int& payloadLength)
{
    ResourceRequest request(beaconURL);
    prepareRequest(frame, request);

    RefPtr<FormData> entityBody = FormData::create(data.utf8());
    unsigned long long entitySize = entityBody->sizeInBytes();
    if (allowance > 0 && static_cast<unsigned>(allowance) < entitySize)
        return false;

    request.setHTTPBody(entityBody);
    request.setHTTPContentType("text/plain;charset=UTF-8");

    issueRequest(frame, request);
    payloadLength = entitySize;
    return true;
}

bool BeaconLoader::sendBeacon(LocalFrame* frame, int allowance, const KURL& beaconURL, PassRefPtr<ArrayBufferView>& data, int& payloadLength)
{
    ASSERT(data);
    unsigned long long entitySize = data->byteLength();
    if (allowance > 0 && static_cast<unsigned long long>(allowance) < entitySize)
        return false;

    ResourceRequest request(beaconURL);
    prepareRequest(frame, request);

    RefPtr<FormData> entityBody = FormData::create(data->baseAddress(), data->byteLength());
    request.setHTTPBody(entityBody.release());

    // FIXME: a reasonable choice, but not in the spec; should it give a default?
    AtomicString contentType = AtomicString("application/octet-stream");
    request.setHTTPContentType(contentType);

    issueRequest(frame, request);
    payloadLength = entitySize;
    return true;
}

bool BeaconLoader::sendBeacon(LocalFrame* frame, int allowance, const KURL& beaconURL, PassRefPtrWillBeRawPtr<Blob>& data, int& payloadLength)
{
    ASSERT(data);
    unsigned long long entitySize = data->size();
    if (allowance > 0 && static_cast<unsigned long long>(allowance) < entitySize)
        return false;

    ResourceRequest request(beaconURL);
    prepareRequest(frame, request);

    RefPtr<FormData> entityBody = FormData::create();
    if (data->hasBackingFile())
        entityBody->appendFile(toFile(data.get())->path());
    else
        entityBody->appendBlob(data->uuid(), data->blobDataHandle());

    request.setHTTPBody(entityBody.release());

    AtomicString contentType;
    const String& blobType = data->type();
    if (!blobType.isEmpty() && isValidContentType(blobType))
        request.setHTTPContentType(AtomicString(contentType));

    issueRequest(frame, request);
    payloadLength = entitySize;
    return true;
}

bool BeaconLoader::sendBeacon(LocalFrame* frame, int allowance, const KURL& beaconURL, PassRefPtrWillBeRawPtr<DOMFormData>& data, int& payloadLength)
{
    ASSERT(data);
    ResourceRequest request(beaconURL);
    prepareRequest(frame, request);

    RefPtr<FormData> entityBody = data->createMultiPartFormData();

    unsigned long long entitySize = entityBody->sizeInBytes();
    if (allowance > 0 && static_cast<unsigned long long>(allowance) < entitySize)
        return false;

    AtomicString contentType = AtomicString("multipart/form-data; boundary=", AtomicString::ConstructFromLiteral) + entityBody->boundary().data();
    request.setHTTPBody(entityBody.release());
    request.setHTTPContentType(contentType);

    issueRequest(frame, request);
    payloadLength = entitySize;
    return true;
}

} // namespace WebCore
