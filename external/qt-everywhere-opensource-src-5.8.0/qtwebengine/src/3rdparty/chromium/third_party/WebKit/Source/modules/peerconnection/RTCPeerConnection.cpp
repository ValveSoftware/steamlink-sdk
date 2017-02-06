/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/peerconnection/RTCPeerConnection.h"

#include "bindings/core/v8/ArrayValue.h"
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/Microtask.h"
#include "bindings/core/v8/Nullable.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "bindings/modules/v8/RTCIceCandidateInitOrRTCIceCandidate.h"
#include "bindings/modules/v8/V8RTCCertificate.h"
#include "core/dom/DOMException.h"
#include "core/dom/DOMTimeStamp.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/HostsUsingFeatures.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/html/VoidCallback.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "modules/crypto/CryptoResultImpl.h"
#include "modules/mediastream/MediaConstraintsImpl.h"
#include "modules/mediastream/MediaStreamEvent.h"
#include "modules/peerconnection/RTCAnswerOptions.h"
#include "modules/peerconnection/RTCDTMFSender.h"
#include "modules/peerconnection/RTCDataChannel.h"
#include "modules/peerconnection/RTCDataChannelEvent.h"
#include "modules/peerconnection/RTCIceCandidateEvent.h"
#include "modules/peerconnection/RTCOfferOptions.h"
#include "modules/peerconnection/RTCPeerConnectionErrorCallback.h"
#include "modules/peerconnection/RTCSessionDescription.h"
#include "modules/peerconnection/RTCSessionDescriptionCallback.h"
#include "modules/peerconnection/RTCSessionDescriptionInit.h"
#include "modules/peerconnection/RTCSessionDescriptionRequestImpl.h"
#include "modules/peerconnection/RTCSessionDescriptionRequestPromiseImpl.h"
#include "modules/peerconnection/RTCStatsCallback.h"
#include "modules/peerconnection/RTCStatsRequestImpl.h"
#include "modules/peerconnection/RTCVoidRequestImpl.h"
#include "modules/peerconnection/RTCVoidRequestPromiseImpl.h"
#include "platform/peerconnection/RTCAnswerOptionsPlatform.h"
#include "platform/peerconnection/RTCConfiguration.h"
#include "platform/peerconnection/RTCOfferOptionsPlatform.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCryptoAlgorithmParams.h"
#include "public/platform/WebMediaStream.h"
#include "public/platform/WebRTCAnswerOptions.h"
#include "public/platform/WebRTCCertificate.h"
#include "public/platform/WebRTCCertificateGenerator.h"
#include "public/platform/WebRTCConfiguration.h"
#include "public/platform/WebRTCDataChannelHandler.h"
#include "public/platform/WebRTCDataChannelInit.h"
#include "public/platform/WebRTCICECandidate.h"
#include "public/platform/WebRTCKeyParams.h"
#include "public/platform/WebRTCOfferOptions.h"
#include "public/platform/WebRTCSessionDescription.h"
#include "public/platform/WebRTCSessionDescriptionRequest.h"
#include "public/platform/WebRTCStatsRequest.h"
#include "public/platform/WebRTCVoidRequest.h"
#include "wtf/CurrentTime.h"
#include "wtf/PtrUtil.h"
#include <algorithm>
#include <memory>

namespace blink {

namespace {

const char kSignalingStateClosedMessage[] = "The RTCPeerConnection's signalingState is 'closed'.";

bool throwExceptionIfSignalingStateClosed(RTCPeerConnection::SignalingState state, ExceptionState& exceptionState)
{
    if (state == RTCPeerConnection::SignalingStateClosed) {
        exceptionState.throwDOMException(InvalidStateError, kSignalingStateClosedMessage);
        return true;
    }

    return false;
}

void asyncCallErrorCallback(RTCPeerConnectionErrorCallback* errorCallback, DOMException* exception)
{
    DCHECK(errorCallback);
    Microtask::enqueueMicrotask(WTF::bind(&RTCPeerConnectionErrorCallback::handleEvent, wrapPersistent(errorCallback), wrapPersistent(exception)));
}

bool callErrorCallbackIfSignalingStateClosed(RTCPeerConnection::SignalingState state, RTCPeerConnectionErrorCallback* errorCallback)
{
    if (state == RTCPeerConnection::SignalingStateClosed) {
        if (errorCallback)
            asyncCallErrorCallback(errorCallback, DOMException::create(InvalidStateError, kSignalingStateClosedMessage));

        return true;
    }

    return false;
}

bool isIceCandidateMissingSdp(const RTCIceCandidateInitOrRTCIceCandidate& candidate)
{
    if (candidate.isRTCIceCandidateInit()) {
        const RTCIceCandidateInit& iceCandidateInit = candidate.getAsRTCIceCandidateInit();
        return !iceCandidateInit.hasSdpMid() && !iceCandidateInit.hasSdpMLineIndex();
    }

    DCHECK(candidate.isRTCIceCandidate());
    return false;
}

WebRTCOfferOptions convertToWebRTCOfferOptions(const RTCOfferOptions& options)
{
    return WebRTCOfferOptions(RTCOfferOptionsPlatform::create(
        options.hasOfferToReceiveVideo() ? std::max(options.offerToReceiveVideo(), 0) : -1,
        options.hasOfferToReceiveAudio() ? std::max(options.offerToReceiveAudio(), 0) : -1,
        options.hasVoiceActivityDetection() ? options.voiceActivityDetection() : true,
        options.hasIceRestart() ? options.iceRestart() : false));
}

WebRTCAnswerOptions convertToWebRTCAnswerOptions(const RTCAnswerOptions& options)
{
    return WebRTCAnswerOptions(RTCAnswerOptionsPlatform::create(
        options.hasVoiceActivityDetection() ? options.voiceActivityDetection() : true));
}

WebRTCICECandidate convertToWebRTCIceCandidate(ExecutionContext* context, const RTCIceCandidateInitOrRTCIceCandidate& candidate)
{
    DCHECK(!candidate.isNull());
    if (candidate.isRTCIceCandidateInit()) {
        const RTCIceCandidateInit& iceCandidateInit = candidate.getAsRTCIceCandidateInit();
        // TODO(guidou): Change default value to -1. crbug.com/614958.
        unsigned short sdpMLineIndex = 0;
        if (iceCandidateInit.hasSdpMLineIndex())
            sdpMLineIndex = iceCandidateInit.sdpMLineIndex();
        else
            UseCounter::count(context, UseCounter::RTCIceCandidateDefaultSdpMLineIndex);
        return WebRTCICECandidate(iceCandidateInit.candidate(), iceCandidateInit.sdpMid(), sdpMLineIndex);
    }

    DCHECK(candidate.isRTCIceCandidate());
    return candidate.getAsRTCIceCandidate()->webCandidate();
}

// Helper class for RTCPeerConnection::generateCertificate.
class WebRTCCertificateObserver : public WebRTCCertificateCallback {
public:
    // Takes ownership of |resolver|.
    static WebRTCCertificateObserver* create(ScriptPromiseResolver* resolver)
    {
        return new WebRTCCertificateObserver(resolver);
    }

    ~WebRTCCertificateObserver() override {}

private:
    WebRTCCertificateObserver(ScriptPromiseResolver* resolver)
        : m_resolver(resolver) {}

    void onSuccess(std::unique_ptr<WebRTCCertificate> certificate) override
    {
        m_resolver->resolve(new RTCCertificate(std::move(certificate)));
    }

    void onError() override
    {
        m_resolver->reject();
    }

    Persistent<ScriptPromiseResolver> m_resolver;
};

RTCConfiguration* parseConfiguration(const Dictionary& configuration, ExceptionState& exceptionState, RtcpMuxPolicy* selectedRtcpMuxPolicy)
{
    if (configuration.isUndefinedOrNull())
        return 0;

    RTCIceTransports iceTransports = RTCIceTransportsAll;
    String iceTransportsString;
    if (DictionaryHelper::get(configuration, "iceTransports", iceTransportsString)) {
        if (iceTransportsString == "none") {
            iceTransports = RTCIceTransportsNone;
        } else if (iceTransportsString == "relay") {
            iceTransports = RTCIceTransportsRelay;
        } else if (iceTransportsString != "all") {
            exceptionState.throwTypeError("Malformed RTCIceTransports");
            return 0;
        }
    }

    ArrayValue iceServers;
    bool ok = DictionaryHelper::get(configuration, "iceServers", iceServers);
    if (!ok || iceServers.isUndefinedOrNull()) {
        exceptionState.throwTypeError("Malformed RTCConfiguration");
        return 0;
    }

    size_t numberOfServers;
    ok = iceServers.length(numberOfServers);
    if (!ok) {
        exceptionState.throwTypeError("Malformed RTCConfiguration");
        return 0;
    }

    RTCBundlePolicy bundlePolicy = RTCBundlePolicyBalanced;
    String bundlePolicyString;
    if (DictionaryHelper::get(configuration, "bundlePolicy", bundlePolicyString)) {
        if (bundlePolicyString == "max-compat") {
            bundlePolicy = RTCBundlePolicyMaxCompat;
        } else if (bundlePolicyString == "max-bundle") {
            bundlePolicy = RTCBundlePolicyMaxBundle;
        } else if (bundlePolicyString != "balanced") {
            exceptionState.throwTypeError("Malformed RTCBundlePolicy");
            return 0;
        }
    }

    // For the histogram value of "WebRTC.PeerConnection.SelectedRtcpMuxPolicy".
    *selectedRtcpMuxPolicy = RtcpMuxPolicyDefault;
    RTCRtcpMuxPolicy rtcpMuxPolicy = RTCRtcpMuxPolicyNegotiate;
    String rtcpMuxPolicyString;
    if (DictionaryHelper::get(configuration, "rtcpMuxPolicy", rtcpMuxPolicyString)) {
        if (rtcpMuxPolicyString == "require") {
            *selectedRtcpMuxPolicy = RtcpMuxPolicyRequire;
            rtcpMuxPolicy = RTCRtcpMuxPolicyRequire;
        } else if (rtcpMuxPolicyString == "negotiate") {
            *selectedRtcpMuxPolicy = RtcpMuxPolicyNegotiate;
            rtcpMuxPolicy = RTCRtcpMuxPolicyNegotiate;
        } else {
            exceptionState.throwTypeError("Malformed RTCRtcpMuxPolicy");
            return 0;
        }
    }

    RTCConfiguration* rtcConfiguration = RTCConfiguration::create();
    rtcConfiguration->setIceTransports(iceTransports);
    rtcConfiguration->setBundlePolicy(bundlePolicy);
    rtcConfiguration->setRtcpMuxPolicy(rtcpMuxPolicy);

    for (size_t i = 0; i < numberOfServers; ++i) {
        Dictionary iceServer;
        ok = iceServers.get(i, iceServer);
        if (!ok) {
            exceptionState.throwTypeError("Malformed RTCIceServer");
            return 0;
        }

        Vector<String> names;
        iceServer.getPropertyNames(names);

        Vector<String> urlStrings;
        if (names.contains("urls")) {
            if (!DictionaryHelper::get(iceServer, "urls", urlStrings) || !urlStrings.size()) {
                String urlString;
                if (DictionaryHelper::get(iceServer, "urls", urlString)) {
                    urlStrings.append(urlString);
                } else {
                    exceptionState.throwTypeError("Malformed RTCIceServer");
                    return 0;
                }
            }
        } else if (names.contains("url")) {
            String urlString;
            if (DictionaryHelper::get(iceServer, "url", urlString)) {
                urlStrings.append(urlString);
            } else {
                exceptionState.throwTypeError("Malformed RTCIceServer");
                return 0;
            }
        } else {
            exceptionState.throwTypeError("Malformed RTCIceServer");
            return 0;
        }

        String username, credential;
        DictionaryHelper::get(iceServer, "username", username);
        DictionaryHelper::get(iceServer, "credential", credential);

        for (Vector<String>::iterator iter = urlStrings.begin(); iter != urlStrings.end(); ++iter) {
            KURL url(KURL(), *iter);
            if (!url.isValid() || !(url.protocolIs("turn") || url.protocolIs("turns") || url.protocolIs("stun"))) {
                exceptionState.throwTypeError("Malformed URL");
                return 0;
            }

            rtcConfiguration->appendServer(RTCIceServer::create(url, username, credential));
        }
    }

    ArrayValue certificates;
    if (DictionaryHelper::get(configuration, "certificates", certificates) && !certificates.isUndefinedOrNull()) {
        size_t numberOfCertificates;
        certificates.length(numberOfCertificates);
        for (size_t i = 0; i < numberOfCertificates; ++i) {
            RTCCertificate* certificate = nullptr;

            Dictionary dictCert;
            certificates.get(i, dictCert);
            v8::Local<v8::Value> valCert = dictCert.v8Value();
            if (!valCert.IsEmpty()) {
                certificate = V8RTCCertificate::toImplWithTypeCheck(configuration.isolate(), valCert);
            }
            if (!certificate) {
                exceptionState.throwTypeError("Malformed sequence<RTCCertificate>");
                return 0;
            }

            rtcConfiguration->appendCertificate(certificate->certificateShallowCopy());
        }
    }
    return rtcConfiguration;
}

RTCOfferOptionsPlatform* parseOfferOptions(const Dictionary& options)
{
    if (options.isUndefinedOrNull())
        return 0;

    Vector<String> propertyNames;
    options.getPropertyNames(propertyNames);

    // Treat |options| as MediaConstraints if it is empty or has "optional" or "mandatory" properties for compatibility.
    // TODO(jiayl): remove constraints when RTCOfferOptions reaches Stable and client code is ready.
    if (propertyNames.isEmpty() || propertyNames.contains("optional") || propertyNames.contains("mandatory"))
        return 0;

    int32_t offerToReceiveVideo = -1;
    int32_t offerToReceiveAudio = -1;
    bool voiceActivityDetection = true;
    bool iceRestart = false;

    if (DictionaryHelper::get(options, "offerToReceiveVideo", offerToReceiveVideo) && offerToReceiveVideo < 0)
        offerToReceiveVideo = 0;
    if (DictionaryHelper::get(options, "offerToReceiveAudio", offerToReceiveAudio) && offerToReceiveAudio < 0)
        offerToReceiveAudio = 0;
    DictionaryHelper::get(options, "voiceActivityDetection", voiceActivityDetection);
    DictionaryHelper::get(options, "iceRestart", iceRestart);

    RTCOfferOptionsPlatform* rtcOfferOptions = RTCOfferOptionsPlatform::create(offerToReceiveVideo, offerToReceiveAudio, voiceActivityDetection, iceRestart);
    return rtcOfferOptions;
}

} // namespace

RTCPeerConnection::EventWrapper::EventWrapper(
    Event* event,
    std::unique_ptr<BoolFunction> function)
    : m_event(event)
    , m_setupFunction(std::move(function))
{
}

bool RTCPeerConnection::EventWrapper::setup()
{
    if (m_setupFunction) {
        return (*m_setupFunction)();
    }
    return true;
}

DEFINE_TRACE(RTCPeerConnection::EventWrapper)
{
    visitor->trace(m_event);
}

RTCPeerConnection* RTCPeerConnection::create(ExecutionContext* context, const Dictionary& rtcConfiguration, const Dictionary& mediaConstraints, ExceptionState& exceptionState)
{
    if (mediaConstraints.isObject())
        UseCounter::count(context, UseCounter::RTCPeerConnectionConstructorConstraints);
    else
        UseCounter::count(context, UseCounter::RTCPeerConnectionConstructorCompliant);

    // Record the RtcpMuxPolicy for histogram "WebRTC.PeerConnection.SelectedRtcpMuxPolicy".
    RtcpMuxPolicy selectedRtcpMuxPolicy = RtcpMuxPolicyDefault;
    RTCConfiguration* configuration = parseConfiguration(rtcConfiguration, exceptionState, &selectedRtcpMuxPolicy);
    if (exceptionState.hadException())
        return 0;

    // Make sure no certificates have expired.
    if (configuration && configuration->numberOfCertificates() > 0) {
        DOMTimeStamp now = convertSecondsToDOMTimeStamp(currentTime());
        for (size_t i = 0; i < configuration->numberOfCertificates(); ++i) {
            DOMTimeStamp expires = configuration->certificate(i)->expires();
            if (expires <= now) {
                exceptionState.throwDOMException(InvalidStateError, "Expired certificate(s).");
                return 0;
            }
        }
    }

    MediaErrorState mediaErrorState;
    WebMediaConstraints constraints = MediaConstraintsImpl::create(context, mediaConstraints, mediaErrorState);
    if (mediaErrorState.hadException()) {
        mediaErrorState.raiseException(exceptionState);
        return 0;
    }

    RTCPeerConnection* peerConnection = new RTCPeerConnection(context, configuration, constraints, exceptionState);
    peerConnection->suspendIfNeeded();
    if (exceptionState.hadException())
        return 0;

    peerConnection->m_peerHandler->logSelectedRtcpMuxPolicy(selectedRtcpMuxPolicy);

    return peerConnection;
}

RTCPeerConnection::RTCPeerConnection(ExecutionContext* context, RTCConfiguration* configuration, WebMediaConstraints constraints, ExceptionState& exceptionState)
    : ActiveScriptWrappable(this)
    , ActiveDOMObject(context)
    , m_signalingState(SignalingStateStable)
    , m_iceGatheringState(ICEGatheringStateNew)
    , m_iceConnectionState(ICEConnectionStateNew)
    , m_dispatchScheduledEventRunner(AsyncMethodRunner<RTCPeerConnection>::create(this, &RTCPeerConnection::dispatchScheduledEvent))
    , m_stopped(false)
    , m_closed(false)
{
    ThreadState::current()->registerPreFinalizer(this);
    Document* document = toDocument(getExecutionContext());

    // If we fail, set |m_closed| and |m_stopped| to true, to avoid hitting the assert in the destructor.

    if (!document->frame()) {
        m_closed = true;
        m_stopped = true;
        exceptionState.throwDOMException(NotSupportedError, "PeerConnections may not be created in detached documents.");
        return;
    }

    m_peerHandler = wrapUnique(Platform::current()->createRTCPeerConnectionHandler(this));
    if (!m_peerHandler) {
        m_closed = true;
        m_stopped = true;
        exceptionState.throwDOMException(NotSupportedError, "No PeerConnection handler can be created, perhaps WebRTC is disabled?");
        return;
    }

    document->frame()->loader().client()->dispatchWillStartUsingPeerConnectionHandler(m_peerHandler.get());

    if (!m_peerHandler->initialize(configuration, constraints)) {
        m_closed = true;
        m_stopped = true;
        exceptionState.throwDOMException(NotSupportedError, "Failed to initialize native PeerConnection.");
        return;
    }
}

RTCPeerConnection::~RTCPeerConnection()
{
    // This checks that close() or stop() is called before the destructor.
    // We are assuming that a wrapper is always created when RTCPeerConnection is created.
    DCHECK(m_closed || m_stopped);
}

void RTCPeerConnection::dispose()
{
    // Promptly clears a raw reference from content/ to an on-heap object
    // so that content/ doesn't access it in a lazy sweeping phase.
    m_peerHandler.reset();
}

ScriptPromise RTCPeerConnection::createOffer(ScriptState* scriptState, const RTCOfferOptions& options)
{
    if (m_signalingState == SignalingStateClosed)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(InvalidStateError, kSignalingStateClosedMessage));

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    RTCSessionDescriptionRequest* request = RTCSessionDescriptionRequestPromiseImpl::create(this, resolver);
    m_peerHandler->createOffer(request, convertToWebRTCOfferOptions(options));
    return promise;
}

ScriptPromise RTCPeerConnection::createOffer(ScriptState* scriptState, RTCSessionDescriptionCallback* successCallback, RTCPeerConnectionErrorCallback* errorCallback, const Dictionary& rtcOfferOptions)
{
    DCHECK(successCallback);
    DCHECK(errorCallback);
    ExecutionContext* context = scriptState->getExecutionContext();
    UseCounter::count(context, UseCounter::RTCPeerConnectionCreateOfferLegacyFailureCallback);
    if (callErrorCallbackIfSignalingStateClosed(m_signalingState, errorCallback))
        return ScriptPromise::castUndefined(scriptState);

    RTCOfferOptionsPlatform* offerOptions = parseOfferOptions(rtcOfferOptions);
    RTCSessionDescriptionRequest* request = RTCSessionDescriptionRequestImpl::create(getExecutionContext(), this, successCallback, errorCallback);

    if (offerOptions) {
        if (offerOptions->offerToReceiveAudio() != -1 || offerOptions->offerToReceiveVideo() != -1)
            UseCounter::count(context, UseCounter::RTCPeerConnectionCreateOfferLegacyOfferOptions);
        else
            UseCounter::count(context, UseCounter::RTCPeerConnectionCreateOfferLegacyCompliant);

        m_peerHandler->createOffer(request, WebRTCOfferOptions(offerOptions));
    } else {
        MediaErrorState mediaErrorState;
        WebMediaConstraints constraints = MediaConstraintsImpl::create(context, rtcOfferOptions, mediaErrorState);
        // Report constraints parsing errors via the callback, but ignore unknown/unsupported constraints as they
        // would be silently discarded by WebIDL.
        if (mediaErrorState.canGenerateException()) {
            String errorMsg = mediaErrorState.getErrorMessage();
            asyncCallErrorCallback(errorCallback, DOMException::create(OperationError, errorMsg));
            return ScriptPromise::castUndefined(scriptState);
        }

        if (!constraints.isEmpty())
            UseCounter::count(context, UseCounter::RTCPeerConnectionCreateOfferLegacyConstraints);
        else
            UseCounter::count(context, UseCounter::RTCPeerConnectionCreateOfferLegacyCompliant);

        m_peerHandler->createOffer(request, constraints);
    }

    return ScriptPromise::castUndefined(scriptState);
}

ScriptPromise RTCPeerConnection::createAnswer(ScriptState* scriptState, const RTCAnswerOptions& options)
{
    if (m_signalingState == SignalingStateClosed)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(InvalidStateError, kSignalingStateClosedMessage));

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    RTCSessionDescriptionRequest* request = RTCSessionDescriptionRequestPromiseImpl::create(this, resolver);
    m_peerHandler->createAnswer(request, convertToWebRTCAnswerOptions(options));
    return promise;
}

ScriptPromise RTCPeerConnection::createAnswer(ScriptState* scriptState, RTCSessionDescriptionCallback* successCallback, RTCPeerConnectionErrorCallback* errorCallback, const Dictionary& mediaConstraints)
{
    DCHECK(successCallback);
    DCHECK(errorCallback);
    ExecutionContext* context = scriptState->getExecutionContext();
    UseCounter::count(context, UseCounter::RTCPeerConnectionCreateAnswerLegacyFailureCallback);
    if (mediaConstraints.isObject())
        UseCounter::count(context, UseCounter::RTCPeerConnectionCreateAnswerLegacyConstraints);
    else
        UseCounter::count(context, UseCounter::RTCPeerConnectionCreateAnswerLegacyCompliant);

    if (callErrorCallbackIfSignalingStateClosed(m_signalingState, errorCallback))
        return ScriptPromise::castUndefined(scriptState);

    MediaErrorState mediaErrorState;
    WebMediaConstraints constraints = MediaConstraintsImpl::create(context, mediaConstraints, mediaErrorState);
    // Report constraints parsing errors via the callback, but ignore unknown/unsupported constraints as they
    // would be silently discarded by WebIDL.
    if (mediaErrorState.canGenerateException()) {
        String errorMsg = mediaErrorState.getErrorMessage();
        asyncCallErrorCallback(errorCallback, DOMException::create(OperationError, errorMsg));
        return ScriptPromise::castUndefined(scriptState);
    }

    RTCSessionDescriptionRequest* request = RTCSessionDescriptionRequestImpl::create(getExecutionContext(), this, successCallback, errorCallback);
    m_peerHandler->createAnswer(request, constraints);
    return ScriptPromise::castUndefined(scriptState);
}

ScriptPromise RTCPeerConnection::setLocalDescription(ScriptState* scriptState, const RTCSessionDescriptionInit& sessionDescriptionInit)
{
    if (m_signalingState == SignalingStateClosed)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(InvalidStateError, kSignalingStateClosedMessage));

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    RTCVoidRequest* request = RTCVoidRequestPromiseImpl::create(this, resolver);
    m_peerHandler->setLocalDescription(request, WebRTCSessionDescription(sessionDescriptionInit.type(), sessionDescriptionInit.sdp()));
    return promise;
}

ScriptPromise RTCPeerConnection::setLocalDescription(ScriptState* scriptState, const RTCSessionDescriptionInit& sessionDescriptionInit, VoidCallback* successCallback, RTCPeerConnectionErrorCallback* errorCallback)
{
    ExecutionContext* context = scriptState->getExecutionContext();
    if (successCallback && errorCallback) {
        UseCounter::count(context, UseCounter::RTCPeerConnectionSetLocalDescriptionLegacyCompliant);
    } else {
        if (!successCallback)
            UseCounter::count(context, UseCounter::RTCPeerConnectionSetLocalDescriptionLegacyNoSuccessCallback);
        if (!errorCallback)
            UseCounter::count(context, UseCounter::RTCPeerConnectionSetLocalDescriptionLegacyNoFailureCallback);
    }

    if (callErrorCallbackIfSignalingStateClosed(m_signalingState, errorCallback))
        return ScriptPromise::castUndefined(scriptState);

    RTCVoidRequest* request = RTCVoidRequestImpl::create(getExecutionContext(), this, successCallback, errorCallback);
    m_peerHandler->setLocalDescription(request, WebRTCSessionDescription(sessionDescriptionInit.type(), sessionDescriptionInit.sdp()));
    return ScriptPromise::castUndefined(scriptState);
}

RTCSessionDescription* RTCPeerConnection::localDescription()
{
    WebRTCSessionDescription webSessionDescription = m_peerHandler->localDescription();
    if (webSessionDescription.isNull())
        return nullptr;

    return RTCSessionDescription::create(webSessionDescription);
}

ScriptPromise RTCPeerConnection::setRemoteDescription(ScriptState* scriptState, const RTCSessionDescriptionInit& sessionDescriptionInit)
{
    if (m_signalingState == SignalingStateClosed)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(InvalidStateError, kSignalingStateClosedMessage));

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    RTCVoidRequest* request = RTCVoidRequestPromiseImpl::create(this, resolver);
    m_peerHandler->setRemoteDescription(request, WebRTCSessionDescription(sessionDescriptionInit.type(), sessionDescriptionInit.sdp()));
    return promise;
}

ScriptPromise RTCPeerConnection::setRemoteDescription(ScriptState* scriptState, const RTCSessionDescriptionInit& sessionDescriptionInit, VoidCallback* successCallback, RTCPeerConnectionErrorCallback* errorCallback)
{
    ExecutionContext* context = scriptState->getExecutionContext();
    if (successCallback && errorCallback) {
        UseCounter::count(context, UseCounter::RTCPeerConnectionSetRemoteDescriptionLegacyCompliant);
    } else {
        if (!successCallback)
            UseCounter::count(context, UseCounter::RTCPeerConnectionSetRemoteDescriptionLegacyNoSuccessCallback);
        if (!errorCallback)
            UseCounter::count(context, UseCounter::RTCPeerConnectionSetRemoteDescriptionLegacyNoFailureCallback);
    }

    if (callErrorCallbackIfSignalingStateClosed(m_signalingState, errorCallback))
        return ScriptPromise::castUndefined(scriptState);

    RTCVoidRequest* request = RTCVoidRequestImpl::create(getExecutionContext(), this, successCallback, errorCallback);
    m_peerHandler->setRemoteDescription(request, WebRTCSessionDescription(sessionDescriptionInit.type(), sessionDescriptionInit.sdp()));
    return ScriptPromise::castUndefined(scriptState);
}

RTCSessionDescription* RTCPeerConnection::remoteDescription()
{
    WebRTCSessionDescription webSessionDescription = m_peerHandler->remoteDescription();
    if (webSessionDescription.isNull())
        return nullptr;

    return RTCSessionDescription::create(webSessionDescription);
}

void RTCPeerConnection::updateIce(ExecutionContext* context, const Dictionary& rtcConfiguration, const Dictionary& mediaConstraints, ExceptionState& exceptionState)
{
    if (throwExceptionIfSignalingStateClosed(m_signalingState, exceptionState))
        return;

    RtcpMuxPolicy selectedRtcpMuxPolicy = RtcpMuxPolicyDefault;
    RTCConfiguration* configuration = parseConfiguration(rtcConfiguration, exceptionState, &selectedRtcpMuxPolicy);

    if (exceptionState.hadException())
        return;

    MediaErrorState mediaErrorState;
    if (mediaErrorState.hadException()) {
        mediaErrorState.raiseException(exceptionState);
        return;
    }

    // Constraints are ignored.
    bool valid = m_peerHandler->updateICE(configuration);
    if (!valid)
        exceptionState.throwDOMException(SyntaxError, "Could not update the ICE Agent with the given configuration.");
}

ScriptPromise RTCPeerConnection::generateCertificate(ScriptState* scriptState, const AlgorithmIdentifier& keygenAlgorithm, ExceptionState& exceptionState)
{
    // Normalize |keygenAlgorithm| with WebCrypto, making sure it is a recognized AlgorithmIdentifier.
    WebCryptoAlgorithm cryptoAlgorithm;
    AlgorithmError error;
    if (!normalizeAlgorithm(keygenAlgorithm, WebCryptoOperationGenerateKey, cryptoAlgorithm, &error)) {
        // Reject generateCertificate with the same error as was produced by WebCrypto.
        // |result| is garbage collected, no need to delete.
        CryptoResultImpl* result = CryptoResultImpl::create(scriptState);
        ScriptPromise promise = result->promise();
        result->completeWithError(error.errorType, error.errorDetails);
        return promise;
    }

    // Check if |keygenAlgorithm| contains the optional DOMTimeStamp |expires| attribute.
    Nullable<DOMTimeStamp> expires;
    if (keygenAlgorithm.isDictionary()) {
        Dictionary keygenAlgorithmDict = keygenAlgorithm.getAsDictionary();
        if (keygenAlgorithmDict.hasProperty("expires")) {
            v8::Local<v8::Value> expiresValue;
            keygenAlgorithmDict.get("expires", expiresValue);
            if (expiresValue->IsNumber()) {
                double expiresDouble = expiresValue->ToNumber(scriptState->isolate()->GetCurrentContext()).ToLocalChecked()->Value();
                if (expiresDouble >= 0) {
                    expires.set(static_cast<DOMTimeStamp>(expiresDouble));
                }
            }
        }
    }

    // Convert from WebCrypto representation to recognized WebRTCKeyParams. WebRTC supports a small subset of what are valid AlgorithmIdentifiers.
    const char* unsupportedParamsString = "The 1st argument provided is an AlgorithmIdentifier with a supported algorithm name, but the parameters are not supported.";
    Nullable<WebRTCKeyParams> keyParams;
    switch (cryptoAlgorithm.id()) {
    case WebCryptoAlgorithmIdRsaSsaPkcs1v1_5:
        // name: "RSASSA-PKCS1-v1_5"
        unsigned publicExponent;
        // "publicExponent" must fit in an unsigned int. The only recognized "hash" is "SHA-256".
        if (cryptoAlgorithm.rsaHashedKeyGenParams()->convertPublicExponentToUnsigned(publicExponent)
            && cryptoAlgorithm.rsaHashedKeyGenParams()->hash().id() == WebCryptoAlgorithmIdSha256) {
            unsigned modulusLength = cryptoAlgorithm.rsaHashedKeyGenParams()->modulusLengthBits();
            keyParams.set(WebRTCKeyParams::createRSA(modulusLength, publicExponent));
        } else {
            return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError, unsupportedParamsString));
        }
        break;
    case WebCryptoAlgorithmIdEcdsa:
        // name: "ECDSA"
        // The only recognized "namedCurve" is "P-256".
        if (cryptoAlgorithm.ecKeyGenParams()->namedCurve() == WebCryptoNamedCurveP256) {
            keyParams.set(WebRTCKeyParams::createECDSA(WebRTCECCurveNistP256));
        } else {
            return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError, unsupportedParamsString));
        }
        break;
    default:
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError, "The 1st argument provided is an AlgorithmIdentifier, but the algorithm is not supported."));
        break;
    }
    DCHECK(!keyParams.isNull());

    std::unique_ptr<WebRTCCertificateGenerator> certificateGenerator = wrapUnique(
        Platform::current()->createRTCCertificateGenerator());

    // |keyParams| was successfully constructed, but does the certificate generator support these parameters?
    if (!certificateGenerator->isSupportedKeyParams(keyParams.get())) {
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError, unsupportedParamsString));
    }

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    std::unique_ptr<WebRTCCertificateObserver> certificateObserver(WebRTCCertificateObserver::create(resolver));

    // Generate certificate. The |certificateObserver| will resolve the promise asynchronously upon completion.
    // The observer will manage its own destruction as well as the resolver's destruction.
    if (expires.isNull()) {
        certificateGenerator->generateCertificate(
            keyParams.get(),
            std::move(certificateObserver));
    } else {
        certificateGenerator->generateCertificateWithExpiration(
            keyParams.get(),
            expires.get(),
            std::move(certificateObserver));
    }

    return promise;
}

ScriptPromise RTCPeerConnection::addIceCandidate(ScriptState* scriptState, const RTCIceCandidateInitOrRTCIceCandidate& candidate)
{
    if (m_signalingState == SignalingStateClosed)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(InvalidStateError, kSignalingStateClosedMessage));

    if (isIceCandidateMissingSdp(candidate))
        return ScriptPromise::reject(scriptState, V8ThrowException::createTypeError(scriptState->isolate(), "Candidate missing values for both sdpMid and sdpMLineIndex"));

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    RTCVoidRequest* request = RTCVoidRequestPromiseImpl::create(this, resolver);
    WebRTCICECandidate webCandidate = convertToWebRTCIceCandidate(scriptState->getExecutionContext(), candidate);
    bool implemented = m_peerHandler->addICECandidate(request, webCandidate);
    if (!implemented)
        resolver->reject(DOMException::create(OperationError, "This operation could not be completed."));

    return promise;
}

ScriptPromise RTCPeerConnection::addIceCandidate(ScriptState* scriptState, const RTCIceCandidateInitOrRTCIceCandidate& candidate, VoidCallback* successCallback, RTCPeerConnectionErrorCallback* errorCallback)
{
    DCHECK(successCallback);
    DCHECK(errorCallback);

    if (callErrorCallbackIfSignalingStateClosed(m_signalingState, errorCallback))
        return ScriptPromise::castUndefined(scriptState);

    if (isIceCandidateMissingSdp(candidate))
        return ScriptPromise::reject(scriptState, V8ThrowException::createTypeError(scriptState->isolate(), "Candidate missing values for both sdpMid and sdpMLineIndex"));

    RTCVoidRequest* request = RTCVoidRequestImpl::create(getExecutionContext(), this, successCallback, errorCallback);
    WebRTCICECandidate webCandidate = convertToWebRTCIceCandidate(scriptState->getExecutionContext(), candidate);
    bool implemented = m_peerHandler->addICECandidate(request, webCandidate);
    if (!implemented)
        asyncCallErrorCallback(errorCallback, DOMException::create(OperationError, "This operation could not be completed."));

    return ScriptPromise::castUndefined(scriptState);
}

String RTCPeerConnection::signalingState() const
{
    switch (m_signalingState) {
    case SignalingStateStable:
        return "stable";
    case SignalingStateHaveLocalOffer:
        return "have-local-offer";
    case SignalingStateHaveRemoteOffer:
        return "have-remote-offer";
    case SignalingStateHaveLocalPrAnswer:
        return "have-local-pranswer";
    case SignalingStateHaveRemotePrAnswer:
        return "have-remote-pranswer";
    case SignalingStateClosed:
        return "closed";
    }

    NOTREACHED();
    return String();
}

String RTCPeerConnection::iceGatheringState() const
{
    switch (m_iceGatheringState) {
    case ICEGatheringStateNew:
        return "new";
    case ICEGatheringStateGathering:
        return "gathering";
    case ICEGatheringStateComplete:
        return "complete";
    }

    NOTREACHED();
    return String();
}

String RTCPeerConnection::iceConnectionState() const
{
    switch (m_iceConnectionState) {
    case ICEConnectionStateNew:
        return "new";
    case ICEConnectionStateChecking:
        return "checking";
    case ICEConnectionStateConnected:
        return "connected";
    case ICEConnectionStateCompleted:
        return "completed";
    case ICEConnectionStateFailed:
        return "failed";
    case ICEConnectionStateDisconnected:
        return "disconnected";
    case ICEConnectionStateClosed:
        return "closed";
    }

    NOTREACHED();
    return String();
}

void RTCPeerConnection::addStream(ExecutionContext* context, MediaStream* stream, const Dictionary& mediaConstraints, ExceptionState& exceptionState)
{
    if (throwExceptionIfSignalingStateClosed(m_signalingState, exceptionState))
        return;

    if (!stream) {
        exceptionState.throwDOMException(TypeMismatchError, ExceptionMessages::argumentNullOrIncorrectType(1, "MediaStream"));
        return;
    }

    if (m_localStreams.contains(stream))
        return;

    MediaErrorState mediaErrorState;
    WebMediaConstraints constraints = MediaConstraintsImpl::create(context, mediaConstraints, mediaErrorState);
    if (mediaErrorState.hadException()) {
        mediaErrorState.raiseException(exceptionState);
        return;
    }

    m_localStreams.append(stream);

    bool valid = m_peerHandler->addStream(stream->descriptor(), constraints);
    if (!valid)
        exceptionState.throwDOMException(SyntaxError, "Unable to add the provided stream.");
}

void RTCPeerConnection::removeStream(MediaStream* stream, ExceptionState& exceptionState)
{
    if (throwExceptionIfSignalingStateClosed(m_signalingState, exceptionState))
        return;

    if (!stream) {
        exceptionState.throwDOMException(TypeMismatchError, ExceptionMessages::argumentNullOrIncorrectType(1, "MediaStream"));
        return;
    }

    size_t pos = m_localStreams.find(stream);
    if (pos == kNotFound)
        return;

    m_localStreams.remove(pos);

    m_peerHandler->removeStream(stream->descriptor());
}

MediaStreamVector RTCPeerConnection::getLocalStreams() const
{
    return m_localStreams;
}

MediaStreamVector RTCPeerConnection::getRemoteStreams() const
{
    return m_remoteStreams;
}

MediaStream* RTCPeerConnection::getStreamById(const String& streamId)
{
    for (MediaStreamVector::iterator iter = m_localStreams.begin(); iter != m_localStreams.end(); ++iter) {
        if ((*iter)->id() == streamId)
            return iter->get();
    }

    for (MediaStreamVector::iterator iter = m_remoteStreams.begin(); iter != m_remoteStreams.end(); ++iter) {
        if ((*iter)->id() == streamId)
            return iter->get();
    }

    return 0;
}

void RTCPeerConnection::getStats(ExecutionContext* context, RTCStatsCallback* successCallback, MediaStreamTrack* selector)
{
    UseCounter::count(context, UseCounter::RTCPeerConnectionGetStatsLegacyNonCompliant);
    RTCStatsRequest* statsRequest = RTCStatsRequestImpl::create(getExecutionContext(), this, successCallback, selector);
    // FIXME: Add passing selector as part of the statsRequest.
    m_peerHandler->getStats(statsRequest);
}

RTCDataChannel* RTCPeerConnection::createDataChannel(String label, const Dictionary& options, ExceptionState& exceptionState)
{
    if (throwExceptionIfSignalingStateClosed(m_signalingState, exceptionState))
        return nullptr;

    WebRTCDataChannelInit init;
    DictionaryHelper::get(options, "ordered", init.ordered);
    DictionaryHelper::get(options, "negotiated", init.negotiated);

    unsigned short value = 0;
    if (DictionaryHelper::get(options, "id", value))
        init.id = value;
    if (DictionaryHelper::get(options, "maxRetransmits", value))
        init.maxRetransmits = value;
    if (DictionaryHelper::get(options, "maxRetransmitTime", value))
        init.maxRetransmitTime = value;

    String protocolString;
    DictionaryHelper::get(options, "protocol", protocolString);
    init.protocol = protocolString;

    RTCDataChannel* channel = RTCDataChannel::create(getExecutionContext(), m_peerHandler.get(), label, init, exceptionState);
    if (exceptionState.hadException())
        return nullptr;
    RTCDataChannel::ReadyState handlerState = channel->getHandlerState();
    if (handlerState != RTCDataChannel::ReadyStateConnecting) {
        // There was an early state transition.  Don't miss it!
        channel->didChangeReadyState(handlerState);
    }
    m_hasDataChannels = true;

    return channel;
}

bool RTCPeerConnection::hasLocalStreamWithTrackId(const String& trackId)
{
    for (MediaStreamVector::iterator iter = m_localStreams.begin(); iter != m_localStreams.end(); ++iter) {
        if ((*iter)->getTrackById(trackId))
            return true;
    }
    return false;
}

RTCDTMFSender* RTCPeerConnection::createDTMFSender(MediaStreamTrack* track, ExceptionState& exceptionState)
{
    if (throwExceptionIfSignalingStateClosed(m_signalingState, exceptionState))
        return nullptr;

    DCHECK(track);

    if (!hasLocalStreamWithTrackId(track->id())) {
        exceptionState.throwDOMException(SyntaxError, "No local stream is available for the track provided.");
        return nullptr;
    }

    RTCDTMFSender* dtmfSender = RTCDTMFSender::create(getExecutionContext(), m_peerHandler.get(), track, exceptionState);
    if (exceptionState.hadException())
        return nullptr;
    return dtmfSender;
}

void RTCPeerConnection::close(ExceptionState& exceptionState)
{
    if (throwExceptionIfSignalingStateClosed(m_signalingState, exceptionState))
        return;

    closeInternal();
}

void RTCPeerConnection::negotiationNeeded()
{
    DCHECK(!m_closed);
    scheduleDispatchEvent(Event::create(EventTypeNames::negotiationneeded));
}

void RTCPeerConnection::didGenerateICECandidate(const WebRTCICECandidate& webCandidate)
{
    DCHECK(!m_closed);
    DCHECK(getExecutionContext()->isContextThread());
    if (webCandidate.isNull()) {
        scheduleDispatchEvent(RTCIceCandidateEvent::create(false, false, nullptr));
    } else {
        RTCIceCandidate* iceCandidate = RTCIceCandidate::create(webCandidate);
        scheduleDispatchEvent(RTCIceCandidateEvent::create(false, false, iceCandidate));
    }
}

void RTCPeerConnection::didChangeSignalingState(SignalingState newState)
{
    DCHECK(!m_closed);
    DCHECK(getExecutionContext()->isContextThread());
    changeSignalingState(newState);
}

void RTCPeerConnection::didChangeICEGatheringState(ICEGatheringState newState)
{
    DCHECK(!m_closed);
    DCHECK(getExecutionContext()->isContextThread());
    changeIceGatheringState(newState);
}

void RTCPeerConnection::didChangeICEConnectionState(ICEConnectionState newState)
{
    DCHECK(!m_closed);
    DCHECK(getExecutionContext()->isContextThread());
    changeIceConnectionState(newState);
}

void RTCPeerConnection::didAddRemoteStream(const WebMediaStream& remoteStream)
{
    DCHECK(!m_closed);
    DCHECK(getExecutionContext()->isContextThread());

    if (m_signalingState == SignalingStateClosed)
        return;

    MediaStream* stream = MediaStream::create(getExecutionContext(), remoteStream);
    m_remoteStreams.append(stream);

    scheduleDispatchEvent(MediaStreamEvent::create(EventTypeNames::addstream, false, false, stream));
}

void RTCPeerConnection::didRemoveRemoteStream(const WebMediaStream& remoteStream)
{
    DCHECK(!m_closed);
    DCHECK(getExecutionContext()->isContextThread());

    MediaStreamDescriptor* streamDescriptor = remoteStream;
    DCHECK(streamDescriptor->client());

    MediaStream* stream = static_cast<MediaStream*>(streamDescriptor->client());
    stream->streamEnded();

    if (m_signalingState == SignalingStateClosed)
        return;

    size_t pos = m_remoteStreams.find(stream);
    DCHECK(pos != kNotFound);
    m_remoteStreams.remove(pos);

    scheduleDispatchEvent(MediaStreamEvent::create(EventTypeNames::removestream, false, false, stream));
}

void RTCPeerConnection::didAddRemoteDataChannel(WebRTCDataChannelHandler* handler)
{
    DCHECK(!m_closed);
    DCHECK(getExecutionContext()->isContextThread());

    if (m_signalingState == SignalingStateClosed)
        return;

    RTCDataChannel* channel = RTCDataChannel::create(getExecutionContext(), wrapUnique(handler));
    scheduleDispatchEvent(RTCDataChannelEvent::create(EventTypeNames::datachannel, false, false, channel));
    m_hasDataChannels = true;
}

void RTCPeerConnection::releasePeerConnectionHandler()
{
    stop();
}

void RTCPeerConnection::closePeerConnection()
{
    DCHECK(m_signalingState != RTCPeerConnection::SignalingStateClosed);
    closeInternal();
}

const AtomicString& RTCPeerConnection::interfaceName() const
{
    return EventTargetNames::RTCPeerConnection;
}

ExecutionContext* RTCPeerConnection::getExecutionContext() const
{
    return ActiveDOMObject::getExecutionContext();
}

void RTCPeerConnection::suspend()
{
    m_dispatchScheduledEventRunner->suspend();
}

void RTCPeerConnection::resume()
{
    m_dispatchScheduledEventRunner->resume();
}

void RTCPeerConnection::stop()
{
    if (m_stopped)
        return;

    m_stopped = true;
    m_iceConnectionState = ICEConnectionStateClosed;
    m_signalingState = SignalingStateClosed;

    m_dispatchScheduledEventRunner->stop();

    m_peerHandler.reset();
}

void RTCPeerConnection::changeSignalingState(SignalingState signalingState)
{
    if (m_signalingState != SignalingStateClosed && m_signalingState != signalingState) {
        m_signalingState = signalingState;
        scheduleDispatchEvent(Event::create(EventTypeNames::signalingstatechange));
    }
}

void RTCPeerConnection::changeIceGatheringState(ICEGatheringState iceGatheringState)
{
    m_iceGatheringState = iceGatheringState;
}

bool RTCPeerConnection::setIceConnectionState(ICEConnectionState iceConnectionState)
{
    if (m_iceConnectionState != ICEConnectionStateClosed && m_iceConnectionState != iceConnectionState) {
        m_iceConnectionState = iceConnectionState;
        if (m_iceConnectionState == ICEConnectionStateConnected)
            recordRapporMetrics();

        return true;
    }
    return false;
}

void RTCPeerConnection::changeIceConnectionState(ICEConnectionState iceConnectionState)
{
    if (m_iceConnectionState != ICEConnectionStateClosed) {
        scheduleDispatchEvent(Event::create(EventTypeNames::iceconnectionstatechange),
            WTF::bind(&RTCPeerConnection::setIceConnectionState, wrapPersistent(this), iceConnectionState));
    }
}

void RTCPeerConnection::closeInternal()
{
    DCHECK(m_signalingState != RTCPeerConnection::SignalingStateClosed);
    m_peerHandler->stop();
    m_closed = true;

    changeIceConnectionState(ICEConnectionStateClosed);
    changeIceGatheringState(ICEGatheringStateComplete);
    changeSignalingState(SignalingStateClosed);
}

void RTCPeerConnection::scheduleDispatchEvent(Event* event)
{
    scheduleDispatchEvent(event, nullptr);
}

void RTCPeerConnection::scheduleDispatchEvent(Event* event,
    std::unique_ptr<BoolFunction> setupFunction)
{
    m_scheduledEvents.append(new EventWrapper(event, std::move(setupFunction)));

    m_dispatchScheduledEventRunner->runAsync();
}

void RTCPeerConnection::dispatchScheduledEvent()
{
    if (m_stopped)
        return;

    HeapVector<Member<EventWrapper>> events;
    events.swap(m_scheduledEvents);

    HeapVector<Member<EventWrapper>>::iterator it = events.begin();
    for (; it != events.end(); ++it) {
        if ((*it)->setup()) {
            dispatchEvent((*it)->m_event.release());
        }
    }

    events.clear();
}

void RTCPeerConnection::recordRapporMetrics()
{
    Document* document = toDocument(getExecutionContext());
    for (const auto& stream : m_localStreams) {
        if (stream->getAudioTracks().size() > 0)
            HostsUsingFeatures::countAnyWorld(*document, HostsUsingFeatures::Feature::RTCPeerConnectionAudio);
        if (stream->getVideoTracks().size() > 0)
            HostsUsingFeatures::countAnyWorld(*document, HostsUsingFeatures::Feature::RTCPeerConnectionVideo);
    }

    for (const auto& stream : m_remoteStreams) {
        if (stream->getAudioTracks().size() > 0)
            HostsUsingFeatures::countAnyWorld(*document, HostsUsingFeatures::Feature::RTCPeerConnectionAudio);
        if (stream->getVideoTracks().size() > 0)
            HostsUsingFeatures::countAnyWorld(*document, HostsUsingFeatures::Feature::RTCPeerConnectionVideo);
    }

    if (m_hasDataChannels)
        HostsUsingFeatures::countAnyWorld(*document, HostsUsingFeatures::Feature::RTCPeerConnectionDataChannel);

}

DEFINE_TRACE(RTCPeerConnection)
{
    visitor->trace(m_localStreams);
    visitor->trace(m_remoteStreams);
    visitor->trace(m_dispatchScheduledEventRunner);
    visitor->trace(m_scheduledEvents);
    EventTargetWithInlineData::trace(visitor);
    ActiveDOMObject::trace(visitor);
}

} // namespace blink
