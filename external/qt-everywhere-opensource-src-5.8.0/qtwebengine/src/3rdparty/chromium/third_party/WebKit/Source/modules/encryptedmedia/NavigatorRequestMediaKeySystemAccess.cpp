// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/encryptedmedia/NavigatorRequestMediaKeySystemAccess.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
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
#include "platform/Logging.h"
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

static WebVector<WebEncryptedMediaInitDataType> convertInitDataTypes(const Vector<String>& initDataTypes)
{
    WebVector<WebEncryptedMediaInitDataType> result(initDataTypes.size());
    for (size_t i = 0; i < initDataTypes.size(); ++i)
        result[i] = EncryptedMediaUtils::convertToInitDataType(initDataTypes[i]);
    return result;
}

static WebVector<WebMediaKeySystemMediaCapability> convertCapabilities(const HeapVector<MediaKeySystemMediaCapability>& capabilities)
{
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

static WebMediaKeySystemConfiguration::Requirement convertMediaKeysRequirement(const String& requirement)
{
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

static WebVector<WebEncryptedMediaSessionType> convertSessionTypes(const Vector<String>& sessionTypes)
{
    WebVector<WebEncryptedMediaSessionType> result(sessionTypes.size());
    for (size_t i = 0; i < sessionTypes.size(); ++i)
        result[i] = EncryptedMediaUtils::convertToSessionType(sessionTypes[i]);
    return result;
}

static bool AreCodecsSpecified(const WebMediaKeySystemMediaCapability& capability)
{
    return !capability.codecs.isEmpty();
}

// This class allows capabilities to be checked and a MediaKeySystemAccess
// object to be created asynchronously.
class MediaKeySystemAccessInitializer final : public EncryptedMediaRequest {
    WTF_MAKE_NONCOPYABLE(MediaKeySystemAccessInitializer);

public:
    MediaKeySystemAccessInitializer(ScriptState*, const String& keySystem, const HeapVector<MediaKeySystemConfiguration>& supportedConfigurations);
    ~MediaKeySystemAccessInitializer() override { }

    // EncryptedMediaRequest implementation.
    WebString keySystem() const override { return m_keySystem; }
    const WebVector<WebMediaKeySystemConfiguration>& supportedConfigurations() const override { return m_supportedConfigurations; }
    SecurityOrigin* getSecurityOrigin() const override { return m_resolver->getExecutionContext()->getSecurityOrigin(); }
    void requestSucceeded(WebContentDecryptionModuleAccess*) override;
    void requestNotSupported(const WebString& errorMessage) override;

    ScriptPromise promise() { return m_resolver->promise(); }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_resolver);
        EncryptedMediaRequest::trace(visitor);
    }

private:
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

    Member<ScriptPromiseResolver> m_resolver;
    const String m_keySystem;
    WebVector<WebMediaKeySystemConfiguration> m_supportedConfigurations;
};

MediaKeySystemAccessInitializer::MediaKeySystemAccessInitializer(ScriptState* scriptState, const String& keySystem, const HeapVector<MediaKeySystemConfiguration>& supportedConfigurations)
    : m_resolver(ScriptPromiseResolver::create(scriptState))
    , m_keySystem(keySystem)
    , m_supportedConfigurations(supportedConfigurations.size())
{
    for (size_t i = 0; i < supportedConfigurations.size(); ++i) {
        const MediaKeySystemConfiguration& config = supportedConfigurations[i];
        WebMediaKeySystemConfiguration webConfig;
        if (config.hasInitDataTypes()) {
            webConfig.hasInitDataTypes = true;
            webConfig.initDataTypes = convertInitDataTypes(config.initDataTypes());
        }
        if (config.hasAudioCapabilities()) {
            webConfig.hasAudioCapabilities = true;
            webConfig.audioCapabilities = convertCapabilities(config.audioCapabilities());
        }
        if (config.hasVideoCapabilities()) {
            webConfig.hasVideoCapabilities = true;
            webConfig.videoCapabilities = convertCapabilities(config.videoCapabilities());
        }
        DCHECK(config.hasDistinctiveIdentifier());
        webConfig.distinctiveIdentifier = convertMediaKeysRequirement(config.distinctiveIdentifier());
        DCHECK(config.hasPersistentState());
        webConfig.persistentState = convertMediaKeysRequirement(config.persistentState());
        if (config.hasSessionTypes()) {
            webConfig.hasSessionTypes = true;
            webConfig.sessionTypes = convertSessionTypes(config.sessionTypes());
        }
        // If |label| is not present, it will be a null string.
        webConfig.label = config.label();
        m_supportedConfigurations[i] = webConfig;
    }

    checkVideoCapabilityRobustness();
}

void MediaKeySystemAccessInitializer::requestSucceeded(WebContentDecryptionModuleAccess* access)
{
    checkEmptyCodecs(access->getConfiguration());

    m_resolver->resolve(new MediaKeySystemAccess(m_keySystem, wrapUnique(access)));
    m_resolver.clear();
}

void MediaKeySystemAccessInitializer::requestNotSupported(const WebString& errorMessage)
{
    m_resolver->reject(DOMException::create(NotSupportedError, errorMessage));
    m_resolver.clear();
}

void MediaKeySystemAccessInitializer::checkVideoCapabilityRobustness() const
{
    // Only check for widevine key system.
    if (keySystem() != "com.widevine.alpha")
        return;

    bool hasVideoCapabilities = false;
    bool hasEmptyRobustness = false;

    for (const auto& config : m_supportedConfigurations) {
        if (!config.hasVideoCapabilities)
            continue;

        hasVideoCapabilities = true;

        for (const auto& capability : config.videoCapabilities) {
            if (capability.robustness.isEmpty()) {
                hasEmptyRobustness = true;
                break;
            }
        }

        if (hasEmptyRobustness)
            break;
    }

    if (hasVideoCapabilities) {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(EnumerationHistogram, emptyRobustnessHistogram, new EnumerationHistogram("Media.EME.Widevine.VideoCapability.HasEmptyRobustness", 2));
        emptyRobustnessHistogram.count(hasEmptyRobustness);
    }

    if (hasEmptyRobustness) {
        m_resolver->getExecutionContext()->addConsoleMessage(ConsoleMessage::create(JSMessageSource, WarningMessageLevel,
            "It is recommended that a robustness level be specified. Not specifying the robustness level could "
            "result in unexpected behavior in the future, potentially including failure to play."));
    }
}

void MediaKeySystemAccessInitializer::checkEmptyCodecs(const WebMediaKeySystemConfiguration& config)
{
    // This is only checking for empty codecs in the selected configuration,
    // as apps may pass container only contentType strings for compatibility
    // with other implementations.
    // This will only check that all returned capabilities do not contain
    // codecs. This avoids alerting on configurations that will continue
    // to succeed in the future once strict checking is enforced.
    bool areAllAudioCodecsEmpty = false;
    if (config.hasAudioCapabilities && !config.audioCapabilities.isEmpty()) {
        areAllAudioCodecsEmpty = std::find_if(config.audioCapabilities.begin(), config.audioCapabilities.end(), AreCodecsSpecified)
            == config.audioCapabilities.end();
    }

    bool areAllVideoCodecsEmpty = false;
    if (config.hasVideoCapabilities && !config.videoCapabilities.isEmpty()) {
        areAllVideoCodecsEmpty = std::find_if(config.videoCapabilities.begin(), config.videoCapabilities.end(), AreCodecsSpecified)
            == config.videoCapabilities.end();
    }

    if (areAllAudioCodecsEmpty || areAllVideoCodecsEmpty) {
        Deprecation::countDeprecation(m_resolver->getExecutionContext(), UseCounter::EncryptedMediaAllSelectedContentTypesMissingCodecs);
    } else {
        UseCounter::count(m_resolver->getExecutionContext(), UseCounter::EncryptedMediaAllSelectedContentTypesHaveCodecs);
    }
}

} // namespace

ScriptPromise NavigatorRequestMediaKeySystemAccess::requestMediaKeySystemAccess(
    ScriptState* scriptState,
    Navigator& navigator,
    const String& keySystem,
    const HeapVector<MediaKeySystemConfiguration>& supportedConfigurations)
{
    DVLOG(3) << __FUNCTION__;

    // From https://w3c.github.io/encrypted-media/#requestMediaKeySystemAccess
    // When this method is invoked, the user agent must run the following steps:
    // 1. If keySystem is an empty string, return a promise rejected with a
    //    new DOMException whose name is InvalidAccessError.
    if (keySystem.isEmpty()) {
        return ScriptPromise::rejectWithDOMException(
            scriptState, DOMException::create(InvalidAccessError, "The keySystem parameter is empty."));
    }

    // 2. If supportedConfigurations was provided and is empty, return a
    //    promise rejected with a new DOMException whose name is
    //    InvalidAccessError.
    if (!supportedConfigurations.size()) {
        return ScriptPromise::rejectWithDOMException(
            scriptState, DOMException::create(InvalidAccessError, "The supportedConfigurations parameter is empty."));
    }

    // 3-4. 'May Document use powerful features?' check.
    ExecutionContext* executionContext = scriptState->getExecutionContext();
    String errorMessage;
    if (executionContext->isSecureContext(errorMessage)) {
        UseCounter::count(executionContext, UseCounter::EncryptedMediaSecureOrigin);
    } else {
        Deprecation::countDeprecation(executionContext, UseCounter::EncryptedMediaInsecureOrigin);
        // TODO(ddorwin): Implement the following:
        // Reject promise with a new DOMException whose name is NotSupportedError.
    }

    // 5. Let origin be the origin of document.
    //    (Passed with the execution context in step 7.)

    // 6. Let promise be a new promise.
    Document* document = toDocument(executionContext);
    if (!document->page()) {
        return ScriptPromise::rejectWithDOMException(
            scriptState, DOMException::create(InvalidStateError, "The context provided is not associated with a page."));
    }

    MediaKeySystemAccessInitializer* initializer = new MediaKeySystemAccessInitializer(scriptState, keySystem, supportedConfigurations);
    ScriptPromise promise = initializer->promise();

    // 7. Asynchronously determine support, and if allowed, create and
    //    initialize the MediaKeySystemAccess object.
    MediaKeysController* controller = MediaKeysController::from(document->page());
    WebEncryptedMediaClient* mediaClient = controller->encryptedMediaClient(executionContext);
    mediaClient->requestMediaKeySystemAccess(WebEncryptedMediaRequest(initializer));

    // 8. Return promise.
    return promise;
}

} // namespace blink
