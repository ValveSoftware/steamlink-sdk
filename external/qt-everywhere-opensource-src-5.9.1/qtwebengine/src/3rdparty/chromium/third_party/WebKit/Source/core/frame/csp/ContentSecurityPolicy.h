/*
 * Copyright (C) 2011 Google, Inc. All rights reserved.
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

#ifndef ContentSecurityPolicy_h
#define ContentSecurityPolicy_h

#include "bindings/core/v8/ScriptState.h"
#include "core/CoreExport.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/SecurityContext.h"
#include "core/fetch/Resource.h"
#include "core/inspector/ConsoleTypes.h"
#include "platform/heap/Handle.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/network/HTTPParsers.h"
#include "platform/network/ResourceRequest.h"
#include "public/platform/WebInsecureRequestPolicy.h"
#include "wtf/HashSet.h"
#include "wtf/Vector.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/TextPosition.h"
#include "wtf/text/WTFString.h"
#include <memory>
#include <utility>

namespace WTF {
class OrdinalNumber;
}

namespace blink {

class ContentSecurityPolicyResponseHeaders;
class ConsoleMessage;
class CSPDirectiveList;
class CSPSource;
class Document;
class Element;
class FrameLoaderClient;
class KURL;
class ResourceRequest;
class SecurityOrigin;
class SecurityPolicyViolationEventInit;

typedef int SandboxFlags;
typedef HeapVector<Member<CSPDirectiveList>> CSPDirectiveListVector;
typedef HeapVector<Member<ConsoleMessage>> ConsoleMessageVector;
typedef std::pair<String, ContentSecurityPolicyHeaderType> CSPHeaderAndType;
using RedirectStatus = ResourceRequest::RedirectStatus;

class CORE_EXPORT ContentSecurityPolicy
    : public GarbageCollectedFinalized<ContentSecurityPolicy> {
 public:
  // CSP Level 1 Directives
  static const char ConnectSrc[];
  static const char DefaultSrc[];
  static const char FontSrc[];
  static const char FrameSrc[];
  static const char ImgSrc[];
  static const char MediaSrc[];
  static const char ObjectSrc[];
  static const char ReportURI[];
  static const char Sandbox[];
  static const char ScriptSrc[];
  static const char StyleSrc[];

  // CSP Level 2 Directives
  static const char BaseURI[];
  static const char ChildSrc[];
  static const char FormAction[];
  static const char FrameAncestors[];
  static const char PluginTypes[];

  // CSP Level 3 Directives
  static const char ManifestSrc[];
  static const char WorkerSrc[];

  // Mixed Content Directive
  // https://w3c.github.io/webappsec/specs/mixedcontent/#strict-mode
  static const char BlockAllMixedContent[];

  // https://w3c.github.io/webappsec/specs/upgrade/
  static const char UpgradeInsecureRequests[];

  // https://mikewest.github.io/cors-rfc1918/#csp
  static const char TreatAsPublicAddress[];

  // https://w3c.github.io/webappsec-subresource-integrity/#require-sri-for
  static const char RequireSRIFor[];

  enum ReportingStatus { SendReport, SuppressReport };

  enum ExceptionStatus { WillThrowException, WillNotThrowException };

  // This covers the possible values of a violation's 'resource', as defined in
  // https://w3c.github.io/webappsec-csp/#violation-resource. By the time we
  // generate a report, we're guaranteed that the value isn't 'null', so we
  // don't need that state in this enum.
  enum ViolationType { InlineViolation, EvalViolation, URLViolation };

  enum class InlineType { Block, Attribute };

  static ContentSecurityPolicy* create() { return new ContentSecurityPolicy(); }
  ~ContentSecurityPolicy();
  DECLARE_TRACE();

  void bindToExecutionContext(ExecutionContext*);
  void setupSelf(const SecurityOrigin&);
  void copyStateFrom(const ContentSecurityPolicy*);
  void copyPluginTypesFrom(const ContentSecurityPolicy*);

  void didReceiveHeaders(const ContentSecurityPolicyResponseHeaders&);
  void didReceiveHeader(const String&,
                        ContentSecurityPolicyHeaderType,
                        ContentSecurityPolicyHeaderSource);
  void addPolicyFromHeaderValue(const String&,
                                ContentSecurityPolicyHeaderType,
                                ContentSecurityPolicyHeaderSource);
  void reportAccumulatedHeaders(FrameLoaderClient*) const;

  std::unique_ptr<Vector<CSPHeaderAndType>> headers() const;

  // |element| will not be present for navigations to javascript URLs,
  // as those checks happen in the middle of the navigation algorithm,
  // and we generally don't have access to the responsible element.
  bool allowJavaScriptURLs(Element*,
                           const String& contextURL,
                           const WTF::OrdinalNumber& contextLine,
                           ReportingStatus = SendReport) const;

  // |element| will be present almost all of the time, but because of
  // strangeness around targeting handlers for '<body>', '<svg>', and
  // '<frameset>', it will be 'nullptr' for handlers on those
  // elements.
  bool allowInlineEventHandler(Element*,
                               const String& source,
                               const String& contextURL,
                               const WTF::OrdinalNumber& contextLine,
                               ReportingStatus = SendReport) const;
  // When the reporting status is |SendReport|, the |ExceptionStatus|
  // should indicate whether the caller will throw a JavaScript
  // exception in the event of a violation. When the caller will throw
  // an exception, ContentSecurityPolicy does not log a violation
  // message to the console because it would be redundant.
  bool allowEval(ScriptState* = nullptr,
                 ReportingStatus = SendReport,
                 ExceptionStatus = WillNotThrowException) const;
  bool allowPluginType(const String& type,
                       const String& typeAttribute,
                       const KURL&,
                       ReportingStatus = SendReport) const;
  // Checks whether the plugin type should be allowed in the given
  // document; enforces the CSP rule that PluginDocuments inherit
  // plugin-types directives from the parent document.
  bool allowPluginTypeForDocument(const Document&,
                                  const String& type,
                                  const String& typeAttribute,
                                  const KURL&,
                                  ReportingStatus = SendReport) const;

  bool allowObjectFromSource(const KURL&,
                             RedirectStatus = RedirectStatus::NoRedirect,
                             ReportingStatus = SendReport) const;
  bool allowFrameFromSource(const KURL&,
                            RedirectStatus = RedirectStatus::NoRedirect,
                            ReportingStatus = SendReport) const;
  bool allowImageFromSource(const KURL&,
                            RedirectStatus = RedirectStatus::NoRedirect,
                            ReportingStatus = SendReport) const;
  bool allowFontFromSource(const KURL&,
                           RedirectStatus = RedirectStatus::NoRedirect,
                           ReportingStatus = SendReport) const;
  bool allowMediaFromSource(const KURL&,
                            RedirectStatus = RedirectStatus::NoRedirect,
                            ReportingStatus = SendReport) const;
  bool allowConnectToSource(const KURL&,
                            RedirectStatus = RedirectStatus::NoRedirect,
                            ReportingStatus = SendReport) const;
  bool allowFormAction(const KURL&,
                       RedirectStatus = RedirectStatus::NoRedirect,
                       ReportingStatus = SendReport) const;
  bool allowBaseURI(const KURL&,
                    RedirectStatus = RedirectStatus::NoRedirect,
                    ReportingStatus = SendReport) const;
  bool allowWorkerContextFromSource(const KURL&,
                                    RedirectStatus = RedirectStatus::NoRedirect,
                                    ReportingStatus = SendReport) const;

  bool allowManifestFromSource(const KURL&,
                               RedirectStatus = RedirectStatus::NoRedirect,
                               ReportingStatus = SendReport) const;

  // Passing 'String()' into the |nonce| arguments in the following methods
  // represents an unnonced resource load.
  bool allowScriptFromSource(const KURL&,
                             const String& nonce,
                             ParserDisposition,
                             RedirectStatus = RedirectStatus::NoRedirect,
                             ReportingStatus = SendReport) const;
  bool allowStyleFromSource(const KURL&,
                            const String& nonce,
                            RedirectStatus = RedirectStatus::NoRedirect,
                            ReportingStatus = SendReport) const;
  bool allowInlineScript(Element*,
                         const String& contextURL,
                         const String& nonce,
                         const WTF::OrdinalNumber& contextLine,
                         const String& scriptContent,
                         ReportingStatus = SendReport) const;
  bool allowInlineStyle(Element*,
                        const String& contextURL,
                        const String& nonce,
                        const WTF::OrdinalNumber& contextLine,
                        const String& styleContent,
                        ReportingStatus = SendReport) const;

  // |allowAncestors| does not need to know whether the resource was a
  // result of a redirect. After a redirect, source paths are usually
  // ignored to stop a page from learning the path to which the
  // request was redirected, but this is not a concern for ancestors,
  // because a child frame can't manipulate the URL of a cross-origin
  // parent.
  bool allowAncestors(LocalFrame*,
                      const KURL&,
                      ReportingStatus = SendReport) const;
  bool isFrameAncestorsEnforced() const;

  // The hash allow functions are guaranteed to not have any side
  // effects, including reporting.
  // Hash functions check all policies relating to use of a script/style
  // with the given hash and return true all CSP policies allow it.
  // If these return true, callers can then process the content or
  // issue a load and be safe disabling any further CSP checks.
  //
  // TODO(mkwst): Fold hashes into 'allow{Script,Style}' checks above, just
  // as we've done with nonces. https://crbug.com/617065
  bool allowScriptWithHash(const String& source, InlineType) const;
  bool allowStyleWithHash(const String& source, InlineType) const;

  bool allowRequestWithoutIntegrity(WebURLRequest::RequestContext,
                                    const KURL&,
                                    RedirectStatus = RedirectStatus::NoRedirect,
                                    ReportingStatus = SendReport) const;

  bool allowRequest(WebURLRequest::RequestContext,
                    const KURL&,
                    const String& nonce,
                    const IntegrityMetadataSet&,
                    ParserDisposition,
                    RedirectStatus = RedirectStatus::NoRedirect,
                    ReportingStatus = SendReport) const;

  void usesScriptHashAlgorithms(uint8_t ContentSecurityPolicyHashAlgorithm);
  void usesStyleHashAlgorithms(uint8_t ContentSecurityPolicyHashAlgorithm);

  void setOverrideAllowInlineStyle(bool);
  void setOverrideURLForSelf(const KURL&);

  bool isActive() const;

  // If a frame is passed in, the message will be logged to its active
  // document's console.  Otherwise, the message will be logged to this object's
  // |m_executionContext|.
  void logToConsole(ConsoleMessage*, LocalFrame* = nullptr);

  void reportDirectiveAsSourceExpression(const String& directiveName,
                                         const String& sourceExpression);
  void reportDuplicateDirective(const String&);
  void reportInvalidDirectiveValueCharacter(const String& directiveName,
                                            const String& value);
  void reportInvalidPathCharacter(const String& directiveName,
                                  const String& value,
                                  const char);
  void reportInvalidPluginTypes(const String&);
  void reportInvalidRequireSRIForTokens(const String&);
  void reportInvalidSandboxFlags(const String&);
  void reportInvalidSourceExpression(const String& directiveName,
                                     const String& source);
  void reportMissingReportURI(const String&);
  void reportUnsupportedDirective(const String&);
  void reportInvalidInReportOnly(const String&);
  void reportInvalidDirectiveInMeta(const String& directiveName);
  void reportReportOnlyInMeta(const String&);
  void reportMetaOutsideHead(const String&);
  void reportValueForEmptyDirective(const String& directiveName,
                                    const String& value);

  // If a frame is passed in, the report will be sent using it as a context. If
  // no frame is passed in, the report will be sent via this object's
  // |m_executionContext| (or dropped on the floor if no such context is
  // available).
  void reportViolation(const String& directiveText,
                       const String& effectiveDirective,
                       const String& consoleMessage,
                       const KURL& blockedURL,
                       const Vector<String>& reportEndpoints,
                       const String& header,
                       ContentSecurityPolicyHeaderType,
                       ViolationType,
                       LocalFrame* = nullptr,
                       RedirectStatus = RedirectStatus::FollowedRedirect,
                       int contextLine = 0,
                       Element* = nullptr);

  // Called when mixed content is detected on a page; will trigger a violation
  // report if the 'block-all-mixed-content' directive is specified for a
  // policy.
  void reportMixedContent(const KURL& mixedURL, RedirectStatus);

  void reportBlockedScriptExecutionToInspector(
      const String& directiveText) const;

  const KURL url() const;
  void enforceSandboxFlags(SandboxFlags);
  void treatAsPublicAddress();
  String evalDisabledErrorMessage() const;

  // Upgrade-Insecure-Requests and Block-All-Mixed-Content are represented in
  // |m_insecureRequestPolicy|
  void enforceStrictMixedContentChecking();
  void upgradeInsecureRequests();
  WebInsecureRequestPolicy getInsecureRequestPolicy() const {
    return m_insecureRequestPolicy;
  }

  bool urlMatchesSelf(const KURL&) const;
  bool protocolMatchesSelf(const KURL&) const;
  bool selfMatchesInnerURL() const;

  bool experimentalFeaturesEnabled() const;

  bool shouldSendCSPHeader(Resource::Type) const;

  static bool shouldBypassMainWorld(const ExecutionContext*);

  static bool isDirectiveName(const String&);

  static bool isNonceableElement(const Element*);

  // This method checks whether the request should be allowed for an
  // experimental EmbeddingCSP feature
  // Please, see https://w3c.github.io/webappsec-csp/embedded/#origin-allowed.
  static bool shouldEnforceEmbeddersPolicy(const ResourceResponse&,
                                           SecurityOrigin*);

  Document* document() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(ContentSecurityPolicyTest, NonceInline);
  FRIEND_TEST_ALL_PREFIXES(ContentSecurityPolicyTest, NonceSinglePolicy);
  FRIEND_TEST_ALL_PREFIXES(ContentSecurityPolicyTest, NonceMultiplePolicy);

  ContentSecurityPolicy();

  void applyPolicySideEffectsToExecutionContext();

  KURL completeURL(const String&) const;

  void logToConsole(const String& message, MessageLevel = ErrorMessageLevel);

  void addAndReportPolicyFromHeaderValue(const String&,
                                         ContentSecurityPolicyHeaderType,
                                         ContentSecurityPolicyHeaderSource);

  bool shouldSendViolationReport(const String&) const;
  void didSendViolationReport(const String&);
  void dispatchViolationEvents(const SecurityPolicyViolationEventInit&,
                               Element*);
  void postViolationReport(const SecurityPolicyViolationEventInit&,
                           LocalFrame*,
                           const Vector<String>& reportEndpoints);

  Member<ExecutionContext> m_executionContext;
  bool m_overrideInlineStyleAllowed;
  CSPDirectiveListVector m_policies;
  ConsoleMessageVector m_consoleMessages;

  HashSet<unsigned, AlreadyHashed> m_violationReportsSent;

  // We put the hash functions used on the policy object so that we only need
  // to calculate a hash once and then distribute it to all of the directives
  // for validation.
  uint8_t m_scriptHashAlgorithmsUsed;
  uint8_t m_styleHashAlgorithmsUsed;

  // State flags used to configure the environment after parsing a policy.
  SandboxFlags m_sandboxMask;
  bool m_treatAsPublicAddress;
  String m_disableEvalErrorMessage;
  WebInsecureRequestPolicy m_insecureRequestPolicy;

  Member<CSPSource> m_selfSource;
  String m_selfProtocol;
};

}  // namespace blink

#endif
