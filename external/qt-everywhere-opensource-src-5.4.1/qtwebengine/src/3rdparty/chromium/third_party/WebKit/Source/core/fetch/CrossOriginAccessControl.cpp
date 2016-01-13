/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 *
 */

#include "config.h"
#include "core/fetch/CrossOriginAccessControl.h"

#include "core/fetch/Resource.h"
#include "core/fetch/ResourceLoaderOptions.h"
#include "platform/network/HTTPParsers.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/Threading.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

bool isOnAccessControlSimpleRequestMethodWhitelist(const String& method)
{
    return method == "GET" || method == "HEAD" || method == "POST";
}

bool isOnAccessControlSimpleRequestHeaderWhitelist(const AtomicString& name, const AtomicString& value)
{
    if (equalIgnoringCase(name, "accept")
        || equalIgnoringCase(name, "accept-language")
        || equalIgnoringCase(name, "content-language")
        || equalIgnoringCase(name, "origin")
        || equalIgnoringCase(name, "referer"))
        return true;

    // Preflight is required for MIME types that can not be sent via form submission.
    if (equalIgnoringCase(name, "content-type")) {
        AtomicString mimeType = extractMIMETypeFromMediaType(value);
        return equalIgnoringCase(mimeType, "application/x-www-form-urlencoded")
            || equalIgnoringCase(mimeType, "multipart/form-data")
            || equalIgnoringCase(mimeType, "text/plain");
    }

    return false;
}

bool isSimpleCrossOriginAccessRequest(const String& method, const HTTPHeaderMap& headerMap)
{
    if (!isOnAccessControlSimpleRequestMethodWhitelist(method))
        return false;

    HTTPHeaderMap::const_iterator end = headerMap.end();
    for (HTTPHeaderMap::const_iterator it = headerMap.begin(); it != end; ++it) {
        if (!isOnAccessControlSimpleRequestHeaderWhitelist(it->key, it->value))
            return false;
    }

    return true;
}

static PassOwnPtr<HTTPHeaderSet> createAllowedCrossOriginResponseHeadersSet()
{
    OwnPtr<HTTPHeaderSet> headerSet = adoptPtr(new HashSet<String, CaseFoldingHash>);

    headerSet->add("cache-control");
    headerSet->add("content-language");
    headerSet->add("content-type");
    headerSet->add("expires");
    headerSet->add("last-modified");
    headerSet->add("pragma");

    return headerSet.release();
}

bool isOnAccessControlResponseHeaderWhitelist(const String& name)
{
    AtomicallyInitializedStatic(HTTPHeaderSet*, allowedCrossOriginResponseHeaders = createAllowedCrossOriginResponseHeadersSet().leakPtr());

    return allowedCrossOriginResponseHeaders->contains(name);
}

void updateRequestForAccessControl(ResourceRequest& request, SecurityOrigin* securityOrigin, StoredCredentials allowCredentials)
{
    request.removeCredentials();
    request.setAllowStoredCredentials(allowCredentials == AllowStoredCredentials);

    if (securityOrigin)
        request.setHTTPOrigin(securityOrigin->toAtomicString());
}

ResourceRequest createAccessControlPreflightRequest(const ResourceRequest& request, SecurityOrigin* securityOrigin)
{
    ResourceRequest preflightRequest(request.url());
    updateRequestForAccessControl(preflightRequest, securityOrigin, DoNotAllowStoredCredentials);
    preflightRequest.setHTTPMethod("OPTIONS");
    preflightRequest.setHTTPHeaderField("Access-Control-Request-Method", request.httpMethod());
    preflightRequest.setPriority(request.priority());

    const HTTPHeaderMap& requestHeaderFields = request.httpHeaderFields();

    if (requestHeaderFields.size() > 0) {
        StringBuilder headerBuffer;
        HTTPHeaderMap::const_iterator it = requestHeaderFields.begin();
        headerBuffer.append(it->key);
        ++it;

        HTTPHeaderMap::const_iterator end = requestHeaderFields.end();
        for (; it != end; ++it) {
            headerBuffer.appendLiteral(", ");
            headerBuffer.append(it->key);
        }

        preflightRequest.setHTTPHeaderField("Access-Control-Request-Headers", AtomicString(headerBuffer.toString().lower()));
    }

    return preflightRequest;
}

static bool isOriginSeparator(UChar ch)
{
    return isASCIISpace(ch) || ch == ',';
}

bool passesAccessControlCheck(const ResourceResponse& response, StoredCredentials includeCredentials, SecurityOrigin* securityOrigin, String& errorDescription)
{
    AtomicallyInitializedStatic(AtomicString&, accessControlAllowOrigin = *new AtomicString("access-control-allow-origin", AtomicString::ConstructFromLiteral));
    AtomicallyInitializedStatic(AtomicString&, accessControlAllowCredentials = *new AtomicString("access-control-allow-credentials", AtomicString::ConstructFromLiteral));

    if (!response.httpStatusCode()) {
        errorDescription = "Received an invalid response. Origin '" + securityOrigin->toString() + "' is therefore not allowed access.";
        return false;
    }

    const AtomicString& accessControlOriginString = response.httpHeaderField(accessControlAllowOrigin);
    if (accessControlOriginString == starAtom) {
        // A wildcard Access-Control-Allow-Origin can not be used if credentials are to be sent,
        // even with Access-Control-Allow-Credentials set to true.
        if (includeCredentials == DoNotAllowStoredCredentials)
            return true;
        if (response.isHTTP()) {
            errorDescription = "A wildcard '*' cannot be used in the 'Access-Control-Allow-Origin' header when the credentials flag is true. Origin '" + securityOrigin->toString() + "' is therefore not allowed access.";
            return false;
        }
    } else if (accessControlOriginString != securityOrigin->toAtomicString()) {
        if (accessControlOriginString.isEmpty()) {
            errorDescription = "No 'Access-Control-Allow-Origin' header is present on the requested resource. Origin '" + securityOrigin->toString() + "' is therefore not allowed access.";
        } else if (accessControlOriginString.string().find(isOriginSeparator, 0) != kNotFound) {
            errorDescription = "The 'Access-Control-Allow-Origin' header contains multiple values '" + accessControlOriginString + "', but only one is allowed. Origin '" + securityOrigin->toString() + "' is therefore not allowed access.";
        } else {
            KURL headerOrigin(KURL(), accessControlOriginString);
            if (!headerOrigin.isValid())
                errorDescription = "The 'Access-Control-Allow-Origin' header contains the invalid value '" + accessControlOriginString + "'. Origin '" + securityOrigin->toString() + "' is therefore not allowed access.";
            else
                errorDescription = "The 'Access-Control-Allow-Origin' header has a value '" + accessControlOriginString + "' that is not equal to the supplied origin. Origin '" + securityOrigin->toString() + "' is therefore not allowed access.";
        }
        return false;
    }

    if (includeCredentials == AllowStoredCredentials) {
        const AtomicString& accessControlCredentialsString = response.httpHeaderField(accessControlAllowCredentials);
        if (accessControlCredentialsString != "true") {
            errorDescription = "Credentials flag is 'true', but the 'Access-Control-Allow-Credentials' header is '" + accessControlCredentialsString + "'. It must be 'true' to allow credentials.";
            return false;
        }
    }

    return true;
}

bool passesPreflightStatusCheck(const ResourceResponse& response, String& errorDescription)
{
    if (response.httpStatusCode() < 200 || response.httpStatusCode() >= 400) {
        errorDescription = "Invalid HTTP status code " + String::number(response.httpStatusCode());
        return false;
    }

    return true;
}

void parseAccessControlExposeHeadersAllowList(const String& headerValue, HTTPHeaderSet& headerSet)
{
    Vector<String> headers;
    headerValue.split(',', false, headers);
    for (unsigned headerCount = 0; headerCount < headers.size(); headerCount++) {
        String strippedHeader = headers[headerCount].stripWhiteSpace();
        if (!strippedHeader.isEmpty())
            headerSet.add(strippedHeader);
    }
}

bool CrossOriginAccessControl::isLegalRedirectLocation(const KURL& requestURL, String& errorDescription)
{
    // CORS restrictions imposed on Location: URL -- http://www.w3.org/TR/cors/#redirect-steps (steps 2 + 3.)
    if (!SchemeRegistry::shouldTreatURLSchemeAsCORSEnabled(requestURL.protocol())) {
        errorDescription = "The request was redirected to a URL ('" + requestURL.string() + "') which has a disallowed scheme for cross-origin requests.";
        return false;
    }

    if (!(requestURL.user().isEmpty() && requestURL.pass().isEmpty())) {
        errorDescription = "The request was redirected to a URL ('" + requestURL.string() + "') containing userinfo, which is disallowed for cross-origin requests.";
        return false;
    }

    return true;
}

bool CrossOriginAccessControl::handleRedirect(Resource* resource, SecurityOrigin* securityOrigin, ResourceRequest& request, const ResourceResponse& redirectResponse, ResourceLoaderOptions& options, String& errorMessage)
{
    // http://www.w3.org/TR/cors/#redirect-steps terminology:
    const KURL& originalURL = redirectResponse.url();
    const KURL& requestURL = request.url();

    bool redirectCrossOrigin = !securityOrigin->canRequest(requestURL);

    // Same-origin request URLs that redirect are allowed without checking access.
    if (!securityOrigin->canRequest(originalURL)) {
        // Follow http://www.w3.org/TR/cors/#redirect-steps
        String errorDescription;

        // Steps 3 & 4 - check if scheme and other URL restrictions hold.
        bool allowRedirect = isLegalRedirectLocation(requestURL, errorDescription);
        if (allowRedirect) {
            // Step 5: perform resource sharing access check.
            StoredCredentials withCredentials = resource->lastResourceRequest().allowStoredCredentials() ? AllowStoredCredentials : DoNotAllowStoredCredentials;
            allowRedirect = passesAccessControlCheck(redirectResponse, withCredentials, securityOrigin, errorDescription);
            if (allowRedirect) {
                RefPtr<SecurityOrigin> originalOrigin = SecurityOrigin::create(originalURL);
                // Step 6: if the request URL origin is not same origin as the original URL's,
                // set the source origin to a globally unique identifier.
                if (!originalOrigin->canRequest(requestURL)) {
                    options.securityOrigin = SecurityOrigin::createUnique();
                    securityOrigin = options.securityOrigin.get();
                }
            }
        }
        if (!allowRedirect) {
            const String& originalOrigin = SecurityOrigin::create(originalURL)->toString();
            errorMessage = "Redirect at origin '" + originalOrigin + "' has been blocked from loading by Cross-Origin Resource Sharing policy: " + errorDescription;
            return false;
        }
    }
    if (redirectCrossOrigin) {
        // If now to a different origin, update/set Origin:.
        request.clearHTTPOrigin();
        request.setHTTPOrigin(securityOrigin->toAtomicString());
        // If the user didn't request credentials in the first place, update our
        // state so we neither request them nor expect they must be allowed.
        if (options.credentialsRequested == ClientDidNotRequestCredentials)
            options.allowCredentials = DoNotAllowStoredCredentials;
    }
    return true;
}

} // namespace WebCore
