/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 * Copyright (C) 2014, Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "config.h"
#include "modules/navigatorcontentutils/NavigatorContentUtils.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "core/page/Page.h"
#include "wtf/HashSet.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

static HashSet<String>* protocolWhitelist;

static void initProtocolHandlerWhitelist()
{
    protocolWhitelist = new HashSet<String>;
    static const char* const protocols[] = {
        "bitcoin",
        "geo",
        "im",
        "irc",
        "ircs",
        "magnet",
        "mailto",
        "mms",
        "news",
        "nntp",
        "sip",
        "sms",
        "smsto",
        "ssh",
        "tel",
        "urn",
        "webcal",
        "wtai",
        "xmpp",
    };
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(protocols); ++i)
        protocolWhitelist->add(protocols[i]);
}

static bool verifyCustomHandlerURL(const KURL& baseURL, const String& url, ExceptionState& exceptionState)
{
    // The specification requires that it is a SyntaxError if the "%s" token is
    // not present.
    static const char token[] = "%s";
    int index = url.find(token);
    if (-1 == index) {
        exceptionState.throwDOMException(SyntaxError, "The url provided ('" + url + "') does not contain '%s'.");
        return false;
    }

    // It is also a SyntaxError if the custom handler URL, as created by removing
    // the "%s" token and prepending the base url, does not resolve.
    String newURL = url;
    newURL.remove(index, WTF_ARRAY_LENGTH(token) - 1);

    KURL kurl(baseURL, newURL);

    if (kurl.isEmpty() || !kurl.isValid()) {
        exceptionState.throwDOMException(SyntaxError, "The custom handler URL created by removing '%s' and prepending '" + baseURL.string() + "' is invalid.");
        return false;
    }

    return true;
}

static bool isProtocolWhitelisted(const String& scheme)
{
    if (!protocolWhitelist)
        initProtocolHandlerWhitelist();

    StringBuilder builder;
    unsigned length = scheme.length();
    for (unsigned i = 0; i < length; ++i)
        builder.append(toASCIILower(scheme[i]));

    return protocolWhitelist->contains(builder.toString());
}

static bool verifyProtocolHandlerScheme(const String& scheme, const String& method, ExceptionState& exceptionState)
{
    if (scheme.startsWith("web+")) {
        // The specification requires that the length of scheme is at least five characteres (including 'web+' prefix).
        if (scheme.length() >= 5 && isValidProtocol(scheme))
            return true;
        if (!isValidProtocol(scheme))
            exceptionState.throwSecurityError("The scheme '" + scheme + "' is not a valid protocol.");
        else
            exceptionState.throwSecurityError("The scheme '" + scheme + "' is less than five characters long.");
        return false;
    }

    // The specification requires that schemes don't contain colons.
    size_t index = scheme.find(':');
    if (index != kNotFound) {
        exceptionState.throwDOMException(SyntaxError, "The scheme '" + scheme + "' contains colon.");
        return false;
    }

    if (isProtocolWhitelisted(scheme))
        return true;
    exceptionState.throwSecurityError("The scheme '" + scheme + "' doesn't belong to the protocol whitelist. Please prefix non-whitelisted schemes with the string 'web+'.");
    return false;
}

NavigatorContentUtils* NavigatorContentUtils::from(Page& page)
{
    return static_cast<NavigatorContentUtils*>(WillBeHeapSupplement<Page>::from(page, supplementName()));
}

NavigatorContentUtils::~NavigatorContentUtils()
{
}

PassOwnPtrWillBeRawPtr<NavigatorContentUtils> NavigatorContentUtils::create(PassOwnPtr<NavigatorContentUtilsClient> client)
{
    return adoptPtrWillBeNoop(new NavigatorContentUtils(client));
}

void NavigatorContentUtils::registerProtocolHandler(Navigator& navigator, const String& scheme, const String& url, const String& title, ExceptionState& exceptionState)
{
    if (!navigator.frame())
        return;

    ASSERT(navigator.frame()->document());
    KURL baseURL = navigator.frame()->document()->baseURL();

    if (!verifyCustomHandlerURL(baseURL, url, exceptionState))
        return;

    if (!verifyProtocolHandlerScheme(scheme, "registerProtocolHandler", exceptionState))
        return;

    ASSERT(navigator.frame()->page());
    NavigatorContentUtils::from(*navigator.frame()->page())->client()->registerProtocolHandler(scheme, baseURL, KURL(ParsedURLString, url), title);
}

static String customHandlersStateString(const NavigatorContentUtilsClient::CustomHandlersState state)
{
    DEFINE_STATIC_LOCAL(const String, newHandler, ("new"));
    DEFINE_STATIC_LOCAL(const String, registeredHandler, ("registered"));
    DEFINE_STATIC_LOCAL(const String, declinedHandler, ("declined"));

    switch (state) {
    case NavigatorContentUtilsClient::CustomHandlersNew:
        return newHandler;
    case NavigatorContentUtilsClient::CustomHandlersRegistered:
        return registeredHandler;
    case NavigatorContentUtilsClient::CustomHandlersDeclined:
        return declinedHandler;
    }

    ASSERT_NOT_REACHED();
    return String();
}

String NavigatorContentUtils::isProtocolHandlerRegistered(Navigator& navigator, const String& scheme, const String& url, ExceptionState& exceptionState)
{
    DEFINE_STATIC_LOCAL(const String, declined, ("declined"));

    if (!navigator.frame())
        return declined;

    Document* document = navigator.frame()->document();
    ASSERT(document);
    if (document->activeDOMObjectsAreStopped())
        return declined;

    KURL baseURL = document->baseURL();

    if (!verifyCustomHandlerURL(baseURL, url, exceptionState))
        return declined;

    if (!verifyProtocolHandlerScheme(scheme, "isProtocolHandlerRegistered", exceptionState))
        return declined;

    ASSERT(navigator.frame()->page());
    return customHandlersStateString(NavigatorContentUtils::from(*navigator.frame()->page())->client()->isProtocolHandlerRegistered(scheme, baseURL, KURL(ParsedURLString, url)));
}

void NavigatorContentUtils::unregisterProtocolHandler(Navigator& navigator, const String& scheme, const String& url, ExceptionState& exceptionState)
{
    if (!navigator.frame())
        return;

    ASSERT(navigator.frame()->document());
    KURL baseURL = navigator.frame()->document()->baseURL();

    if (!verifyCustomHandlerURL(baseURL, url, exceptionState))
        return;

    if (!verifyProtocolHandlerScheme(scheme, "unregisterProtocolHandler", exceptionState))
        return;

    ASSERT(navigator.frame()->page());
    NavigatorContentUtils::from(*navigator.frame()->page())->client()->unregisterProtocolHandler(scheme, baseURL, KURL(ParsedURLString, url));
}

const char* NavigatorContentUtils::supplementName()
{
    return "NavigatorContentUtils";
}

void provideNavigatorContentUtilsTo(Page& page, PassOwnPtr<NavigatorContentUtilsClient> client)
{
    NavigatorContentUtils::provideTo(page, NavigatorContentUtils::supplementName(), NavigatorContentUtils::create(client));
}

} // namespace WebCore
