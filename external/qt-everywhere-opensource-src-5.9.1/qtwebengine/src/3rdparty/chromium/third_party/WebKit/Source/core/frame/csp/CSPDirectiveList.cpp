// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/csp/CSPDirectiveList.h"

#include "bindings/core/v8/SourceLocation.h"
#include "core/dom/Document.h"
#include "core/dom/SecurityContext.h"
#include "core/dom/SpaceSplitString.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLScriptElement.h"
#include "core/inspector/ConsoleMessage.h"
#include "platform/Crypto.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/weborigin/KURL.h"
#include "wtf/text/Base64.h"
#include "wtf/text/ParsingUtilities.h"
#include "wtf/text/StringUTF8Adaptor.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace {

String getSha256String(const String& content) {
  DigestValue digest;
  StringUTF8Adaptor utf8Content(content);
  bool digestSuccess = computeDigest(HashAlgorithmSha256, utf8Content.data(),
                                     utf8Content.length(), digest);
  if (!digestSuccess) {
    return "sha256-...";
  }

  return "sha256-" + base64Encode(reinterpret_cast<char*>(digest.data()),
                                  digest.size(), Base64DoNotInsertLFs);
}

template <typename CharType>
inline bool isASCIIAlphanumericOrHyphen(CharType c) {
  return isASCIIAlphanumeric(c) || c == '-';
}

}  // namespace

CSPDirectiveList::CSPDirectiveList(ContentSecurityPolicy* policy,
                                   ContentSecurityPolicyHeaderType type,
                                   ContentSecurityPolicyHeaderSource source)
    : m_policy(policy),
      m_headerType(type),
      m_headerSource(source),
      m_hasSandboxPolicy(false),
      m_strictMixedContentCheckingEnforced(false),
      m_upgradeInsecureRequests(false),
      m_treatAsPublicAddress(false),
      m_requireSRIFor(RequireSRIForToken::None) {}

CSPDirectiveList* CSPDirectiveList::create(
    ContentSecurityPolicy* policy,
    const UChar* begin,
    const UChar* end,
    ContentSecurityPolicyHeaderType type,
    ContentSecurityPolicyHeaderSource source) {
  CSPDirectiveList* directives = new CSPDirectiveList(policy, type, source);
  directives->parse(begin, end);

  if (!directives->checkEval(
          directives->operativeDirective(directives->m_scriptSrc.get()))) {
    String message =
        "Refused to evaluate a string as JavaScript because 'unsafe-eval' is "
        "not an allowed source of script in the following Content Security "
        "Policy directive: \"" +
        directives->operativeDirective(directives->m_scriptSrc.get())->text() +
        "\".\n";
    directives->setEvalDisabledErrorMessage(message);
  }

  if (directives->isReportOnly() &&
      source != ContentSecurityPolicyHeaderSourceMeta &&
      directives->reportEndpoints().isEmpty())
    policy->reportMissingReportURI(String(begin, end - begin));

  return directives;
}

void CSPDirectiveList::reportViolation(
    const String& directiveText,
    const String& effectiveDirective,
    const String& consoleMessage,
    const KURL& blockedURL,
    ResourceRequest::RedirectStatus redirectStatus) const {
  String message =
      isReportOnly() ? "[Report Only] " + consoleMessage : consoleMessage;
  m_policy->logToConsole(ConsoleMessage::create(SecurityMessageSource,
                                                ErrorMessageLevel, message));
  m_policy->reportViolation(directiveText, effectiveDirective, message,
                            blockedURL, m_reportEndpoints, m_header,
                            m_headerType, ContentSecurityPolicy::URLViolation,
                            nullptr, redirectStatus);
}

void CSPDirectiveList::reportViolationWithFrame(
    const String& directiveText,
    const String& effectiveDirective,
    const String& consoleMessage,
    const KURL& blockedURL,
    LocalFrame* frame) const {
  String message =
      isReportOnly() ? "[Report Only] " + consoleMessage : consoleMessage;
  m_policy->logToConsole(
      ConsoleMessage::create(SecurityMessageSource, ErrorMessageLevel, message),
      frame);
  m_policy->reportViolation(
      directiveText, effectiveDirective, message, blockedURL, m_reportEndpoints,
      m_header, m_headerType, ContentSecurityPolicy::URLViolation, frame);
}

void CSPDirectiveList::reportViolationWithLocation(
    const String& directiveText,
    const String& effectiveDirective,
    const String& consoleMessage,
    const KURL& blockedURL,
    const String& contextURL,
    const WTF::OrdinalNumber& contextLine,
    Element* element) const {
  String message =
      isReportOnly() ? "[Report Only] " + consoleMessage : consoleMessage;
  m_policy->logToConsole(ConsoleMessage::create(
      SecurityMessageSource, ErrorMessageLevel, message,
      SourceLocation::capture(contextURL, contextLine.oneBasedInt(), 0)));
  m_policy->reportViolation(
      directiveText, effectiveDirective, message, blockedURL, m_reportEndpoints,
      m_header, m_headerType, ContentSecurityPolicy::InlineViolation, nullptr,
      RedirectStatus::NoRedirect, contextLine.oneBasedInt(), element);
}

void CSPDirectiveList::reportViolationWithState(
    const String& directiveText,
    const String& effectiveDirective,
    const String& message,
    const KURL& blockedURL,
    ScriptState* scriptState,
    const ContentSecurityPolicy::ExceptionStatus exceptionStatus) const {
  String reportMessage = isReportOnly() ? "[Report Only] " + message : message;
  // Print a console message if it won't be redundant with a
  // JavaScript exception that the caller will throw. (Exceptions will
  // never get thrown in report-only mode because the caller won't see
  // a violation.)
  if (isReportOnly() ||
      exceptionStatus == ContentSecurityPolicy::WillNotThrowException) {
    ConsoleMessage* consoleMessage = ConsoleMessage::create(
        SecurityMessageSource, ErrorMessageLevel, reportMessage);
    m_policy->logToConsole(consoleMessage);
  }
  m_policy->reportViolation(directiveText, effectiveDirective, message,
                            blockedURL, m_reportEndpoints, m_header,
                            m_headerType, ContentSecurityPolicy::EvalViolation);
}

bool CSPDirectiveList::checkEval(SourceListDirective* directive) const {
  return !directive || directive->allowEval();
}

bool CSPDirectiveList::checkInline(SourceListDirective* directive) const {
  return !directive ||
         (directive->allowInline() && !directive->isHashOrNoncePresent());
}

bool CSPDirectiveList::isMatchingNoncePresent(SourceListDirective* directive,
                                              const String& nonce) const {
  return directive && directive->allowNonce(nonce);
}

bool CSPDirectiveList::checkHash(SourceListDirective* directive,
                                 const CSPHashValue& hashValue) const {
  return !directive || directive->allowHash(hashValue);
}

bool CSPDirectiveList::checkHashedAttributes(
    SourceListDirective* directive) const {
  return !directive || directive->allowHashedAttributes();
}

bool CSPDirectiveList::checkDynamic(SourceListDirective* directive) const {
  return !directive || directive->allowDynamic();
}

void CSPDirectiveList::reportMixedContent(
    const KURL& mixedURL,
    ResourceRequest::RedirectStatus redirectStatus) const {
  if (strictMixedContentChecking()) {
    m_policy->reportViolation(ContentSecurityPolicy::BlockAllMixedContent,
                              ContentSecurityPolicy::BlockAllMixedContent,
                              String(), mixedURL, m_reportEndpoints, m_header,
                              m_headerType, ContentSecurityPolicy::URLViolation,
                              nullptr, redirectStatus);
  }
}

bool CSPDirectiveList::checkSource(
    SourceListDirective* directive,
    const KURL& url,
    ResourceRequest::RedirectStatus redirectStatus) const {
  // If |url| is empty, fall back to the policy URL to ensure that <object>'s
  // without a `src` can be blocked/allowed, as they can still load plugins
  // even though they don't actually have a URL.
  return !directive ||
         directive->allows(url.isEmpty() ? m_policy->url() : url,
                           redirectStatus);
}

bool CSPDirectiveList::checkAncestors(SourceListDirective* directive,
                                      LocalFrame* frame) const {
  if (!frame || !directive)
    return true;

  for (Frame* current = frame->tree().parent(); current;
       current = current->tree().parent()) {
    // The |current| frame might be a remote frame which has no URL, so use
    // its origin instead.  This should suffice for this check since it
    // doesn't do path comparisons.  See https://crbug.com/582544.
    //
    // TODO(mkwst): Move this check up into the browser process.  See
    // https://crbug.com/555418.
    KURL url(KURL(),
             current->securityContext()->getSecurityOrigin()->toString());
    if (!directive->allows(url, ResourceRequest::RedirectStatus::NoRedirect))
      return false;
  }
  return true;
}

bool CSPDirectiveList::checkRequestWithoutIntegrity(
    WebURLRequest::RequestContext context) const {
  if (m_requireSRIFor == RequireSRIForToken::None)
    return true;
  // SRI specification
  // (https://w3c.github.io/webappsec-subresource-integrity/#apply-algorithm-to-request)
  // says to match token with request's destination with the token.
  // Keep this logic aligned with ContentSecurityPolicy::allowRequest
  if ((m_requireSRIFor & RequireSRIForToken::Script) &&
      (context == WebURLRequest::RequestContextScript ||
       context == WebURLRequest::RequestContextImport ||
       context == WebURLRequest::RequestContextServiceWorker ||
       context == WebURLRequest::RequestContextSharedWorker ||
       context == WebURLRequest::RequestContextWorker)) {
    return false;
  }
  if ((m_requireSRIFor & RequireSRIForToken::Style) &&
      context == WebURLRequest::RequestContextStyle)
    return false;
  return true;
}

bool CSPDirectiveList::checkRequestWithoutIntegrityAndReportViolation(
    WebURLRequest::RequestContext context,
    const KURL& url,
    ResourceRequest::RedirectStatus redirectStatus) const {
  if (checkRequestWithoutIntegrity(context))
    return true;
  String resourceType;
  switch (context) {
    case WebURLRequest::RequestContextScript:
    case WebURLRequest::RequestContextImport:
      resourceType = "script";
      break;
    case WebURLRequest::RequestContextStyle:
      resourceType = "stylesheet";
      break;
    case WebURLRequest::RequestContextServiceWorker:
      resourceType = "service worker";
      break;
    case WebURLRequest::RequestContextSharedWorker:
      resourceType = "shared worker";
      break;
    case WebURLRequest::RequestContextWorker:
      resourceType = "worker";
      break;
    default:
      break;
  }

  reportViolation(ContentSecurityPolicy::RequireSRIFor,
                  ContentSecurityPolicy::RequireSRIFor,
                  "Refused to load the " + resourceType + " '" +
                      url.elidedString() +
                      "' because 'require-sri-for' directive requires "
                      "integrity attribute be present for all " +
                      resourceType + "s.",
                  url, redirectStatus);
  return denyIfEnforcingPolicy();
}

bool CSPDirectiveList::allowRequestWithoutIntegrity(
    WebURLRequest::RequestContext context,
    const KURL& url,
    ResourceRequest::RedirectStatus redirectStatus,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  if (reportingStatus == ContentSecurityPolicy::SendReport)
    return checkRequestWithoutIntegrityAndReportViolation(context, url,
                                                          redirectStatus);
  return denyIfEnforcingPolicy() || checkRequestWithoutIntegrity(context);
}

bool CSPDirectiveList::checkMediaType(MediaListDirective* directive,
                                      const String& type,
                                      const String& typeAttribute) const {
  if (!directive)
    return true;
  if (typeAttribute.isEmpty() || typeAttribute.stripWhiteSpace() != type)
    return false;
  return directive->allows(type);
}

SourceListDirective* CSPDirectiveList::operativeDirective(
    SourceListDirective* directive) const {
  return directive ? directive : m_defaultSrc.get();
}

SourceListDirective* CSPDirectiveList::operativeDirective(
    SourceListDirective* directive,
    SourceListDirective* override) const {
  return directive ? directive : override;
}

bool CSPDirectiveList::checkEvalAndReportViolation(
    SourceListDirective* directive,
    const String& consoleMessage,
    ScriptState* scriptState,
    ContentSecurityPolicy::ExceptionStatus exceptionStatus) const {
  if (checkEval(directive))
    return true;

  String suffix = String();
  if (directive == m_defaultSrc)
    suffix =
        " Note that 'script-src' was not explicitly set, so 'default-src' is "
        "used as a fallback.";

  reportViolationWithState(
      directive->text(), ContentSecurityPolicy::ScriptSrc,
      consoleMessage + "\"" + directive->text() + "\"." + suffix + "\n", KURL(),
      scriptState, exceptionStatus);
  if (!isReportOnly()) {
    m_policy->reportBlockedScriptExecutionToInspector(directive->text());
    return false;
  }
  return true;
}

bool CSPDirectiveList::checkMediaTypeAndReportViolation(
    MediaListDirective* directive,
    const String& type,
    const String& typeAttribute,
    const String& consoleMessage) const {
  if (checkMediaType(directive, type, typeAttribute))
    return true;

  String message = consoleMessage + "\'" + directive->text() + "\'.";
  if (typeAttribute.isEmpty())
    message = message +
              " When enforcing the 'plugin-types' directive, the plugin's "
              "media type must be explicitly declared with a 'type' attribute "
              "on the containing element (e.g. '<object type=\"[TYPE GOES "
              "HERE]\" ...>').";

  // 'RedirectStatus::NoRedirect' is safe here, as we do the media type check
  // before actually loading data; this means that we shouldn't leak redirect
  // targets, as we won't have had a chance to redirect yet.
  reportViolation(directive->text(), ContentSecurityPolicy::PluginTypes,
                  message + "\n", KURL(),
                  ResourceRequest::RedirectStatus::NoRedirect);
  return denyIfEnforcingPolicy();
}

bool CSPDirectiveList::checkInlineAndReportViolation(
    SourceListDirective* directive,
    const String& consoleMessage,
    Element* element,
    const String& contextURL,
    const WTF::OrdinalNumber& contextLine,
    bool isScript,
    const String& hashValue) const {
  if (checkInline(directive))
    return true;

  String suffix = String();
  if (directive->allowInline() && directive->isHashOrNoncePresent()) {
    // If inline is allowed, but a hash or nonce is present, we ignore
    // 'unsafe-inline'. Throw a reasonable error.
    suffix =
        " Note that 'unsafe-inline' is ignored if either a hash or nonce value "
        "is present in the source list.";
  } else {
    suffix =
        " Either the 'unsafe-inline' keyword, a hash ('" + hashValue +
        "'), or a nonce ('nonce-...') is required to enable inline execution.";
    if (directive == m_defaultSrc)
      suffix = suffix + " Note also that '" +
               String(isScript ? "script" : "style") +
               "-src' was not explicitly set, so 'default-src' is used as a "
               "fallback.";
  }

  reportViolationWithLocation(
      directive->text(), isScript ? ContentSecurityPolicy::ScriptSrc
                                  : ContentSecurityPolicy::StyleSrc,
      consoleMessage + "\"" + directive->text() + "\"." + suffix + "\n", KURL(),
      contextURL, contextLine, element);

  if (!isReportOnly()) {
    if (isScript)
      m_policy->reportBlockedScriptExecutionToInspector(directive->text());
    return false;
  }
  return true;
}

bool CSPDirectiveList::checkSourceAndReportViolation(
    SourceListDirective* directive,
    const KURL& url,
    const String& effectiveDirective,
    ResourceRequest::RedirectStatus redirectStatus) const {
  if (!directive)
    return true;

  // We ignore URL-based whitelists if we're allowing dynamic script injection.
  if (checkSource(directive, url, redirectStatus) && !checkDynamic(directive))
    return true;

  // We should never have a violation against `child-src` or `default-src`
  // directly; the effective directive should always be one of the explicit
  // fetch directives.
  DCHECK_NE(ContentSecurityPolicy::ChildSrc, effectiveDirective);
  DCHECK_NE(ContentSecurityPolicy::DefaultSrc, effectiveDirective);

  String prefix;
  if (ContentSecurityPolicy::BaseURI == effectiveDirective)
    prefix = "Refused to set the document's base URI to '";
  else if (ContentSecurityPolicy::WorkerSrc == effectiveDirective)
    prefix = "Refused to create a worker from '";
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
  else if (ContentSecurityPolicy::ManifestSrc == effectiveDirective)
    prefix = "Refused to load manifest from '";
  else if (ContentSecurityPolicy::ObjectSrc == effectiveDirective)
    prefix = "Refused to load plugin data from '";
  else if (ContentSecurityPolicy::ScriptSrc == effectiveDirective)
    prefix = "Refused to load the script '";
  else if (ContentSecurityPolicy::StyleSrc == effectiveDirective)
    prefix = "Refused to load the stylesheet '";

  String suffix = String();
  if (checkDynamic(directive))
    suffix =
        " 'strict-dynamic' is present, so host-based whitelisting is disabled.";
  if (directive == m_defaultSrc)
    suffix =
        suffix + " Note that '" + effectiveDirective +
        "' was not explicitly set, so 'default-src' is used as a fallback.";

  reportViolation(directive->text(), effectiveDirective,
                  prefix + url.elidedString() +
                      "' because it violates the following Content Security "
                      "Policy directive: \"" +
                      directive->text() + "\"." + suffix + "\n",
                  url, redirectStatus);
  return denyIfEnforcingPolicy();
}

bool CSPDirectiveList::checkAncestorsAndReportViolation(
    SourceListDirective* directive,
    LocalFrame* frame,
    const KURL& url) const {
  if (checkAncestors(directive, frame))
    return true;

  reportViolationWithFrame(directive->text(), "frame-ancestors",
                           "Refused to display '" + url.elidedString() +
                               "' in a frame because an ancestor violates the "
                               "following Content Security Policy directive: "
                               "\"" +
                               directive->text() + "\".",
                           url, frame);
  return denyIfEnforcingPolicy();
}

bool CSPDirectiveList::allowJavaScriptURLs(
    Element* element,
    const String& contextURL,
    const WTF::OrdinalNumber& contextLine,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  if (reportingStatus == ContentSecurityPolicy::SendReport) {
    return checkInlineAndReportViolation(
        operativeDirective(m_scriptSrc.get()),
        "Refused to execute JavaScript URL because it violates the following "
        "Content Security Policy directive: ",
        element, contextURL, contextLine, true, "sha256-...");
  }
  return checkInline(operativeDirective(m_scriptSrc.get()));
}

bool CSPDirectiveList::allowInlineEventHandlers(
    Element* element,
    const String& contextURL,
    const WTF::OrdinalNumber& contextLine,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  if (reportingStatus == ContentSecurityPolicy::SendReport) {
    return checkInlineAndReportViolation(
        operativeDirective(m_scriptSrc.get()),
        "Refused to execute inline event handler because it violates the "
        "following Content Security Policy directive: ",
        element, contextURL, contextLine, true, "sha256-...");
  }
  return checkInline(operativeDirective(m_scriptSrc.get()));
}

bool CSPDirectiveList::allowInlineScript(
    Element* element,
    const String& contextURL,
    const String& nonce,
    const WTF::OrdinalNumber& contextLine,
    ContentSecurityPolicy::ReportingStatus reportingStatus,
    const String& content) const {
  if (isMatchingNoncePresent(operativeDirective(m_scriptSrc.get()), nonce))
    return true;
  if (element && isHTMLScriptElement(element) &&
      !toHTMLScriptElement(element)->loader()->isParserInserted() &&
      allowDynamic()) {
    return true;
  }
  if (reportingStatus == ContentSecurityPolicy::SendReport) {
    return checkInlineAndReportViolation(
        operativeDirective(m_scriptSrc.get()),
        "Refused to execute inline script because it violates the following "
        "Content Security Policy directive: ",
        element, contextURL, contextLine, true, getSha256String(content));
  }
  return checkInline(operativeDirective(m_scriptSrc.get()));
}

bool CSPDirectiveList::allowInlineStyle(
    Element* element,
    const String& contextURL,
    const String& nonce,
    const WTF::OrdinalNumber& contextLine,
    ContentSecurityPolicy::ReportingStatus reportingStatus,
    const String& content) const {
  if (isMatchingNoncePresent(operativeDirective(m_styleSrc.get()), nonce))
    return true;
  if (reportingStatus == ContentSecurityPolicy::SendReport) {
    return checkInlineAndReportViolation(
        operativeDirective(m_styleSrc.get()),
        "Refused to apply inline style because it violates the following "
        "Content Security Policy directive: ",
        element, contextURL, contextLine, false, getSha256String(content));
  }
  return checkInline(operativeDirective(m_styleSrc.get()));
}

bool CSPDirectiveList::allowEval(
    ScriptState* scriptState,
    ContentSecurityPolicy::ReportingStatus reportingStatus,
    ContentSecurityPolicy::ExceptionStatus exceptionStatus) const {
  if (reportingStatus == ContentSecurityPolicy::SendReport) {
    return checkEvalAndReportViolation(
        operativeDirective(m_scriptSrc.get()),
        "Refused to evaluate a string as JavaScript because 'unsafe-eval' is "
        "not an allowed source of script in the following Content Security "
        "Policy directive: ",
        scriptState, exceptionStatus);
  }
  return checkEval(operativeDirective(m_scriptSrc.get()));
}

bool CSPDirectiveList::allowPluginType(
    const String& type,
    const String& typeAttribute,
    const KURL& url,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  return reportingStatus == ContentSecurityPolicy::SendReport
             ? checkMediaTypeAndReportViolation(
                   m_pluginTypes.get(), type, typeAttribute,
                   "Refused to load '" + url.elidedString() + "' (MIME type '" +
                       typeAttribute +
                       "') because it violates the following Content Security "
                       "Policy Directive: ")
             : checkMediaType(m_pluginTypes.get(), type, typeAttribute);
}

bool CSPDirectiveList::allowScriptFromSource(
    const KURL& url,
    const String& nonce,
    ParserDisposition parserDisposition,
    ResourceRequest::RedirectStatus redirectStatus,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  if (isMatchingNoncePresent(operativeDirective(m_scriptSrc.get()), nonce))
    return true;
  if (parserDisposition == NotParserInserted && allowDynamic())
    return true;
  return reportingStatus == ContentSecurityPolicy::SendReport
             ? checkSourceAndReportViolation(
                   operativeDirective(m_scriptSrc.get()), url,
                   ContentSecurityPolicy::ScriptSrc, redirectStatus)
             : checkSource(operativeDirective(m_scriptSrc.get()), url,
                           redirectStatus);
}

bool CSPDirectiveList::allowObjectFromSource(
    const KURL& url,
    ResourceRequest::RedirectStatus redirectStatus,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  if (url.protocolIsAbout())
    return true;
  return reportingStatus == ContentSecurityPolicy::SendReport
             ? checkSourceAndReportViolation(
                   operativeDirective(m_objectSrc.get()), url,
                   ContentSecurityPolicy::ObjectSrc, redirectStatus)
             : checkSource(operativeDirective(m_objectSrc.get()), url,
                           redirectStatus);
}

bool CSPDirectiveList::allowFrameFromSource(
    const KURL& url,
    ResourceRequest::RedirectStatus redirectStatus,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  if (url.protocolIsAbout())
    return true;

  // 'frame-src' overrides 'child-src', which overrides the default
  // sources. So, we do this nested set of calls to 'operativeDirective()' to
  // grab 'frame-src' if it exists, 'child-src' if it doesn't, and 'defaut-src'
  // if neither are available.
  SourceListDirective* whichDirective = operativeDirective(
      m_frameSrc.get(), operativeDirective(m_childSrc.get()));

  return reportingStatus == ContentSecurityPolicy::SendReport
             ? checkSourceAndReportViolation(whichDirective, url,
                                             ContentSecurityPolicy::FrameSrc,
                                             redirectStatus)
             : checkSource(whichDirective, url, redirectStatus);
}

bool CSPDirectiveList::allowImageFromSource(
    const KURL& url,
    ResourceRequest::RedirectStatus redirectStatus,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  return reportingStatus == ContentSecurityPolicy::SendReport
             ? checkSourceAndReportViolation(operativeDirective(m_imgSrc.get()),
                                             url, ContentSecurityPolicy::ImgSrc,
                                             redirectStatus)
             : checkSource(operativeDirective(m_imgSrc.get()), url,
                           redirectStatus);
}

bool CSPDirectiveList::allowStyleFromSource(
    const KURL& url,
    const String& nonce,
    ResourceRequest::RedirectStatus redirectStatus,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  if (isMatchingNoncePresent(operativeDirective(m_styleSrc.get()), nonce))
    return true;
  return reportingStatus == ContentSecurityPolicy::SendReport
             ? checkSourceAndReportViolation(
                   operativeDirective(m_styleSrc.get()), url,
                   ContentSecurityPolicy::StyleSrc, redirectStatus)
             : checkSource(operativeDirective(m_styleSrc.get()), url,
                           redirectStatus);
}

bool CSPDirectiveList::allowFontFromSource(
    const KURL& url,
    ResourceRequest::RedirectStatus redirectStatus,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  return reportingStatus == ContentSecurityPolicy::SendReport
             ? checkSourceAndReportViolation(
                   operativeDirective(m_fontSrc.get()), url,
                   ContentSecurityPolicy::FontSrc, redirectStatus)
             : checkSource(operativeDirective(m_fontSrc.get()), url,
                           redirectStatus);
}

bool CSPDirectiveList::allowMediaFromSource(
    const KURL& url,
    ResourceRequest::RedirectStatus redirectStatus,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  return reportingStatus == ContentSecurityPolicy::SendReport
             ? checkSourceAndReportViolation(
                   operativeDirective(m_mediaSrc.get()), url,
                   ContentSecurityPolicy::MediaSrc, redirectStatus)
             : checkSource(operativeDirective(m_mediaSrc.get()), url,
                           redirectStatus);
}

bool CSPDirectiveList::allowManifestFromSource(
    const KURL& url,
    ResourceRequest::RedirectStatus redirectStatus,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  return reportingStatus == ContentSecurityPolicy::SendReport
             ? checkSourceAndReportViolation(
                   operativeDirective(m_manifestSrc.get()), url,
                   ContentSecurityPolicy::ManifestSrc, redirectStatus)
             : checkSource(operativeDirective(m_manifestSrc.get()), url,
                           redirectStatus);
}

bool CSPDirectiveList::allowConnectToSource(
    const KURL& url,
    ResourceRequest::RedirectStatus redirectStatus,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  return reportingStatus == ContentSecurityPolicy::SendReport
             ? checkSourceAndReportViolation(
                   operativeDirective(m_connectSrc.get()), url,
                   ContentSecurityPolicy::ConnectSrc, redirectStatus)
             : checkSource(operativeDirective(m_connectSrc.get()), url,
                           redirectStatus);
}

bool CSPDirectiveList::allowFormAction(
    const KURL& url,
    ResourceRequest::RedirectStatus redirectStatus,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  return reportingStatus == ContentSecurityPolicy::SendReport
             ? checkSourceAndReportViolation(m_formAction.get(), url,
                                             ContentSecurityPolicy::FormAction,
                                             redirectStatus)
             : checkSource(m_formAction.get(), url, redirectStatus);
}

bool CSPDirectiveList::allowBaseURI(
    const KURL& url,
    ResourceRequest::RedirectStatus redirectStatus,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  return reportingStatus == ContentSecurityPolicy::SendReport
             ? checkSourceAndReportViolation(m_baseURI.get(), url,
                                             ContentSecurityPolicy::BaseURI,
                                             redirectStatus)
             : checkSource(m_baseURI.get(), url, redirectStatus);
}

bool CSPDirectiveList::allowWorkerFromSource(
    const KURL& url,
    ResourceRequest::RedirectStatus redirectStatus,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  // 'worker-src' overrides 'child-src', which overrides the default
  // sources. So, we do this nested set of calls to 'operativeDirective()' to
  // grab 'worker-src' if it exists, 'child-src' if it doesn't, and 'defaut-src'
  // if neither are available.
  SourceListDirective* whichDirective = operativeDirective(
      m_workerSrc.get(), operativeDirective(m_childSrc.get()));

  return reportingStatus == ContentSecurityPolicy::SendReport
             ? checkSourceAndReportViolation(whichDirective, url,
                                             ContentSecurityPolicy::WorkerSrc,
                                             redirectStatus)
             : checkSource(whichDirective, url, redirectStatus);
}

bool CSPDirectiveList::allowAncestors(
    LocalFrame* frame,
    const KURL& url,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  return reportingStatus == ContentSecurityPolicy::SendReport
             ? checkAncestorsAndReportViolation(m_frameAncestors.get(), frame,
                                                url)
             : checkAncestors(m_frameAncestors.get(), frame);
}

bool CSPDirectiveList::allowScriptHash(
    const CSPHashValue& hashValue,
    ContentSecurityPolicy::InlineType type) const {
  if (type == ContentSecurityPolicy::InlineType::Attribute) {
    if (!m_policy->experimentalFeaturesEnabled())
      return false;
    if (!checkHashedAttributes(operativeDirective(m_scriptSrc.get())))
      return false;
  }
  return checkHash(operativeDirective(m_scriptSrc.get()), hashValue);
}

bool CSPDirectiveList::allowStyleHash(
    const CSPHashValue& hashValue,
    ContentSecurityPolicy::InlineType type) const {
  if (type != ContentSecurityPolicy::InlineType::Block)
    return false;
  return checkHash(operativeDirective(m_styleSrc.get()), hashValue);
}

bool CSPDirectiveList::allowDynamic() const {
  return checkDynamic(operativeDirective(m_scriptSrc.get()));
}

const String& CSPDirectiveList::pluginTypesText() const {
  ASSERT(hasPluginTypes());
  return m_pluginTypes->text();
}

bool CSPDirectiveList::shouldSendCSPHeader(Resource::Type type) const {
  // TODO(mkwst): Revisit this once the CORS prefetch issue with the 'CSP'
  //              header is worked out, one way or another:
  //              https://github.com/whatwg/fetch/issues/52
  return false;
}

// policy            = directive-list
// directive-list    = [ directive *( ";" [ directive ] ) ]
//
void CSPDirectiveList::parse(const UChar* begin, const UChar* end) {
  m_header = String(begin, end - begin).stripWhiteSpace();

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
bool CSPDirectiveList::parseDirective(const UChar* begin,
                                      const UChar* end,
                                      String& name,
                                      String& value) {
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
    m_policy->reportUnsupportedDirective(
        String(nameBegin, position - nameBegin));
    return false;
  }

  name = String(nameBegin, position - nameBegin);

  if (position == end)
    return true;

  if (!skipExactly<UChar, isASCIISpace>(position, end)) {
    skipWhile<UChar, isNotASCIISpace>(position, end);
    m_policy->reportUnsupportedDirective(
        String(nameBegin, position - nameBegin));
    return false;
  }

  skipWhile<UChar, isASCIISpace>(position, end);

  const UChar* valueBegin = position;
  skipWhile<UChar, isCSPDirectiveValueCharacter>(position, end);

  if (position != end) {
    m_policy->reportInvalidDirectiveValueCharacter(
        name, String(valueBegin, end - valueBegin));
    return false;
  }

  // The directive-value may be empty.
  if (valueBegin == position)
    return true;

  value = String(valueBegin, position - valueBegin);
  return true;
}

void CSPDirectiveList::parseRequireSRIFor(const String& name,
                                          const String& value) {
  if (m_requireSRIFor != 0) {
    m_policy->reportDuplicateDirective(name);
    return;
  }
  StringBuilder tokenErrors;
  unsigned numberOfTokenErrors = 0;
  Vector<UChar> characters;
  value.appendTo(characters);

  const UChar* position = characters.data();
  const UChar* end = position + characters.size();

  while (position < end) {
    skipWhile<UChar, isASCIISpace>(position, end);

    const UChar* tokenBegin = position;
    skipWhile<UChar, isNotASCIISpace>(position, end);

    if (tokenBegin < position) {
      String token = String(tokenBegin, position - tokenBegin);
      if (equalIgnoringCase(token, "script")) {
        m_requireSRIFor |= RequireSRIForToken::Script;
      } else if (equalIgnoringCase(token, "style")) {
        m_requireSRIFor |= RequireSRIForToken::Style;
      } else {
        if (numberOfTokenErrors)
          tokenErrors.append(", \'");
        else
          tokenErrors.append('\'');
        tokenErrors.append(token);
        tokenErrors.append('\'');
        numberOfTokenErrors++;
      }
    }
  }

  if (numberOfTokenErrors == 0)
    return;

  String invalidTokensErrorMessage;
  if (numberOfTokenErrors > 1)
    tokenErrors.append(" are invalid 'require-sri-for' tokens.");
  else
    tokenErrors.append(" is an invalid 'require-sri-for' token.");

  invalidTokensErrorMessage = tokenErrors.toString();

  DCHECK(!invalidTokensErrorMessage.isEmpty());

  m_policy->reportInvalidRequireSRIForTokens(invalidTokensErrorMessage);
}

void CSPDirectiveList::parseReportURI(const String& name, const String& value) {
  if (!m_reportEndpoints.isEmpty()) {
    m_policy->reportDuplicateDirective(name);
    return;
  }

  // Remove report-uri in meta policies, per
  // https://www.w3.org/TR/CSP2/#delivery-html-meta-element.
  if (m_headerSource == ContentSecurityPolicyHeaderSourceMeta) {
    m_policy->reportInvalidDirectiveInMeta(name);
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
      m_reportEndpoints.append(url);
    }
  }
}

template <class CSPDirectiveType>
void CSPDirectiveList::setCSPDirective(const String& name,
                                       const String& value,
                                       Member<CSPDirectiveType>& directive) {
  if (directive) {
    m_policy->reportDuplicateDirective(name);
    return;
  }

  // Remove frame-ancestors directives in meta policies, per
  // https://www.w3.org/TR/CSP2/#delivery-html-meta-element.
  if (m_headerSource == ContentSecurityPolicyHeaderSourceMeta &&
      name == ContentSecurityPolicy::FrameAncestors) {
    m_policy->reportInvalidDirectiveInMeta(name);
    return;
  }

  directive = new CSPDirectiveType(name, value, m_policy);
}

void CSPDirectiveList::applySandboxPolicy(const String& name,
                                          const String& sandboxPolicy) {
  // Remove sandbox directives in meta policies, per
  // https://www.w3.org/TR/CSP2/#delivery-html-meta-element.
  if (m_headerSource == ContentSecurityPolicyHeaderSourceMeta) {
    m_policy->reportInvalidDirectiveInMeta(name);
    return;
  }
  if (isReportOnly()) {
    m_policy->reportInvalidInReportOnly(name);
    return;
  }
  if (m_hasSandboxPolicy) {
    m_policy->reportDuplicateDirective(name);
    return;
  }
  m_hasSandboxPolicy = true;
  String invalidTokens;
  SpaceSplitString policyTokens(AtomicString(sandboxPolicy),
                                SpaceSplitString::ShouldNotFoldCase);
  m_policy->enforceSandboxFlags(
      parseSandboxPolicy(policyTokens, invalidTokens));
  if (!invalidTokens.isNull())
    m_policy->reportInvalidSandboxFlags(invalidTokens);
}

void CSPDirectiveList::treatAsPublicAddress(const String& name,
                                            const String& value) {
  if (isReportOnly()) {
    m_policy->reportInvalidInReportOnly(name);
    return;
  }
  if (m_treatAsPublicAddress) {
    m_policy->reportDuplicateDirective(name);
    return;
  }
  m_treatAsPublicAddress = true;
  m_policy->treatAsPublicAddress();
  if (!value.isEmpty())
    m_policy->reportValueForEmptyDirective(name, value);
}

void CSPDirectiveList::enforceStrictMixedContentChecking(const String& name,
                                                         const String& value) {
  if (m_strictMixedContentCheckingEnforced) {
    m_policy->reportDuplicateDirective(name);
    return;
  }
  if (!value.isEmpty())
    m_policy->reportValueForEmptyDirective(name, value);

  m_strictMixedContentCheckingEnforced = true;

  if (!isReportOnly())
    m_policy->enforceStrictMixedContentChecking();
}

void CSPDirectiveList::enableInsecureRequestsUpgrade(const String& name,
                                                     const String& value) {
  if (isReportOnly()) {
    m_policy->reportInvalidInReportOnly(name);
    return;
  }
  if (m_upgradeInsecureRequests) {
    m_policy->reportDuplicateDirective(name);
    return;
  }
  m_upgradeInsecureRequests = true;

  m_policy->upgradeInsecureRequests();
  if (!value.isEmpty())
    m_policy->reportValueForEmptyDirective(name, value);
}

void CSPDirectiveList::addDirective(const String& name, const String& value) {
  ASSERT(!name.isEmpty());

  if (equalIgnoringCase(name, ContentSecurityPolicy::DefaultSrc)) {
    setCSPDirective<SourceListDirective>(name, value, m_defaultSrc);
    // TODO(mkwst) It seems unlikely that developers would use different
    // algorithms for scripts and styles. We may want to combine the
    // usesScriptHashAlgorithms() and usesStyleHashAlgorithms.
    m_policy->usesScriptHashAlgorithms(m_defaultSrc->hashAlgorithmsUsed());
    m_policy->usesStyleHashAlgorithms(m_defaultSrc->hashAlgorithmsUsed());
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
  } else if (equalIgnoringCase(name, ContentSecurityPolicy::BaseURI)) {
    setCSPDirective<SourceListDirective>(name, value, m_baseURI);
  } else if (equalIgnoringCase(name, ContentSecurityPolicy::ChildSrc)) {
    setCSPDirective<SourceListDirective>(name, value, m_childSrc);
  } else if (equalIgnoringCase(name, ContentSecurityPolicy::WorkerSrc) &&
             m_policy->experimentalFeaturesEnabled()) {
    setCSPDirective<SourceListDirective>(name, value, m_workerSrc);
  } else if (equalIgnoringCase(name, ContentSecurityPolicy::FormAction)) {
    setCSPDirective<SourceListDirective>(name, value, m_formAction);
  } else if (equalIgnoringCase(name, ContentSecurityPolicy::PluginTypes)) {
    setCSPDirective<MediaListDirective>(name, value, m_pluginTypes);
  } else if (equalIgnoringCase(
                 name, ContentSecurityPolicy::UpgradeInsecureRequests)) {
    enableInsecureRequestsUpgrade(name, value);
  } else if (equalIgnoringCase(name,
                               ContentSecurityPolicy::BlockAllMixedContent)) {
    enforceStrictMixedContentChecking(name, value);
  } else if (equalIgnoringCase(name, ContentSecurityPolicy::ManifestSrc)) {
    setCSPDirective<SourceListDirective>(name, value, m_manifestSrc);
  } else if (equalIgnoringCase(name,
                               ContentSecurityPolicy::TreatAsPublicAddress)) {
    treatAsPublicAddress(name, value);
  } else if (equalIgnoringCase(name, ContentSecurityPolicy::RequireSRIFor) &&
             m_policy->experimentalFeaturesEnabled()) {
    parseRequireSRIFor(name, value);
  } else {
    m_policy->reportUnsupportedDirective(name);
  }
}

DEFINE_TRACE(CSPDirectiveList) {
  visitor->trace(m_policy);
  visitor->trace(m_pluginTypes);
  visitor->trace(m_baseURI);
  visitor->trace(m_childSrc);
  visitor->trace(m_connectSrc);
  visitor->trace(m_defaultSrc);
  visitor->trace(m_fontSrc);
  visitor->trace(m_formAction);
  visitor->trace(m_frameAncestors);
  visitor->trace(m_frameSrc);
  visitor->trace(m_imgSrc);
  visitor->trace(m_mediaSrc);
  visitor->trace(m_manifestSrc);
  visitor->trace(m_objectSrc);
  visitor->trace(m_scriptSrc);
  visitor->trace(m_styleSrc);
  visitor->trace(m_workerSrc);
}

}  // namespace blink
