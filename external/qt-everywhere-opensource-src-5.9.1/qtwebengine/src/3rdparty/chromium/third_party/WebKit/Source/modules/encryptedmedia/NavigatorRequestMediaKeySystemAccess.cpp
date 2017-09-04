// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/encryptedmedia/NavigatorRequestMediaKeySystemAccess.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/Deprecation.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/encryptedmedia/EncryptedMediaUtils.h"
#include "modules/encryptedmedia/MediaKeySession.h"
#include "modules/encryptedmedia/MediaKeySystemAccess.h"
#include "modules/encryptedmedia/MediaKeysController.h"
#include "platform/EncryptedMediaRequest.h"
#include "platform/Histogram.h"
#include "platform/network/ParsedContentType.h"
#include "public/platform/WebEncryptedMediaClient.h"
#include "public/platform/WebEncryptedMediaRequest.h"
#include "public/platform/WebMediaKeySystemConfiguration.h"
#include "public/platform/WebMediaKeySystemMediaCapability.h"
#include "public/platform/WebVector.h"
#include "wtf/PtrUtil.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"
#include <algorithm>

namespace blink {

namespace {

static WebVector<WebEncryptedMediaInitDataType> convertInitDataTypes(
    const Vector<String>& initDataTypes) {
  WebVector<WebEncryptedMediaInitDataType> result(initDataTypes.size());
  for (size_t i = 0; i < initDataTypes.size(); ++i)
    result[i] = EncryptedMediaUtils::convertToInitDataType(initDataTypes[i]);
  return result;
}

static WebVector<WebMediaKeySystemMediaCapability> convertCapabilities(
    const HeapVector<MediaKeySystemMediaCapability>& capabilities) {
  WebVector<WebMediaKeySystemMediaCapability> result(capabilities.size());
  for (size_t i = 0; i < capabilities.size(); ++i) {
    const WebString& contentType = capabilities[i].contentType();
    result[i].contentType = contentType;
    if (isValidContentType(contentType)) {
      // FIXME: Fail if there are unrecognized parameters.
      ParsedContentType type(capabilities[i].contentType());
      result[i].mimeType = type.mimeType();
      result[i].codecs = type.parameterValueForName("codecs");
    }
    result[i].robustness = capabilities[i].robustness();
  }
  return result;
}

static WebMediaKeySystemConfiguration::Requirement convertMediaKeysRequirement(
    const String& requirement) {
  if (requirement == "required")
    return WebMediaKeySystemConfiguration::Requirement::Required;
  if (requirement == "optional")
    return WebMediaKeySystemConfiguration::Requirement::Optional;
  if (requirement == "not-allowed")
    return WebMediaKeySystemConfiguration::Requirement::NotAllowed;

  // Everything else gets the default value.
  NOTREACHED();
  return WebMediaKeySystemConfiguration::Requirement::Optional;
}

static WebVector<WebEncryptedMediaSessionType> convertSessionTypes(
    const Vector<String>& sessionTypes) {
  WebVector<WebEncryptedMediaSessionType> result(sessionTypes.size());
  for (size_t i = 0; i < sessionTypes.size(); ++i)
    result[i] = EncryptedMediaUtils::convertToSessionType(sessionTypes[i]);
  return result;
}

static bool AreCodecsSpecified(
    const WebMediaKeySystemMediaCapability& capability) {
  return !capability.codecs.isEmpty();
}

// This class allows capabilities to be checked and a MediaKeySystemAccess
// object to be created asynchronously.
class MediaKeySystemAccessInitializer final : public EncryptedMediaRequest {
  WTF_MAKE_NONCOPYABLE(MediaKeySystemAccessInitializer);

 public:
  MediaKeySystemAccessInitializer(
      ScriptState*,
      const String& keySystem,
      const HeapVector<MediaKeySystemConfiguration>& supportedConfigurations);
  ~MediaKeySystemAccessInitializer() override {}

  // EncryptedMediaRequest implementation.
  WebString keySystem() const override { return m_keySystem; }
  const WebVector<WebMediaKeySystemConfiguration>& supportedConfigurations()
      const override {
    return m_supportedConfigurations;
  }
  SecurityOrigin* getSecurityOrigin() const override;
  void requestSucceeded(WebContentDecryptionModuleAccess*) override;
  void requestNotSupported(const WebString& errorMessage) override;

  ScriptPromise promise() { return m_resolver->promise(); }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_resolver);
    EncryptedMediaRequest::trace(visitor);
  }

 private:
  // Returns true if the ExecutionContext is valid, false otherwise.
  bool isExecutionContextValid() const;

  // For widevine key system, generate warning and report to UMA if
  // |m_supportedConfigurations| contains any video capability with empty
  // robustness string.
  // TODO(xhwang): Remove after we handle empty robustness correctly.
  // See http://crbug.com/482277
  void checkVideoCapabilityRobustness() const;

  // Generate deprecation warning and log UseCounter if configuration
  // contains only container-only contentType strings.
  // TODO(jrummell): Remove once this is no longer allowed.
  // See http://crbug.com/605661.
  void checkEmptyCodecs(const WebMediaKeySystemConfiguration&);

  // Log UseCounter if selected configuration does not have at least one of
  // 'audioCapabilities' and 'videoCapabilities' non-empty.
  // TODO(jrummell): Switch to deprecation message once we have data.
  // See http://crbug.com/616233.
  void checkCapabilities(const WebMediaKeySystemConfiguration&);

  Member<ScriptPromiseResolver> m_resolver;
  const String m_keySystem;
  WebVector<WebMediaKeySystemConfiguration> m_supportedConfigurations;
};

MediaKeySystemAccessInitializer::MediaKeySystemAccessInitializer(
    ScriptState* scriptState,
    const String& keySystem,
    const HeapVector<MediaKeySystemConfiguration>& supportedConfigurations)
    : m_resolver(ScriptPromiseResolver::create(scriptState)),
      m_keySystem(keySystem),
      m_supportedConfigurations(supportedConfigurations.size()) {
  for (size_t i = 0; i < supportedConfigurations.size(); ++i) {
    const MediaKeySystemConfiguration& config = supportedConfigurations[i];
    WebMediaKeySystemConfiguration webConfig;

    DCHECK(config.hasInitDataTypes());
    webConfig.initDataTypes = convertInitDataTypes(config.initDataTypes());

    DCHECK(config.hasAudioCapabilities());
    webConfig.audioCapabilities =
        convertCapabilities(config.audioCapabilities());

    DCHECK(config.hasVideoCapabilities());
    webConfig.videoCapabilities =
        convertCapabilities(config.videoCapabilities());

    DCHECK(config.hasDistinctiveIdentifier());
    webConfig.distinctiveIdentifier =
        convertMediaKeysRequirement(config.distinctiveIdentifier());

    DCHECK(config.hasPersistentState());
    webConfig.persistentState =
        convertMediaKeysRequirement(config.persistentState());

    if (config.hasSessionTypes()) {
      webConfig.sessionTypes = convertSessionTypes(config.sessionTypes());
    } else {
      // From the spec
      // (http://w3c.github.io/encrypted-media/#idl-def-mediakeysystemconfiguration):
      // If this member is not present when the dictionary is passed to
      // requestMediaKeySystemAccess(), the dictionary will be treated
      // as if this member is set to [ "temporary" ].
      WebVector<WebEncryptedMediaSessionType> sessionTypes(
          static_cast<size_t>(1));
      sessionTypes[0] = WebEncryptedMediaSessionType::Temporary;
      webConfig.sessionTypes = sessionTypes;
    }

    // If |label| is not present, it will be a null string.
    webConfig.label = config.label();
    m_supportedConfigurations[i] = webConfig;
  }

  checkVideoCapabilityRobustness();
}

SecurityOrigin* MediaKeySystemAccessInitializer::getSecurityOrigin() const {
  return isExecutionContextValid()
             ? m_resolver->getExecutionContext()->getSecurityOrigin()
             : nullptr;
}

void MediaKeySystemAccessInitializer::requestSucceeded(
    WebContentDecryptionModuleAccess* access) {
  checkEmptyCodecs(access->getConfiguration());
  checkCapabilities(access->getConfiguration());

  if (!isExecutionContextValid())
    return;

  m_resolver->resolve(
      new MediaKeySystemAccess(m_keySystem, wrapUnique(access)));
  m_resolver.clear();
}

void MediaKeySystemAccessInitializer::requestNotSupported(
    const WebString& errorMessage) {
  if (!isExecutionContextValid())
    return;

  m_resolver->reject(DOMException::create(NotSupportedError, errorMessage));
  m_resolver.clear();
}

bool MediaKeySystemAccessInitializer::isExecutionContextValid() const {
  // activeDOMObjectsAreStopped() is called to see if the context is in the
  // process of being destroyed. If it is true, assume the context is no
  // longer valid as it is about to be destroyed anyway.
  ExecutionContext* context = m_resolver->getExecutionContext();
  return context && !context->activeDOMObjectsAreStopped();
}

void MediaKeySystemAccessInitializer::checkVideoCapabilityRobustness() const {
  // Only check for widevine key system.
  if (keySystem() != "com.widevine.alpha")
    return;

  bool hasVideoCapabilities = false;
  bool hasEmptyRobustness = false;

  for (const auto& config : m_supportedConfigurations) {
    for (const auto& capability : config.videoCapabilities) {
      hasVideoCapabilities = true;
      if (capability.robustness.isEmpty()) {
        hasEmptyRobustness = true;
        break;
      }
    }

    if (hasEmptyRobustness)
      break;
  }

  if (hasVideoCapabilities) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        EnumerationHistogram, emptyRobustnessHistogram,
        new EnumerationHistogram(
            "Media.EME.Widevine.VideoCapability.HasEmptyRobustness", 2));
    emptyRobustnessHistogram.count(hasEmptyRobustness);
  }

  if (hasEmptyRobustness) {
    m_resolver->getExecutionContext()->addConsoleMessage(ConsoleMessage::create(
        JSMessageSource, WarningMessageLevel,
        "It is recommended that a robustness level be specified. Not "
        "specifying the robustness level could "
        "result in unexpected behavior in the future, potentially including "
        "failure to play."));
  }
}

void MediaKeySystemAccessInitializer::checkEmptyCodecs(
    const WebMediaKeySystemConfiguration& config) {
  // This is only checking for empty codecs in the selected configuration,
  // as apps may pass container only contentType strings for compatibility
  // with other implementations.
  // This will only check that all returned capabilities do not contain
  // codecs. This avoids alerting on configurations that will continue
  // to succeed in the future once strict checking is enforced.
  bool areAllAudioCodecsEmpty = false;
  if (!config.audioCapabilities.isEmpty()) {
    areAllAudioCodecsEmpty =
        std::find_if(config.audioCapabilities.begin(),
                     config.audioCapabilities.end(),
                     AreCodecsSpecified) == config.audioCapabilities.end();
  }

  bool areAllVideoCodecsEmpty = false;
  if (!config.videoCapabilities.isEmpty()) {
    areAllVideoCodecsEmpty =
        std::find_if(config.videoCapabilities.begin(),
                     config.videoCapabilities.end(),
                     AreCodecsSpecified) == config.videoCapabilities.end();
  }

  if (areAllAudioCodecsEmpty || areAllVideoCodecsEmpty) {
    Deprecation::countDeprecation(
        m_resolver->getExecutionContext(),
        UseCounter::EncryptedMediaAllSelectedContentTypesMissingCodecs);
  } else {
    UseCounter::count(
        m_resolver->getExecutionContext(),
        UseCounter::EncryptedMediaAllSelectedContentTypesHaveCodecs);
  }
}

void MediaKeySystemAccessInitializer::checkCapabilities(
    const WebMediaKeySystemConfiguration& config) {
  // This is only checking that at least one capability is provided in the
  // selected configuration, as apps may pass empty capabilities for
  // compatibility with other implementations.
  bool atLeastOneAudioCapability = config.audioCapabilities.size() > 0;
  bool atLeastOneVideoCapability = config.videoCapabilities.size() > 0;

  if (atLeastOneAudioCapability || atLeastOneVideoCapability) {
    UseCounter::count(m_resolver->getExecutionContext(),
                      UseCounter::EncryptedMediaCapabilityProvided);
  } else {
    Deprecation::countDeprecation(
        m_resolver->getExecutionContext(),
        UseCounter::EncryptedMediaCapabilityNotProvided);
  }
}

}  // namespace

ScriptPromise NavigatorRequestMediaKeySystemAccess::requestMediaKeySystemAccess(
    ScriptState* scriptState,
    Navigator& navigator,
    const String& keySystem,
    const HeapVector<MediaKeySystemConfiguration>& supportedConfigurations) {
  DVLOG(3) << __func__;

  // From https://w3c.github.io/encrypted-media/#requestMediaKeySystemAccess
  // When this method is invoked, the user agent must run the following steps:
  // 1. If keySystem is the empty string, return a promise rejected with a
  //    newly created TypeError.
  if (keySystem.isEmpty()) {
    return ScriptPromise::reject(
        scriptState,
        V8ThrowException::createTypeError(scriptState->isolate(),
                                          "The keySystem parameter is empty."));
  }

  // 2. If supportedConfigurations is empty, return a promise rejected with
  //    a newly created TypeError.
  if (!supportedConfigurations.size()) {
    return ScriptPromise::reject(
        scriptState, V8ThrowException::createTypeError(
                         scriptState->isolate(),
                         "The supportedConfigurations parameter is empty."));
  }

  // Note: This method should only be exposed to secure contexts as indicated
  // by the [SecureContext] IDL attribute. Since that will break some existing
  // sites, we simply keep track of sites that aren't secure and output a
  // deprecation message.
  ExecutionContext* executionContext = scriptState->getExecutionContext();
  String errorMessage;
  if (executionContext->isSecureContext(errorMessage)) {
    UseCounter::count(executionContext, UseCounter::EncryptedMediaSecureOrigin);
  } else {
    Deprecation::countDeprecation(executionContext,
                                  UseCounter::EncryptedMediaInsecureOrigin);
    // TODO(ddorwin): Implement the following:
    // Reject promise with a new DOMException whose name is NotSupportedError.
  }

  // 3. Let document be the calling context's Document.
  Document* document = toDocument(executionContext);
  if (!document->page()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(
            InvalidStateError,
            "The context provided is not associated with a page."));
  }

  // 4. Let origin be the origin of document.
  //    (Passed with the execution context.)

  // 5. Let promise be a new promise.
  MediaKeySystemAccessInitializer* initializer =
      new MediaKeySystemAccessInitializer(scriptState, keySystem,
                                          supportedConfigurations);
  ScriptPromise promise = initializer->promise();

  // 6. Asynchronously determine support, and if allowed, create and
  //    initialize the MediaKeySystemAccess object.
  MediaKeysController* controller = MediaKeysController::from(document->page());
  WebEncryptedMediaClient* mediaClient =
      controller->encryptedMediaClient(executionContext);
  mediaClient->requestMediaKeySystemAccess(
      WebEncryptedMediaRequest(initializer));

  // 7. Return promise.
  return promise;
}

}  // namespace blink
