// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSPDirectiveList_h
#define CSPDirectiveList_h

#include "core/fetch/Resource.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/frame/csp/MediaListDirective.h"
#include "core/frame/csp/SourceListDirective.h"
#include "platform/heap/Handle.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/network/HTTPParsers.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/ReferrerPolicy.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ContentSecurityPolicy;

class CORE_EXPORT CSPDirectiveList : public GarbageCollectedFinalized<CSPDirectiveList> {
    WTF_MAKE_NONCOPYABLE(CSPDirectiveList);
public:
    static CSPDirectiveList* create(ContentSecurityPolicy*, const UChar* begin, const UChar* end, ContentSecurityPolicyHeaderType, ContentSecurityPolicyHeaderSource);

    void parse(const UChar* begin, const UChar* end);

    const String& header() const { return m_header; }
    ContentSecurityPolicyHeaderType headerType() const { return m_headerType; }
    ContentSecurityPolicyHeaderSource headerSource() const { return m_headerSource; }

    bool allowJavaScriptURLs(const String& contextURL, const WTF::OrdinalNumber& contextLine, ContentSecurityPolicy::ReportingStatus) const;
    bool allowInlineEventHandlers(const String& contextURL, const WTF::OrdinalNumber& contextLine, ContentSecurityPolicy::ReportingStatus) const;
    bool allowInlineScript(const String& contextURL, const String& nonce, const WTF::OrdinalNumber& contextLine, ContentSecurityPolicy::ReportingStatus, const String& scriptContent) const;
    bool allowInlineStyle(const String& contextURL, const String& nonce, const WTF::OrdinalNumber& contextLine, ContentSecurityPolicy::ReportingStatus, const String& styleContent) const;
    bool allowEval(ScriptState*, ContentSecurityPolicy::ReportingStatus, ContentSecurityPolicy::ExceptionStatus = ContentSecurityPolicy::WillNotThrowException) const;
    bool allowPluginType(const String& type, const String& typeAttribute, const KURL&, ContentSecurityPolicy::ReportingStatus) const;

    bool allowScriptFromSource(const KURL&, const String& nonce, ResourceRequest::RedirectStatus, ContentSecurityPolicy::ReportingStatus) const;
    bool allowStyleFromSource(const KURL&, const String& nonce, ResourceRequest::RedirectStatus, ContentSecurityPolicy::ReportingStatus) const;

    bool allowObjectFromSource(const KURL&, ResourceRequest::RedirectStatus, ContentSecurityPolicy::ReportingStatus) const;
    bool allowChildFrameFromSource(const KURL&, ResourceRequest::RedirectStatus, ContentSecurityPolicy::ReportingStatus) const;
    bool allowImageFromSource(const KURL&, ResourceRequest::RedirectStatus, ContentSecurityPolicy::ReportingStatus) const;
    bool allowFontFromSource(const KURL&, ResourceRequest::RedirectStatus, ContentSecurityPolicy::ReportingStatus) const;
    bool allowMediaFromSource(const KURL&, ResourceRequest::RedirectStatus, ContentSecurityPolicy::ReportingStatus) const;
    bool allowManifestFromSource(const KURL&, ResourceRequest::RedirectStatus, ContentSecurityPolicy::ReportingStatus) const;
    bool allowConnectToSource(const KURL&, ResourceRequest::RedirectStatus, ContentSecurityPolicy::ReportingStatus) const;
    bool allowFormAction(const KURL&, ResourceRequest::RedirectStatus, ContentSecurityPolicy::ReportingStatus) const;
    bool allowBaseURI(const KURL&, ResourceRequest::RedirectStatus, ContentSecurityPolicy::ReportingStatus) const;
    bool allowChildContextFromSource(const KURL&, ResourceRequest::RedirectStatus, ContentSecurityPolicy::ReportingStatus) const;
    // |allowAncestors| does not need to know whether the resource was a
    // result of a redirect. After a redirect, source paths are usually
    // ignored to stop a page from learning the path to which the
    // request was redirected, but this is not a concern for ancestors,
    // because a child frame can't manipulate the URL of a cross-origin
    // parent.
    bool allowAncestors(LocalFrame*, const KURL&, ContentSecurityPolicy::ReportingStatus) const;
    bool allowScriptHash(const CSPHashValue&, ContentSecurityPolicy::InlineType) const;
    bool allowStyleHash(const CSPHashValue&, ContentSecurityPolicy::InlineType) const;
    bool allowDynamic() const;

    bool strictMixedContentChecking() const { return m_strictMixedContentCheckingEnforced; }
    void reportMixedContent(const KURL& mixedURL, ResourceRequest::RedirectStatus) const;

    const String& evalDisabledErrorMessage() const { return m_evalDisabledErrorMessage; }
    ReflectedXSSDisposition getReflectedXSSDisposition() const { return m_reflectedXSSDisposition; }
    ReferrerPolicy getReferrerPolicy() const { return m_referrerPolicy; }
    bool didSetReferrerPolicy() const { return m_didSetReferrerPolicy; }
    bool isReportOnly() const { return m_reportOnly; }
    const Vector<String>& reportEndpoints() const { return m_reportEndpoints; }
    bool isFrameAncestorsEnforced() const { return m_frameAncestors.get() && !m_reportOnly; }

    // Used to copy plugin-types into a plugin document in a nested
    // browsing context.
    bool hasPluginTypes() const { return !!m_pluginTypes; }
    const String& pluginTypesText() const;

    bool shouldSendCSPHeader(Resource::Type) const;

    DECLARE_TRACE();

private:
    FRIEND_TEST_ALL_PREFIXES(CSPDirectiveListTest, IsMatchingNoncePresent);

    CSPDirectiveList(ContentSecurityPolicy*, ContentSecurityPolicyHeaderType, ContentSecurityPolicyHeaderSource);

    bool parseDirective(const UChar* begin, const UChar* end, String& name, String& value);
    void parseReportURI(const String& name, const String& value);
    void parsePluginTypes(const String& name, const String& value);
    void parseReflectedXSS(const String& name, const String& value);
    void parseReferrer(const String& name, const String& value);
    void addDirective(const String& name, const String& value);
    void applySandboxPolicy(const String& name, const String& sandboxPolicy);
    void enforceStrictMixedContentChecking(const String& name, const String& value);
    void enableInsecureRequestsUpgrade(const String& name, const String& value);
    void treatAsPublicAddress(const String& name, const String& value);

    template <class CSPDirectiveType>
    void setCSPDirective(const String& name, const String& value, Member<CSPDirectiveType>&);

    SourceListDirective* operativeDirective(SourceListDirective*) const;
    SourceListDirective* operativeDirective(SourceListDirective*, SourceListDirective* override) const;
    void reportViolation(const String& directiveText, const String& effectiveDirective, const String& consoleMessage, const KURL& blockedURL, ResourceRequest::RedirectStatus) const;
    void reportViolationWithFrame(const String& directiveText, const String& effectiveDirective, const String& consoleMessage, const KURL& blockedURL, LocalFrame*) const;
    void reportViolationWithLocation(const String& directiveText, const String& effectiveDirective, const String& consoleMessage, const KURL& blockedURL, const String& contextURL, const WTF::OrdinalNumber& contextLine) const;
    void reportViolationWithState(const String& directiveText, const String& effectiveDirective, const String& message, const KURL& blockedURL, ScriptState*, const ContentSecurityPolicy::ExceptionStatus) const;

    bool checkEval(SourceListDirective*) const;
    bool checkInline(SourceListDirective*) const;
    bool checkDynamic(SourceListDirective*) const;
    bool isMatchingNoncePresent(SourceListDirective*, const String&) const;
    bool checkHash(SourceListDirective*, const CSPHashValue&) const;
    bool checkHashedAttributes(SourceListDirective*) const;
    bool checkSource(SourceListDirective*, const KURL&, ResourceRequest::RedirectStatus) const;
    bool checkMediaType(MediaListDirective*, const String& type, const String& typeAttribute) const;
    bool checkAncestors(SourceListDirective*, LocalFrame*) const;

    void setEvalDisabledErrorMessage(const String& errorMessage) { m_evalDisabledErrorMessage = errorMessage; }

    bool checkEvalAndReportViolation(SourceListDirective*, const String& consoleMessage, ScriptState*, ContentSecurityPolicy::ExceptionStatus = ContentSecurityPolicy::WillNotThrowException) const;
    bool checkInlineAndReportViolation(SourceListDirective*, const String& consoleMessage, const String& contextURL, const WTF::OrdinalNumber& contextLine, bool isScript, const String& hashValue) const;

    bool checkSourceAndReportViolation(SourceListDirective*, const KURL&, const String& effectiveDirective, ResourceRequest::RedirectStatus) const;
    bool checkMediaTypeAndReportViolation(MediaListDirective*, const String& type, const String& typeAttribute, const String& consoleMessage) const;
    bool checkAncestorsAndReportViolation(SourceListDirective*, LocalFrame*, const KURL&) const;

    bool denyIfEnforcingPolicy() const { return m_reportOnly; }

    Member<ContentSecurityPolicy> m_policy;

    String m_header;
    ContentSecurityPolicyHeaderType m_headerType;
    ContentSecurityPolicyHeaderSource m_headerSource;

    bool m_reportOnly;
    bool m_hasSandboxPolicy;
    ReflectedXSSDisposition m_reflectedXSSDisposition;

    bool m_didSetReferrerPolicy;
    ReferrerPolicy m_referrerPolicy;

    bool m_strictMixedContentCheckingEnforced;

    bool m_upgradeInsecureRequests;
    bool m_treatAsPublicAddress;

    Member<MediaListDirective> m_pluginTypes;
    Member<SourceListDirective> m_baseURI;
    Member<SourceListDirective> m_childSrc;
    Member<SourceListDirective> m_connectSrc;
    Member<SourceListDirective> m_defaultSrc;
    Member<SourceListDirective> m_fontSrc;
    Member<SourceListDirective> m_formAction;
    Member<SourceListDirective> m_frameAncestors;
    Member<SourceListDirective> m_frameSrc;
    Member<SourceListDirective> m_imgSrc;
    Member<SourceListDirective> m_mediaSrc;
    Member<SourceListDirective> m_manifestSrc;
    Member<SourceListDirective> m_objectSrc;
    Member<SourceListDirective> m_scriptSrc;
    Member<SourceListDirective> m_styleSrc;

    Vector<String> m_reportEndpoints;

    String m_evalDisabledErrorMessage;
};

} // namespace blink

#endif
