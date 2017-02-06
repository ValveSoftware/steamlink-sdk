// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/HttpEquiv.h"

#include "core/dom/Document.h"
#include "core/dom/ScriptableDocumentParser.h"
#include "core/dom/StyleEngine.h"
#include "core/fetch/ClientHintsPreferences.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLDocument.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/DocumentLoader.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "platform/HTTPNames.h"
#include "platform/network/HTTPParsers.h"
#include "platform/weborigin/KURL.h"

namespace blink {

void HttpEquiv::process(Document& document, const AtomicString& equiv, const AtomicString& content, bool inDocumentHeadElement)
{
    ASSERT(!equiv.isNull() && !content.isNull());

    if (equalIgnoringCase(equiv, "default-style")) {
        processHttpEquivDefaultStyle(document, content);
    } else if (equalIgnoringCase(equiv, "refresh")) {
        processHttpEquivRefresh(document, content);
    } else if (equalIgnoringCase(equiv, "set-cookie")) {
        processHttpEquivSetCookie(document, content);
    } else if (equalIgnoringCase(equiv, "content-language")) {
        document.setContentLanguage(content);
    } else if (equalIgnoringCase(equiv, "x-dns-prefetch-control")) {
        document.parseDNSPrefetchControlHeader(content);
    } else if (equalIgnoringCase(equiv, "x-frame-options")) {
        document.addConsoleMessage(ConsoleMessage::create(SecurityMessageSource, ErrorMessageLevel, "X-Frame-Options may only be set via an HTTP header sent along with a document. It may not be set inside <meta>."));
    } else if (equalIgnoringCase(equiv, "accept-ch")) {
        processHttpEquivAcceptCH(document, content);
    } else if (equalIgnoringCase(equiv, "content-security-policy") || equalIgnoringCase(equiv, "content-security-policy-report-only")) {
        if (inDocumentHeadElement)
            processHttpEquivContentSecurityPolicy(document, equiv, content);
        else
            document.contentSecurityPolicy()->reportMetaOutsideHead(content);
    } else if (equalIgnoringCase(equiv, "suborigin")) {
        document.addConsoleMessage(ConsoleMessage::create(SecurityMessageSource, ErrorMessageLevel, "Error with Suborigin header: Suborigin header with value '" + content + "' was delivered via a <meta> element and not an HTTP header, which is disallowed. The Suborigin has been ignored."));
    } else if (equalIgnoringCase(equiv, HTTPNames::Origin_Trial)) {
        if (inDocumentHeadElement)
            OriginTrialContext::from(&document)->addToken(content);
    }
}

void HttpEquiv::processHttpEquivContentSecurityPolicy(Document& document, const AtomicString& equiv, const AtomicString& content)
{
    if (document.importLoader())
        return;
    if (equalIgnoringCase(equiv, "content-security-policy"))
        document.contentSecurityPolicy()->didReceiveHeader(content, ContentSecurityPolicyHeaderTypeEnforce, ContentSecurityPolicyHeaderSourceMeta);
    else if (equalIgnoringCase(equiv, "content-security-policy-report-only"))
        document.contentSecurityPolicy()->didReceiveHeader(content, ContentSecurityPolicyHeaderTypeReport, ContentSecurityPolicyHeaderSourceMeta);
    else
        ASSERT_NOT_REACHED();
}

void HttpEquiv::processHttpEquivAcceptCH(Document& document, const AtomicString& content)
{
    if (!document.frame())
        return;

    UseCounter::count(document, UseCounter::ClientHintsMetaAcceptCH);
    document.clientHintsPreferences().updateFromAcceptClientHintsHeader(content, document.fetcher());
}

void HttpEquiv::processHttpEquivDefaultStyle(Document& document, const AtomicString& content)
{
    document.styleEngine().setHttpDefaultStyle(content);
}

void HttpEquiv::processHttpEquivRefresh(Document& document, const AtomicString& content)
{
    document.maybeHandleHttpRefresh(content, Document::HttpRefreshFromMetaTag);
}

void HttpEquiv::processHttpEquivSetCookie(Document& document, const AtomicString& content)
{
    // FIXME: make setCookie work on XML documents too; e.g. in case of <html:meta .....>
    if (!document.isHTMLDocument())
        return;

    // Exception (for sandboxed documents) ignored.
    toHTMLDocument(document).setCookie(content, IGNORE_EXCEPTION);
}

} // namespace blink
