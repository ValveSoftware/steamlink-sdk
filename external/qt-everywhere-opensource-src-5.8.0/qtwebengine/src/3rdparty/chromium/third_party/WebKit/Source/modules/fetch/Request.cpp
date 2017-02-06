// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/Request.h"

#include "bindings/core/v8/Dictionary.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/fetch/FetchUtils.h"
#include "core/fetch/ResourceLoaderOptions.h"
#include "core/loader/ThreadableLoader.h"
#include "modules/fetch/BodyStreamBuffer.h"
#include "modules/fetch/DataConsumerHandleUtil.h"
#include "modules/fetch/FetchBlobDataConsumerHandle.h"
#include "modules/fetch/FetchManager.h"
#include "modules/fetch/RequestInit.h"
#include "platform/HTTPNames.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/network/HTTPParsers.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/OriginAccessEntry.h"
#include "platform/weborigin/Referrer.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRequest.h"

namespace blink {

FetchRequestData* createCopyOfFetchRequestDataForFetch(ScriptState* scriptState, const FetchRequestData* original)
{
    FetchRequestData* request = FetchRequestData::create();
    request->setURL(original->url());
    request->setMethod(original->method());
    request->setHeaderList(original->headerList()->clone());
    request->setUnsafeRequestFlag(true);
    // FIXME: Set client.
    DOMWrapperWorld& world = scriptState->world();
    if (world.isIsolatedWorld())
        request->setOrigin(world.isolatedWorldSecurityOrigin());
    else
        request->setOrigin(scriptState->getExecutionContext()->getSecurityOrigin());
    // FIXME: Set ForceOriginHeaderFlag.
    request->setSameOriginDataURLFlag(true);
    request->setReferrer(original->referrer());
    request->setMode(original->mode());
    request->setCredentials(original->credentials());
    request->setAttachedCredential(original->attachedCredential());
    request->setRedirect(original->redirect());
    request->setIntegrity(original->integrity());
    // FIXME: Set cache mode.
    // TODO(yhirano): Set redirect mode.
    return request;
}

Request* Request::createRequestWithRequestOrString(ScriptState* scriptState, Request* inputRequest, const String& inputString, RequestInit& init, ExceptionState& exceptionState)
{
    // - "If |input| is a Request object and it is disturbed, throw a
    //   TypeError."
    if (inputRequest && inputRequest->bodyUsed()) {
        exceptionState.throwTypeError("Cannot construct a Request with a Request object that has already been used.");
        return nullptr;
    }
    // - "Let |temporaryBody| be |input|'s request's body if |input| is a
    //   Request object, and null otherwise."
    BodyStreamBuffer* temporaryBody = inputRequest ? inputRequest->bodyBuffer() : nullptr;

    // "Let |request| be |input|'s request, if |input| is a Request object,
    // and a new request otherwise."

    RefPtr<SecurityOrigin> origin = scriptState->getExecutionContext()->getSecurityOrigin();

    // TODO(yhirano): Implement the following steps:
    // - "Let |window| be client."
    // - "If |request|'s window is an environment settings object and its
    //   origin is same origin with entry settings object's origin, set
    //   |window| to |request|'s window."
    // - "If |init|'s window member is present and it is not null, throw a
    //   TypeError."
    // - "If |init|'s window member is present, set |window| to no-window."
    //
    // "Set |request| to a new request whose url is |request|'s current url,
    // method is |request|'s method, header list is a copy of |request|'s
    // header list, unsafe-request flag is set, client is entry settings object,
    // window is |window|, origin is "client", omit-Origin-header flag is
    // |request|'s omit-Origin-header flag, same-origin data-URL flag is set,
    // referrer is |request|'s referrer, referrer policy is |request|'s
    // referrer policy, destination is the empty string, mode is |request|'s
    // mode, credentials mode is |request|'s credentials mode, cache mode is
    // |request|'s cache mode, redirect mode is |request|'s redirect mode, and
    // integrity metadata is |request|'s integrity metadata."
    FetchRequestData* request = createCopyOfFetchRequestDataForFetch(scriptState, inputRequest ? inputRequest->request() : FetchRequestData::create());

    // We don't use fallback values. We set these flags directly in below.
    // - "Let |fallbackMode| be null."
    // - "Let |fallbackCredentials| be null."
    // - "Let |baseURL| be entry settings object's API base URL."

    // "If |input| is a string, run these substeps:"
    if (!inputRequest) {
        // "Let |parsedURL| be the result of parsing |input| with |baseURL|."
        KURL parsedURL = scriptState->getExecutionContext()->completeURL(inputString);
        // "If |parsedURL| is failure, throw a TypeError."
        if (!parsedURL.isValid()) {
            exceptionState.throwTypeError("Failed to parse URL from " + inputString);
            return nullptr;
        }
        //   "If |parsedURL| includes credentials, throw a TypeError."
        if (!parsedURL.user().isEmpty() || !parsedURL.pass().isEmpty()) {
            exceptionState.throwTypeError("Request cannot be constructed from a URL that includes credentials: " + inputString);
            return nullptr;
        }
        // "Set |request|'s url to |parsedURL| and replace |request|'s url list
        // single URL with a copy of |parsedURL|."
        request->setURL(parsedURL);

        // We don't use fallback values. We set these flags directly in below.
        // - "Set |fallbackMode| to "cors"."
        // - "Set |fallbackCredentials| to "omit"."
    }

    // "If any of |init|'s members are present, run these substeps:"
    if (init.areAnyMembersSet) {
        // "If |request|'s |mode| is "navigate", throw a TypeError."
        if (request->mode() == WebURLRequest::FetchRequestModeNavigate) {
            exceptionState.throwTypeError("Cannot construct a Request with a Request whose mode is 'navigate' and a non-empty RequestInit.");
            return nullptr;
        }

        // TODO(yhirano): Implement the following substep:
        //   "Unset |request|'s omit-Origin-header flag."

        // The substep "Set |request|'s referrer to "client"." is performed by
        // the code below as follows:
        // - |init.referrer.referrer| gets initialized by the RequestInit
        //   constructor to "about:client" when any of |options|'s members are
        //   present.
        // - The code below does the equivalent as the step specified in the
        //   spec by processing the "about:client".

        // The substep "Set |request|'s referrer policy to the empty string."
        // is also performed by the code below similarly.
    }

    // The following if-clause performs the following two steps:
    // - "If |init|'s referrer member is present, run these substeps:"
    //   "If |init|'s referrerPolicy member is present, set |request|'s
    //    referrer policy to it."
    //
    // The condition "if any of |init|'s members are present"
    // (areAnyMembersSet) is used for the if-clause instead of conditions
    // indicating presence of each member as specified in the spec. This is to
    // perform the substeps in the previous step together here.
    if (init.areAnyMembersSet) {
        // Nothing to do for the step "Let |referrer| be |init|'s referrer
        // member."

        if (init.referrer.referrer.isEmpty()) {
            // "If |referrer| is the empty string, set |request|'s referrer to
            // "no-referrer" and terminate these substeps."
            request->setReferrerString(AtomicString(Referrer::noReferrer()));
        } else {
            // "Let |parsedReferrer| be the result of parsing |referrer| with
            // |baseURL|."
            KURL parsedReferrer = scriptState->getExecutionContext()->completeURL(init.referrer.referrer);
            if (!parsedReferrer.isValid()) {
                // "If |parsedReferrer| is failure, throw a TypeError."
                exceptionState.throwTypeError("Referrer '" + init.referrer.referrer + "' is not a valid URL.");
                return nullptr;
            }
            if (parsedReferrer.protocolIsAbout() && parsedReferrer.host().isEmpty() && parsedReferrer.path() == "client") {
                // "If |parsedReferrer|'s non-relative flag is set, scheme is
                // "about", and path contains a single string "client", set
                // request's referrer to "client" and terminate these
                // substeps."
                request->setReferrerString(FetchRequestData::clientReferrerString());
            } else if (!origin->isSameSchemeHostPortAndSuborigin(SecurityOrigin::create(parsedReferrer).get())) {
                // "If |parsedReferrer|'s origin is not same origin with
                // |origin|, throw a TypeError."
                exceptionState.throwTypeError("The origin of '" + init.referrer.referrer + "' should be same as '" + origin->toString() + "'");
                return nullptr;
            } else {
                // "Set |request|'s referrer to |parsedReferrer|."
                request->setReferrerString(AtomicString(parsedReferrer.getString()));
            }
        }
        request->setReferrerPolicy(init.referrer.referrerPolicy);
    }

    // The following code performs the following steps:
    // - "Let |mode| be |init|'s mode member if it is present, and
    //   |fallbackMode| otherwise."
    // - "If |mode| is "navigate", throw a TypeError."
    // - "If |mode| is non-null, set |request|'s mode to |mode|."
    if (init.mode == "navigate") {
        exceptionState.throwTypeError("Cannot construct a Request with a RequestInit whose mode member is set as 'navigate'.");
        return nullptr;
    }
    if (init.mode == "same-origin") {
        request->setMode(WebURLRequest::FetchRequestModeSameOrigin);
    } else if (init.mode == "no-cors") {
        request->setMode(WebURLRequest::FetchRequestModeNoCORS);
    } else if (init.mode == "cors") {
        request->setMode(WebURLRequest::FetchRequestModeCORS);
    } else {
        // |inputRequest| is directly checked here instead of setting and
        // checking |fallbackMode| as specified in the spec.
        if (!inputRequest)
            request->setMode(WebURLRequest::FetchRequestModeCORS);
    }

    // "Let |credentials| be |init|'s credentials member if it is present, and
    // |fallbackCredentials| otherwise."
    // "If |credentials| is non-null, set |request|'s credentials mode to
    // |credentials|."
    if (init.credentials == "omit") {
        request->setCredentials(WebURLRequest::FetchCredentialsModeOmit);
    } else if (init.credentials == "same-origin") {
        request->setCredentials(WebURLRequest::FetchCredentialsModeSameOrigin);
    } else if (init.credentials == "include") {
        request->setCredentials(WebURLRequest::FetchCredentialsModeInclude);
    } else if (init.credentials == "password") {
        if (!init.attachedCredential.get()) {
            exceptionState.throwTypeError("Cannot construct a Request with a credential mode of 'password' without a PasswordCredential.");
            return nullptr;
        }
        request->setCredentials(WebURLRequest::FetchCredentialsModePassword);
        request->setAttachedCredential(init.attachedCredential);
        request->setRedirect(WebURLRequest::FetchRedirectModeManual);
    } else {
        if (!inputRequest)
            request->setCredentials(WebURLRequest::FetchCredentialsModeOmit);
    }

    // TODO(yhirano): Implement the following step:
    //   "If |init|'s cache member is present, set |request|'s cache mode to
    //   it."

    // "If |init|'s redirect member is present, set |request|'s redirect mode
    // to it."
    if (init.redirect == "follow") {
        request->setRedirect(WebURLRequest::FetchRedirectModeFollow);
    } else if (init.redirect == "error") {
        request->setRedirect(WebURLRequest::FetchRedirectModeError);
    } else if (init.redirect == "manual") {
        request->setRedirect(WebURLRequest::FetchRedirectModeManual);
    }

    // "If |init|'s integrity member is present, set |request|'s
    // integrity metadata to it."
    if (!init.integrity.isNull())
        request->setIntegrity(init.integrity);

    // "If |init|'s method member is present, let |method| be it and run these
    // substeps:"
    if (!init.method.isNull()) {
        // "If |method| is not a method or method is a forbidden method, throw
        // a TypeError."
        if (!isValidHTTPToken(init.method)) {
            exceptionState.throwTypeError("'" + init.method + "' is not a valid HTTP method.");
            return nullptr;
        }
        if (FetchUtils::isForbiddenMethod(init.method)) {
            exceptionState.throwTypeError("'" + init.method + "' HTTP method is unsupported.");
            return nullptr;
        }
        // "Normalize |method|."
        // "Set |request|'s method to |method|."
        request->setMethod(FetchUtils::normalizeMethod(AtomicString(init.method)));
    }
    // "Let |r| be a new Request object associated with |request| and a new
    // Headers object whose guard is "request"."
    Request* r = Request::create(scriptState, request);
    // Perform the following steps:
    // - "Let |headers| be a copy of |r|'s Headers object."
    // - "If |init|'s headers member is present, set |headers| to |init|'s
    //   headers member."
    //
    // We don't create a copy of r's Headers object when init's headers member
    // is present.
    Headers* headers = nullptr;
    if (!init.headers && init.headersDictionary.isUndefinedOrNull()) {
        headers = r->getHeaders()->clone();
    }
    // "Empty |r|'s request's header list."
    r->m_request->headerList()->clearList();
    // "If |r|'s request's mode is "no-cors", run these substeps:
    if (r->request()->mode() == WebURLRequest::FetchRequestModeNoCORS) {
        // "If |r|'s request's method is not a simple method, throw a
        // TypeError."
        if (!FetchUtils::isSimpleMethod(r->request()->method())) {
            exceptionState.throwTypeError("'" + r->request()->method() + "' is unsupported in no-cors mode.");
            return nullptr;
        }
        // "If |request|'s integrity metadata is not the empty string, throw a
        // TypeError."
        if (!request->integrity().isEmpty()) {
            exceptionState.throwTypeError("The integrity attribute is unsupported in no-cors mode.");
            return nullptr;
        }
        // "Set |r|'s Headers object's guard to "request-no-cors"."
        r->getHeaders()->setGuard(Headers::RequestNoCORSGuard);
    }
    // "Fill |r|'s Headers object with |headers|. Rethrow any exceptions."
    if (init.headers) {
        ASSERT(init.headersDictionary.isUndefinedOrNull());
        r->getHeaders()->fillWith(init.headers.get(), exceptionState);
    } else if (!init.headersDictionary.isUndefinedOrNull()) {
        r->getHeaders()->fillWith(init.headersDictionary, exceptionState);
    } else {
        ASSERT(headers);
        r->getHeaders()->fillWith(headers, exceptionState);
    }
    if (exceptionState.hadException())
        return nullptr;

    // "If either |init|'s body member is present or |temporaryBody| is
    // non-null, and |request|'s method is `GET` or `HEAD`, throw a TypeError.
    if (init.body || temporaryBody || request->credentials() == WebURLRequest::FetchCredentialsModePassword) {
        if (request->method() == HTTPNames::GET || request->method() == HTTPNames::HEAD) {
            exceptionState.throwTypeError("Request with GET/HEAD method cannot have body.");
            return nullptr;
        }
    }

    // TODO(mkwst): See the comment in RequestInit about serializing the attached credential
    // prior to hitting the Service Worker machinery.
    if (request->credentials() == WebURLRequest::FetchCredentialsModePassword) {
        r->getHeaders()->append(HTTPNames::Content_Type, init.contentType, exceptionState);

        const OriginAccessEntry accessEntry = OriginAccessEntry(r->url().protocol(), r->url().host(), OriginAccessEntry::AllowRegisterableDomains);
        if (accessEntry.matchesDomain(*origin) == OriginAccessEntry::DoesNotMatchOrigin) {
            exceptionState.throwTypeError("Credentials may only be submitted to endpoints on the same registrable domain.");
            return nullptr;
        }
    }

    // "If |init|'s body member is present, run these substeps:"
    if (init.body) {
        // Perform the following steps:
        // - "Let |stream| and |Content-Type| be the result of extracting
        //   |init|'s body member."
        // - "Set |temporaryBody| to |stream|.
        // - "If |Content-Type| is non-null and |r|'s request's header list
        //   contains no header named `Content-Type`, append
        //   `Content-Type`/|Content-Type| to |r|'s Headers object. Rethrow any
        //   exception."
        temporaryBody = new BodyStreamBuffer(scriptState, std::move(init.body));
        if (!init.contentType.isEmpty() && !r->getHeaders()->has(HTTPNames::Content_Type, exceptionState)) {
            r->getHeaders()->append(HTTPNames::Content_Type, init.contentType, exceptionState);
        }
        if (exceptionState.hadException())
            return nullptr;
    }

    // "Set |r|'s request's body to |temporaryBody|.
    if (temporaryBody) {
        r->m_request->setBuffer(temporaryBody);
        r->refreshBody(scriptState);
    }

    // "Set |r|'s MIME type to the result of extracting a MIME type from |r|'s
    // request's header list."
    r->m_request->setMIMEType(r->m_request->headerList()->extractMIMEType());

    // "If |input| is a Request object and |input|'s request's body is
    // non-null, run these substeps:"
    if (inputRequest && inputRequest->bodyBuffer()) {
        // "Let |dummyStream| be an empty ReadableStream object."
        auto dummyStream = new BodyStreamBuffer(scriptState, createFetchDataConsumerHandleFromWebHandle(createDoneDataConsumerHandle()));
        // "Set |input|'s request's body to a new body whose stream is
        // |dummyStream|."
        inputRequest->m_request->setBuffer(dummyStream);
        inputRequest->refreshBody(scriptState);
        // "Let |reader| be the result of getting reader from |dummyStream|."
        // "Read all bytes from |dummyStream| with |reader|."
        inputRequest->bodyBuffer()->closeAndLockAndDisturb();
    }

    // "Return |r|."
    return r;
}

Request* Request::create(ScriptState* scriptState, const RequestInfo& input, const Dictionary& init, ExceptionState& exceptionState)
{
    ASSERT(!input.isNull());
    if (input.isUSVString())
        return create(scriptState, input.getAsUSVString(), init, exceptionState);
    return create(scriptState, input.getAsRequest(), init, exceptionState);
}

Request* Request::create(ScriptState* scriptState, const String& input, ExceptionState& exceptionState)
{
    return create(scriptState, input, Dictionary(), exceptionState);
}

Request* Request::create(ScriptState* scriptState, const String& input, const Dictionary& init, ExceptionState& exceptionState)
{
    RequestInit requestInit(scriptState->getExecutionContext(), init, exceptionState);
    return createRequestWithRequestOrString(scriptState, nullptr, input, requestInit, exceptionState);
}

Request* Request::create(ScriptState* scriptState, Request* input, ExceptionState& exceptionState)
{
    return create(scriptState, input, Dictionary(), exceptionState);
}

Request* Request::create(ScriptState* scriptState, Request* input, const Dictionary& init, ExceptionState& exceptionState)
{
    RequestInit requestInit(scriptState->getExecutionContext(), init, exceptionState);
    return createRequestWithRequestOrString(scriptState, input, String(), requestInit, exceptionState);
}

Request* Request::create(ScriptState* scriptState, FetchRequestData* request)
{
    return new Request(scriptState, request);
}

Request* Request::create(ScriptState* scriptState, const WebServiceWorkerRequest& webRequest)
{
    return new Request(scriptState, webRequest);
}

Request::Request(ScriptState* scriptState, FetchRequestData* request, Headers* headers)
    : Body(scriptState->getExecutionContext())
    , m_request(request)
    , m_headers(headers)
{
    refreshBody(scriptState);
}

Request::Request(ScriptState* scriptState, FetchRequestData* request)
    : Request(scriptState, request, Headers::create(request->headerList()))
{
    m_headers->setGuard(Headers::RequestGuard);
}

Request::Request(ScriptState* scriptState, const WebServiceWorkerRequest& request)
    : Request(scriptState, FetchRequestData::create(scriptState, request))
{
}

String Request::method() const
{
    // "The method attribute's getter must return request's method."
    return m_request->method();
}

KURL Request::url() const
{
    // The url attribute's getter must return request's url, serialized with the exclude fragment flag set.
    if (!m_request->url().hasFragmentIdentifier())
        return m_request->url();
    KURL url(m_request->url());
    url.removeFragmentIdentifier();
    return url;
}

String Request::context() const
{
    // "The context attribute's getter must return request's context"
    switch (m_request->context()) {
    case WebURLRequest::RequestContextUnspecified:
        return "";
    case WebURLRequest::RequestContextAudio:
        return "audio";
    case WebURLRequest::RequestContextBeacon:
        return "beacon";
    case WebURLRequest::RequestContextCSPReport:
        return "cspreport";
    case WebURLRequest::RequestContextDownload:
        return "download";
    case WebURLRequest::RequestContextEmbed:
        return "embed";
    case WebURLRequest::RequestContextEventSource:
        return "eventsource";
    case WebURLRequest::RequestContextFavicon:
        return "favicon";
    case WebURLRequest::RequestContextFetch:
        return "fetch";
    case WebURLRequest::RequestContextFont:
        return "font";
    case WebURLRequest::RequestContextForm:
        return "form";
    case WebURLRequest::RequestContextFrame:
        return "frame";
    case WebURLRequest::RequestContextHyperlink:
        return "hyperlink";
    case WebURLRequest::RequestContextIframe:
        return "iframe";
    case WebURLRequest::RequestContextImage:
        return "image";
    case WebURLRequest::RequestContextImageSet:
        return "imageset";
    case WebURLRequest::RequestContextImport:
        return "import";
    case WebURLRequest::RequestContextInternal:
        return "internal";
    case WebURLRequest::RequestContextLocation:
        return "location";
    case WebURLRequest::RequestContextManifest:
        return "manifest";
    case WebURLRequest::RequestContextObject:
        return "object";
    case WebURLRequest::RequestContextPing:
        return "ping";
    case WebURLRequest::RequestContextPlugin:
        return "plugin";
    case WebURLRequest::RequestContextPrefetch:
        return "prefetch";
    case WebURLRequest::RequestContextScript:
        return "script";
    case WebURLRequest::RequestContextServiceWorker:
        return "serviceworker";
    case WebURLRequest::RequestContextSharedWorker:
        return "sharedworker";
    case WebURLRequest::RequestContextSubresource:
        return "subresource";
    case WebURLRequest::RequestContextStyle:
        return "style";
    case WebURLRequest::RequestContextTrack:
        return "track";
    case WebURLRequest::RequestContextVideo:
        return "video";
    case WebURLRequest::RequestContextWorker:
        return "worker";
    case WebURLRequest::RequestContextXMLHttpRequest:
        return "xmlhttprequest";
    case WebURLRequest::RequestContextXSLT:
        return "xslt";
    }
    ASSERT_NOT_REACHED();
    return "";
}

String Request::referrer() const
{
    // "The referrer attribute's getter must return the empty string if
    // request's referrer is no referrer, "about:client" if request's referrer
    // is client and request's referrer, serialized, otherwise."
    ASSERT(FetchRequestData::noReferrerString() == AtomicString());
    ASSERT(FetchRequestData::clientReferrerString() == AtomicString("about:client"));
    return m_request->referrerString();
}

String Request::referrerPolicy() const
{
    switch (m_request->getReferrerPolicy()) {
    case ReferrerPolicyAlways:
        return "unsafe-url";
    case ReferrerPolicyDefault:
        return "";
    case ReferrerPolicyNoReferrerWhenDowngrade:
        return "no-referrer-when-downgrade";
    case ReferrerPolicyNever:
        return "no-referrer";
    case ReferrerPolicyOrigin:
        return "origin";
    case ReferrerPolicyOriginWhenCrossOrigin:
        return "origin-when-cross-origin";
    case ReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin:
        ASSERT(RuntimeEnabledFeatures::reducedReferrerGranularityEnabled());
        return "no-referrer-when-downgrade-origin-when-cross-origin";
    }
    ASSERT_NOT_REACHED();
    return String();
}

String Request::mode() const
{
    // "The mode attribute's getter must return the value corresponding to the
    // first matching statement, switching on request's mode:"
    switch (m_request->mode()) {
    case WebURLRequest::FetchRequestModeSameOrigin:
        return "same-origin";
    case WebURLRequest::FetchRequestModeNoCORS:
        return "no-cors";
    case WebURLRequest::FetchRequestModeCORS:
    case WebURLRequest::FetchRequestModeCORSWithForcedPreflight:
        return "cors";
    case WebURLRequest::FetchRequestModeNavigate:
        return "navigate";
    }
    ASSERT_NOT_REACHED();
    return "";
}

String Request::credentials() const
{
    // "The credentials attribute's getter must return the value corresponding
    // to the first matching statement, switching on request's credentials
    // mode:"
    switch (m_request->credentials()) {
    case WebURLRequest::FetchCredentialsModeOmit:
        return "omit";
    case WebURLRequest::FetchCredentialsModeSameOrigin:
        return "same-origin";
    case WebURLRequest::FetchCredentialsModeInclude:
        return "include";
    case WebURLRequest::FetchCredentialsModePassword:
        return "password";
    }
    ASSERT_NOT_REACHED();
    return "";
}

String Request::redirect() const
{
    // "The redirect attribute's getter must return request's redirect mode."
    switch (m_request->redirect()) {
    case WebURLRequest::FetchRedirectModeFollow:
        return "follow";
    case WebURLRequest::FetchRedirectModeError:
        return "error";
    case WebURLRequest::FetchRedirectModeManual:
        return "manual";
    }
    ASSERT_NOT_REACHED();
    return "";
}

String Request::integrity() const
{
    return m_request->integrity();
}

Request* Request::clone(ScriptState* scriptState, ExceptionState& exceptionState)
{
    if (isBodyLocked() || bodyUsed()) {
        exceptionState.throwTypeError("Request body is already used");
        return nullptr;
    }

    FetchRequestData* request = m_request->clone(scriptState);
    refreshBody(scriptState);
    Headers* headers = Headers::create(request->headerList());
    headers->setGuard(m_headers->getGuard());
    return new Request(scriptState, request, headers);
}

FetchRequestData* Request::passRequestData(ScriptState* scriptState)
{
    ASSERT(!bodyUsed());
    FetchRequestData* data = m_request->pass(scriptState);
    refreshBody(scriptState);
    // |data|'s buffer('s js wrapper) has no retainer, but it's OK because
    // the only caller is the fetch function and it uses the body buffer
    // immediately.
    return data;
}

bool Request::hasBody() const
{
    return bodyBuffer();
}

void Request::populateWebServiceWorkerRequest(WebServiceWorkerRequest& webRequest) const
{
    webRequest.setMethod(method());
    webRequest.setRequestContext(m_request->context());
    // This strips off the fragment part.
    webRequest.setURL(url());

    const FetchHeaderList* headerList = m_headers->headerList();
    for (size_t i = 0, size = headerList->size(); i < size; ++i) {
        const FetchHeaderList::Header& header = headerList->entry(i);
        webRequest.appendHeader(header.first, header.second);
    }

    webRequest.setReferrer(m_request->referrerString(), static_cast<WebReferrerPolicy>(m_request->getReferrerPolicy()));
    // FIXME: How can we set isReload properly? What is the correct place to load it in to the Request object? We should investigate the right way
    // to plumb this information in to here.
}

String Request::mimeType() const
{
    return m_request->mimeType();
}

void Request::refreshBody(ScriptState* scriptState)
{
    v8::Local<v8::Value> bodyBuffer = toV8(this->bodyBuffer(), scriptState);
    v8::Local<v8::Value> request = toV8(this, scriptState);
    if (request.IsEmpty()) {
        // |toV8| can return an empty handle when the worker is terminating.
        // We don't want the renderer to crash in such cases.
        // TODO(yhirano): Delete this block after the graceful shutdown
        // mechanism is introduced.
        return;
    }
    DCHECK(request->IsObject());
    V8HiddenValue::setHiddenValue(scriptState, request.As<v8::Object>(), V8HiddenValue::internalBodyBuffer(scriptState->isolate()), bodyBuffer);
}

DEFINE_TRACE(Request)
{
    Body::trace(visitor);
    visitor->trace(m_request);
    visitor->trace(m_headers);
}

} // namespace blink
