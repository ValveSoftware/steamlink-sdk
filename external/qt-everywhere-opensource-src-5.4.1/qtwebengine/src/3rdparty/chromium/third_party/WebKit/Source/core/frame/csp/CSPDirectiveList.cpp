// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/frame/csp/CSPDirectiveList.h"

#include "core/frame/LocalFrame.h"
#include "platform/ParsingUtilities.h"
#include "platform/weborigin/KURL.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

CSPDirectiveList::CSPDirectiveList(ContentSecurityPolicy* policy, ContentSecurityPolicyHeaderType type, ContentSecurityPolicyHeaderSource source)
    : m_policy(policy)
    , m_headerType(type)
    , m_headerSource(source)
    , m_reportOnly(false)
    , m_haveSandboxPolicy(false)
    , m_reflectedXSSDisposition(ReflectedXSSUnset)
    , m_didSetReferrerPolicy(false)
    , m_referrerPolicy(ReferrerPolicyDefault)
{
    m_reportOnly = type == ContentSecurityPolicyHeaderTypeReport;
}

PassOwnPtr<CSPDirectiveList> CSPDirectiveList::create(ContentSecurityPolicy* policy, const UChar* begin, const UChar* end, ContentSecurityPolicyHeaderType type, ContentSecurityPolicyHeaderSource source)
{
    OwnPtr<CSPDirectiveList> directives = adoptPtr(new CSPDirectiveList(policy, type, source));
    directives->parse(begin, end);

    if (!directives->checkEval(directives->operativeDirective(directives->m_scriptSrc.get()))) {
        String message = "Refused to evaluate a string as JavaScript because 'unsafe-eval' is not an allowed source of script in the following Content Security Policy directive: \"" + directives->operativeDirective(directives->m_scriptSrc.get())->text() + "\".\n";
        directives->setEvalDisabledErrorMessage(message);
    }

    if (directives->isReportOnly() && directives->reportURIs().isEmpty())
        policy->reportMissingReportURI(String(begin, end - begin));

    return directives.release();
}

void CSPDirectiveList::reportViolation(const String& directiveText, const String& effectiveDirective, const String& consoleMessage, const KURL& blockedURL) const
{
    String message = m_reportOnly ? "[Report Only] " + consoleMessage : consoleMessage;
    m_policy->executionContext()->addConsoleMessage(SecurityMessageSource, ErrorMessageLevel, message);
    m_policy->reportViolation(directiveText, effectiveDirective, message, blockedURL, m_reportURIs, m_header);
}

void CSPDirectiveList::reportViolationWithLocation(const String& directiveText, const String& effectiveDirective, const String& consoleMessage, const KURL& blockedURL, const String& contextURL, const WTF::OrdinalNumber& contextLine) const
{
    String message = m_reportOnly ? "[Report Only] " + consoleMessage : consoleMessage;
    m_policy->executionContext()->addConsoleMessage(SecurityMessageSource, ErrorMessageLevel, message, contextURL, contextLine.oneBasedInt());
    m_policy->reportViolation(directiveText, effectiveDirective, message, blockedURL, m_reportURIs, m_header);
}

void CSPDirectiveList::reportViolationWithState(const String& directiveText, const String& effectiveDirective, const String& consoleMessage, const KURL& blockedURL, ScriptState* scriptState) const
{
    String message = m_reportOnly ? "[Report Only] " + consoleMessage : consoleMessage;
    m_policy->executionContext()->addConsoleMessage(SecurityMessageSource, ErrorMessageLevel, message, scriptState);
    m_policy->reportViolation(directiveText, effectiveDirective, message, blockedURL, m_reportURIs, m_header);
}

bool CSPDirectiveList::checkEval(SourceListDirective* directive) const
{
    return !directive || directive->allowEval();
}

bool CSPDirectiveList::checkInline(SourceListDirective* directive) const
{
    return !directive || (directive->allowInline() && !directive->isHashOrNoncePresent());
}

bool CSPDirectiveList::checkNonce(SourceListDirective* directive, const String& nonce) const
{
    return !directive || directive->allowNonce(nonce);
}

bool CSPDirectiveList::checkHash(SourceListDirective* directive, const CSPHashValue& hashValue) const
{
    return !directive || directive->allowHash(hashValue);
}

bool CSPDirectiveList::checkSource(SourceListDirective* directive, const KURL& url) const
{
    return !directive || directive->allows(url);
}

bool CSPDirectiveList::checkAncestors(SourceListDirective* directive, LocalFrame* frame) const
{
    if (!frame || !directive)
        return true;

    for (Frame* current = frame->tree().parent(); current; current = current->tree().parent()) {
        // FIXME: To make this work for out-of-process iframes, we need to propagate URL information of ancestor frames across processes.
        if (!current->isLocalFrame() || !directive->allows(toLocalFrame(current)->document()->url()))
            return false;
    }
    return true;
}

bool CSPDirectiveList::checkMediaType(MediaListDirective* directive, const String& type, const String& typeAttribute) const
{
    if (!directive)
        return true;
    if (typeAttribute.isEmpty() || typeAttribute.stripWhiteSpace() != type)
        return false;
    return directive->allows(type);
}

SourceListDirective* CSPDirectiveList::operativeDirective(SourceListDirective* directive) const
{
    return directive ? directive : m_defaultSrc.get();
}

SourceListDirective* CSPDirectiveList::operativeDirective(SourceListDirective* directive, SourceListDirective* override) const
{
    return directive ? directive : override;
}

bool CSPDirectiveList::checkEvalAndReportViolation(SourceListDirective* directive, const String& consoleMessage, ScriptState* scriptState) const
{
    if (checkEval(directive))
        return true;

    String suffix = String();
    if (directive == m_defaultSrc)
        suffix = " Note that 'script-src' was not explicitly set, so 'default-src' is used as a fallback.";

    reportViolationWithState(directive->text(), ContentSecurityPolicy::ScriptSrc, consoleMessage + "\"" + directive->text() + "\"." + suffix + "\n", KURL(), scriptState);
    if (!m_reportOnly) {
        m_policy->reportBlockedScriptExecutionToInspector(directive->text());
        return false;
    }
    return true;
}

bool CSPDirectiveList::checkMediaTypeAndReportViolation(MediaListDirective* directive, const String& type, const String& typeAttribute, const String& consoleMessage) const
{
    if (checkMediaType(directive, type, typeAttribute))
        return true;

    String message = consoleMessage + "\'" + directive->text() + "\'.";
    if (typeAttribute.isEmpty())
        message = message + " When enforcing the 'plugin-types' directive, the plugin's media type must be explicitly declared with a 'type' attribute on the containing element (e.g. '<object type=\"[TYPE GOES HERE]\" ...>').";

    reportViolation(directive->text(), ContentSecurityPolicy::PluginTypes, message + "\n", KURL());
    return denyIfEnforcingPolicy();
}

bool CSPDirectiveList::checkInlineAndReportViolation(SourceListDirective* directive, const String& consoleMessage, const String& contextURL, const WTF::OrdinalNumber& contextLine, bool isScript) const
{
    if (checkInline(directive))
        return true;

    String suffix = String();
    if (directive->allowInline() && directive->isHashOrNoncePresent()) {
        // If inline is allowed, but a hash or nonce is present, we ignore 'unsafe-inline'. Throw a reasonable error.
        suffix = " Note that 'unsafe-inline' is ignored if either a hash or nonce value is present in the source list.";
    } else {
        suffix = " Either the 'unsafe-inline' keyword, a hash ('sha256-...'), or a nonce ('nonce-...') is required to enable inline execution.";
        if (directive == m_defaultSrc)
            suffix = suffix + " Note also that '" + String(isScript ? "script" : "style") + "-src' was not explicitly set, so 'default-src' is used as a fallback.";
    }

    reportViolationWithLocation(directive->text(), isScript ? ContentSecurityPolicy::ScriptSrc : ContentSecurityPolicy::StyleSrc, consoleMessage + "\"" + directive->text() + "\"." + suffix + "\n", KURL(), contextURL, contextLine);

    if (!m_reportOnly) {
        if (isScript)
            m_policy->reportBlockedScriptExecutionToInspector(directive->text());
        return false;
    }
    return true;
}

bool CSPDirectiveList::checkSourceAndReportViolation(SourceListDirective* directive, const KURL& url, const String& effectiveDirective) const
{
    if (checkSource(directive, url))
        return true;

    String prefix;
    if (ContentSecurityPolicy::BaseURI == effectiveDirective)
        prefix = "Refused to set the document's base URI to '";
    else if (ContentSecurityPolicy::ChildSrc == effectiveDirective)
        prefix = "Refused to create a child context containing '";
    else if (ContentSecurityPolicy::ConnectSrc == effectiveDirective)
        prefix = "Refused to connect to '";
    else if (ContentSecurityPolicy::FontSrc == effectiveDirective)
        prefix = "Refused to load the font '";
    else if (ContentSecurityPolicy::FormAction == effectiveDirective)
        prefix = "Refused to send form data to '";
    else if (ContentSecurityPolicy::FrameSrc == effectiveDirective)
        prefix = "Refused to frame '";
    else if (ContentSecurityPolicy::ImgSrc == effectiveDirective)
        prefix = "Refused to load the image '";
    else if (ContentSecurityPolicy::MediaSrc == effectiveDirective)
        prefix = "Refused to load media from '";
    else if (ContentSecurityPolicy::ObjectSrc == effectiveDirective)
        prefix = "Refused to load plugin data from '";
    else if (ContentSecurityPolicy::ScriptSrc == effectiveDirective)
        prefix = "Refused to load the script '";
    else if (ContentSecurityPolicy::StyleSrc == effectiveDirective)
        prefix = "Refused to load the stylesheet '";

    String suffix = String();
    if (directive == m_defaultSrc)
        suffix = " Note that '" + effectiveDirective + "' was not explicitly set, so 'default-src' is used as a fallback.";

    reportViolation(directive->text(), effectiveDirective, prefix + url.elidedString() + "' because it violates the following Content Security Policy directive: \"" + directive->text() + "\"." + suffix + "\n", url);
    return denyIfEnforcingPolicy();
}

bool CSPDirectiveList::checkAncestorsAndReportViolation(SourceListDirective* directive, LocalFrame* frame) const
{
    if (checkAncestors(directive, frame))
        return true;

    reportViolation(directive->text(), "frame-ancestors", "Refused to display '" + frame->document()->url().elidedString() + " in a frame because an ancestor violates the following Content Security Policy directive: \"" + directive->text() + "\".", frame->document()->url());
    return denyIfEnforcingPolicy();
}

bool CSPDirectiveList::allowJavaScriptURLs(const String& contextURL, const WTF::OrdinalNumber& contextLine, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to execute JavaScript URL because it violates the following Content Security Policy directive: "));
    if (reportingStatus == ContentSecurityPolicy::SendReport)
        return checkInlineAndReportViolation(operativeDirective(m_scriptSrc.get()), consoleMessage, contextURL, contextLine, true);

    return checkInline(operativeDirective(m_scriptSrc.get()));
}

bool CSPDirectiveList::allowInlineEventHandlers(const String& contextURL, const WTF::OrdinalNumber& contextLine, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to execute inline event handler because it violates the following Content Security Policy directive: "));
    if (reportingStatus == ContentSecurityPolicy::SendReport)
        return checkInlineAndReportViolation(operativeDirective(m_scriptSrc.get()), consoleMessage, contextURL, contextLine, true);
    return checkInline(operativeDirective(m_scriptSrc.get()));
}

bool CSPDirectiveList::allowInlineScript(const String& contextURL, const WTF::OrdinalNumber& contextLine, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to execute inline script because it violates the following Content Security Policy directive: "));
    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkInlineAndReportViolation(operativeDirective(m_scriptSrc.get()), consoleMessage, contextURL, contextLine, true) :
        checkInline(operativeDirective(m_scriptSrc.get()));
}

bool CSPDirectiveList::allowInlineStyle(const String& contextURL, const WTF::OrdinalNumber& contextLine, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to apply inline style because it violates the following Content Security Policy directive: "));
    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkInlineAndReportViolation(operativeDirective(m_styleSrc.get()), consoleMessage, contextURL, contextLine, false) :
        checkInline(operativeDirective(m_styleSrc.get()));
}

bool CSPDirectiveList::allowEval(ScriptState* scriptState, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to evaluate a string as JavaScript because 'unsafe-eval' is not an allowed source of script in the following Content Security Policy directive: "));

    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkEvalAndReportViolation(operativeDirective(m_scriptSrc.get()), consoleMessage, scriptState) :
        checkEval(operativeDirective(m_scriptSrc.get()));
}

bool CSPDirectiveList::allowPluginType(const String& type, const String& typeAttribute, const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkMediaTypeAndReportViolation(m_pluginTypes.get(), type, typeAttribute, "Refused to load '" + url.elidedString() + "' (MIME type '" + typeAttribute + "') because it violates the following Content Security Policy Directive: ") :
        checkMediaType(m_pluginTypes.get(), type, typeAttribute);
}

bool CSPDirectiveList::allowScriptFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkSourceAndReportViolation(operativeDirective(m_scriptSrc.get()), url, ContentSecurityPolicy::ScriptSrc) :
        checkSource(operativeDirective(m_scriptSrc.get()), url);
}

bool CSPDirectiveList::allowObjectFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    if (url.protocolIsAbout())
        return true;
    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkSourceAndReportViolation(operativeDirective(m_objectSrc.get()), url, ContentSecurityPolicy::ObjectSrc) :
        checkSource(operativeDirective(m_objectSrc.get()), url);
}

bool CSPDirectiveList::allowChildFrameFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    if (url.protocolIsAbout())
        return true;

    // 'frame-src' is the only directive which overrides something other than the default sources.
    // It overrides 'child-src', which overrides the default sources. So, we do this nested set
    // of calls to 'operativeDirective()' to grab 'frame-src' if it exists, 'child-src' if it
    // doesn't, and 'defaut-src' if neither are available.
    //
    // All of this only applies, of course, if we're in CSP 1.1. In CSP 1.0, 'frame-src'
    // overrides 'default-src' directly.
    SourceListDirective* whichDirective = m_policy->experimentalFeaturesEnabled() ?
        operativeDirective(m_frameSrc.get(), operativeDirective(m_childSrc.get())) :
        operativeDirective(m_frameSrc.get());

    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkSourceAndReportViolation(whichDirective, url, ContentSecurityPolicy::FrameSrc) :
        checkSource(whichDirective, url);
}

bool CSPDirectiveList::allowImageFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkSourceAndReportViolation(operativeDirective(m_imgSrc.get()), url, ContentSecurityPolicy::ImgSrc) :
        checkSource(operativeDirective(m_imgSrc.get()), url);
}

bool CSPDirectiveList::allowStyleFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkSourceAndReportViolation(operativeDirective(m_styleSrc.get()), url, ContentSecurityPolicy::StyleSrc) :
        checkSource(operativeDirective(m_styleSrc.get()), url);
}

bool CSPDirectiveList::allowFontFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkSourceAndReportViolation(operativeDirective(m_fontSrc.get()), url, ContentSecurityPolicy::FontSrc) :
        checkSource(operativeDirective(m_fontSrc.get()), url);
}

bool CSPDirectiveList::allowMediaFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkSourceAndReportViolation(operativeDirective(m_mediaSrc.get()), url, ContentSecurityPolicy::MediaSrc) :
        checkSource(operativeDirective(m_mediaSrc.get()), url);
}

bool CSPDirectiveList::allowConnectToSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkSourceAndReportViolation(operativeDirective(m_connectSrc.get()), url, ContentSecurityPolicy::ConnectSrc) :
        checkSource(operativeDirective(m_connectSrc.get()), url);
}

bool CSPDirectiveList::allowFormAction(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkSourceAndReportViolation(m_formAction.get(), url, ContentSecurityPolicy::FormAction) :
        checkSource(m_formAction.get(), url);
}

bool CSPDirectiveList::allowBaseURI(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkSourceAndReportViolation(m_baseURI.get(), url, ContentSecurityPolicy::BaseURI) :
        checkSource(m_baseURI.get(), url);
}

bool CSPDirectiveList::allowAncestors(LocalFrame* frame, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkAncestorsAndReportViolation(m_frameAncestors.get(), frame) :
        checkAncestors(m_frameAncestors.get(), frame);
}

bool CSPDirectiveList::allowChildContextFromSource(const KURL& url, ContentSecurityPolicy::ReportingStatus reportingStatus) const
{
    return reportingStatus == ContentSecurityPolicy::SendReport ?
        checkSourceAndReportViolation(operativeDirective(m_childSrc.get()), url, ContentSecurityPolicy::ChildSrc) :
        checkSource(operativeDirective(m_childSrc.get()), url);
}

bool CSPDirectiveList::allowScriptNonce(const String& nonce) const
{
    return checkNonce(operativeDirective(m_scriptSrc.get()), nonce);
}

bool CSPDirectiveList::allowStyleNonce(const String& nonce) const
{
    return checkNonce(operativeDirective(m_styleSrc.get()), nonce);
}

bool CSPDirectiveList::allowScriptHash(const CSPHashValue& hashValue) const
{
    return checkHash(operativeDirective(m_scriptSrc.get()), hashValue);
}

bool CSPDirectiveList::allowStyleHash(const CSPHashValue& hashValue) const
{
    return checkHash(operativeDirective(m_styleSrc.get()), hashValue);
}

// policy            = directive-list
// directive-list    = [ directive *( ";" [ directive ] ) ]
//
void CSPDirectiveList::parse(const UChar* begin, const UChar* end)
{
    m_header = String(begin, end - begin);

    if (begin == end)
        return;

    const UChar* position = begin;
    while (position < end) {
        const UChar* directiveBegin = position;
        skipUntil<UChar>(position, end, ';');

        String name, value;
        if (parseDirective(directiveBegin, position, name, value)) {
            ASSERT(!name.isEmpty());
            addDirective(name, value);
        }

        ASSERT(position == end || *position == ';');
        skipExactly<UChar>(position, end, ';');
    }
}

// directive         = *WSP [ directive-name [ WSP directive-value ] ]
// directive-name    = 1*( ALPHA / DIGIT / "-" )
// directive-value   = *( WSP / <VCHAR except ";"> )
//
bool CSPDirectiveList::parseDirective(const UChar* begin, const UChar* end, String& name, String& value)
{
    ASSERT(name.isEmpty());
    ASSERT(value.isEmpty());

    const UChar* position = begin;
    skipWhile<UChar, isASCIISpace>(position, end);

    // Empty directive (e.g. ";;;"). Exit early.
    if (position == end)
        return false;

    const UChar* nameBegin = position;
    skipWhile<UChar, isCSPDirectiveNameCharacter>(position, end);

    // The directive-name must be non-empty.
    if (nameBegin == position) {
        skipWhile<UChar, isNotASCIISpace>(position, end);
        m_policy->reportUnsupportedDirective(String(nameBegin, position - nameBegin));
        return false;
    }

    name = String(nameBegin, position - nameBegin);

    if (position == end)
        return true;

    if (!skipExactly<UChar, isASCIISpace>(position, end)) {
        skipWhile<UChar, isNotASCIISpace>(position, end);
        m_policy->reportUnsupportedDirective(String(nameBegin, position - nameBegin));
        return false;
    }

    skipWhile<UChar, isASCIISpace>(position, end);

    const UChar* valueBegin = position;
    skipWhile<UChar, isCSPDirectiveValueCharacter>(position, end);

    if (position != end) {
        m_policy->reportInvalidDirectiveValueCharacter(name, String(valueBegin, end - valueBegin));
        return false;
    }

    // The directive-value may be empty.
    if (valueBegin == position)
        return true;

    value = String(valueBegin, position - valueBegin);
    return true;
}

void CSPDirectiveList::parseReportURI(const String& name, const String& value)
{
    if (!m_reportURIs.isEmpty()) {
        m_policy->reportDuplicateDirective(name);
        return;
    }

    Vector<UChar> characters;
    value.appendTo(characters);

    const UChar* position = characters.data();
    const UChar* end = position + characters.size();

    while (position < end) {
        skipWhile<UChar, isASCIISpace>(position, end);

        const UChar* urlBegin = position;
        skipWhile<UChar, isNotASCIISpace>(position, end);

        if (urlBegin < position) {
            String url = String(urlBegin, position - urlBegin);
            m_reportURIs.append(m_policy->completeURL(url));
        }
    }
}


template<class CSPDirectiveType>
void CSPDirectiveList::setCSPDirective(const String& name, const String& value, OwnPtr<CSPDirectiveType>& directive)
{
    if (directive) {
        m_policy->reportDuplicateDirective(name);
        return;
    }
    directive = adoptPtr(new CSPDirectiveType(name, value, m_policy));
}

void CSPDirectiveList::applySandboxPolicy(const String& name, const String& sandboxPolicy)
{
    if (m_reportOnly) {
        m_policy->reportInvalidInReportOnly(name);
        return;
    }
    if (m_haveSandboxPolicy) {
        m_policy->reportDuplicateDirective(name);
        return;
    }
    m_haveSandboxPolicy = true;
    String invalidTokens;
    m_policy->enforceSandboxFlags(parseSandboxPolicy(sandboxPolicy, invalidTokens));
    if (!invalidTokens.isNull())
        m_policy->reportInvalidSandboxFlags(invalidTokens);
}

void CSPDirectiveList::parseReflectedXSS(const String& name, const String& value)
{
    if (m_reflectedXSSDisposition != ReflectedXSSUnset) {
        m_policy->reportDuplicateDirective(name);
        m_reflectedXSSDisposition = ReflectedXSSInvalid;
        return;
    }

    if (value.isEmpty()) {
        m_reflectedXSSDisposition = ReflectedXSSInvalid;
        m_policy->reportInvalidReflectedXSS(value);
        return;
    }

    Vector<UChar> characters;
    value.appendTo(characters);

    const UChar* position = characters.data();
    const UChar* end = position + characters.size();

    skipWhile<UChar, isASCIISpace>(position, end);
    const UChar* begin = position;
    skipWhile<UChar, isNotASCIISpace>(position, end);

    // value1
    //       ^
    if (equalIgnoringCase("allow", begin, position - begin)) {
        m_reflectedXSSDisposition = AllowReflectedXSS;
    } else if (equalIgnoringCase("filter", begin, position - begin)) {
        m_reflectedXSSDisposition = FilterReflectedXSS;
    } else if (equalIgnoringCase("block", begin, position - begin)) {
        m_reflectedXSSDisposition = BlockReflectedXSS;
    } else {
        m_reflectedXSSDisposition = ReflectedXSSInvalid;
        m_policy->reportInvalidReflectedXSS(value);
        return;
    }

    skipWhile<UChar, isASCIISpace>(position, end);
    if (position == end && m_reflectedXSSDisposition != ReflectedXSSUnset)
        return;

    // value1 value2
    //        ^
    m_reflectedXSSDisposition = ReflectedXSSInvalid;
    m_policy->reportInvalidReflectedXSS(value);
}

void CSPDirectiveList::parseReferrer(const String& name, const String& value)
{
    if (m_didSetReferrerPolicy) {
        m_policy->reportDuplicateDirective(name);
        m_referrerPolicy = ReferrerPolicyNever;
        return;
    }

    m_didSetReferrerPolicy = true;

    if (value.isEmpty()) {
        m_policy->reportInvalidReferrer(value);
        m_referrerPolicy = ReferrerPolicyNever;
        return;
    }

    Vector<UChar> characters;
    value.appendTo(characters);

    const UChar* position = characters.data();
    const UChar* end = position + characters.size();

    skipWhile<UChar, isASCIISpace>(position, end);
    const UChar* begin = position;
    skipWhile<UChar, isNotASCIISpace>(position, end);

    // value1
    //       ^
    if (equalIgnoringCase("always", begin, position - begin)) {
        m_referrerPolicy = ReferrerPolicyAlways;
    } else if (equalIgnoringCase("default", begin, position - begin)) {
        m_referrerPolicy = ReferrerPolicyDefault;
    } else if (equalIgnoringCase("never", begin, position - begin)) {
        m_referrerPolicy = ReferrerPolicyNever;
    } else if (equalIgnoringCase("origin", begin, position - begin)) {
        m_referrerPolicy = ReferrerPolicyOrigin;
    } else {
        m_referrerPolicy = ReferrerPolicyNever;
        m_policy->reportInvalidReferrer(value);
        return;
    }

    skipWhile<UChar, isASCIISpace>(position, end);
    if (position == end)
        return;

    // value1 value2
    //        ^
    m_referrerPolicy = ReferrerPolicyNever;
    m_policy->reportInvalidReferrer(value);

}

void CSPDirectiveList::addDirective(const String& name, const String& value)
{
    ASSERT(!name.isEmpty());

    if (equalIgnoringCase(name, ContentSecurityPolicy::DefaultSrc)) {
        setCSPDirective<SourceListDirective>(name, value, m_defaultSrc);
    } else if (equalIgnoringCase(name, ContentSecurityPolicy::ScriptSrc)) {
        setCSPDirective<SourceListDirective>(name, value, m_scriptSrc);
        m_policy->usesScriptHashAlgorithms(m_scriptSrc->hashAlgorithmsUsed());
    } else if (equalIgnoringCase(name, ContentSecurityPolicy::ObjectSrc)) {
        setCSPDirective<SourceListDirective>(name, value, m_objectSrc);
    } else if (equalIgnoringCase(name, ContentSecurityPolicy::FrameAncestors)) {
        setCSPDirective<SourceListDirective>(name, value, m_frameAncestors);
    } else if (equalIgnoringCase(name, ContentSecurityPolicy::FrameSrc)) {
        setCSPDirective<SourceListDirective>(name, value, m_frameSrc);
    } else if (equalIgnoringCase(name, ContentSecurityPolicy::ImgSrc)) {
        setCSPDirective<SourceListDirective>(name, value, m_imgSrc);
    } else if (equalIgnoringCase(name, ContentSecurityPolicy::StyleSrc)) {
        setCSPDirective<SourceListDirective>(name, value, m_styleSrc);
        m_policy->usesStyleHashAlgorithms(m_styleSrc->hashAlgorithmsUsed());
    } else if (equalIgnoringCase(name, ContentSecurityPolicy::FontSrc)) {
        setCSPDirective<SourceListDirective>(name, value, m_fontSrc);
    } else if (equalIgnoringCase(name, ContentSecurityPolicy::MediaSrc)) {
        setCSPDirective<SourceListDirective>(name, value, m_mediaSrc);
    } else if (equalIgnoringCase(name, ContentSecurityPolicy::ConnectSrc)) {
        setCSPDirective<SourceListDirective>(name, value, m_connectSrc);
    } else if (equalIgnoringCase(name, ContentSecurityPolicy::Sandbox)) {
        applySandboxPolicy(name, value);
    } else if (equalIgnoringCase(name, ContentSecurityPolicy::ReportURI)) {
        parseReportURI(name, value);
    } else if (m_policy->experimentalFeaturesEnabled()) {
        if (equalIgnoringCase(name, ContentSecurityPolicy::BaseURI))
            setCSPDirective<SourceListDirective>(name, value, m_baseURI);
        else if (equalIgnoringCase(name, ContentSecurityPolicy::ChildSrc))
            setCSPDirective<SourceListDirective>(name, value, m_childSrc);
        else if (equalIgnoringCase(name, ContentSecurityPolicy::FormAction))
            setCSPDirective<SourceListDirective>(name, value, m_formAction);
        else if (equalIgnoringCase(name, ContentSecurityPolicy::PluginTypes))
            setCSPDirective<MediaListDirective>(name, value, m_pluginTypes);
        else if (equalIgnoringCase(name, ContentSecurityPolicy::ReflectedXSS))
            parseReflectedXSS(name, value);
        else if (equalIgnoringCase(name, ContentSecurityPolicy::Referrer))
            parseReferrer(name, value);
        else
            m_policy->reportUnsupportedDirective(name);
    } else {
        m_policy->reportUnsupportedDirective(name);
    }
}


} // namespace WebCore
