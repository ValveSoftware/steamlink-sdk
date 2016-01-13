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

#include "config.h"

#include "modules/mediastream/RTCPeerConnection.h"

#include "bindings/v8/ArrayValue.h"
#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "core/html/VoidCallback.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "modules/mediastream/MediaConstraintsImpl.h"
#include "modules/mediastream/MediaStreamEvent.h"
#include "modules/mediastream/RTCDTMFSender.h"
#include "modules/mediastream/RTCDataChannel.h"
#include "modules/mediastream/RTCDataChannelEvent.h"
#include "modules/mediastream/RTCErrorCallback.h"
#include "modules/mediastream/RTCIceCandidateEvent.h"
#include "modules/mediastream/RTCSessionDescription.h"
#include "modules/mediastream/RTCSessionDescriptionCallback.h"
#include "modules/mediastream/RTCSessionDescriptionRequestImpl.h"
#include "modules/mediastream/RTCStatsCallback.h"
#include "modules/mediastream/RTCStatsRequestImpl.h"
#include "modules/mediastream/RTCVoidRequestImpl.h"
#include "platform/mediastream/RTCConfiguration.h"
#include "public/platform/Platform.h"
#include "public/platform/WebMediaStream.h"
#include "public/platform/WebRTCConfiguration.h"
#include "public/platform/WebRTCDataChannelHandler.h"
#include "public/platform/WebRTCDataChannelInit.h"
#include "public/platform/WebRTCICECandidate.h"
#include "public/platform/WebRTCSessionDescription.h"
#include "public/platform/WebRTCSessionDescriptionRequest.h"
#include "public/platform/WebRTCStatsRequest.h"
#include "public/platform/WebRTCVoidRequest.h"

namespace WebCore {

namespace {

static bool throwExceptionIfSignalingStateClosed(RTCPeerConnection::SignalingState state, ExceptionState& exceptionState)
{
    if (state == RTCPeerConnection::SignalingStateClosed) {
        exceptionState.throwDOMException(InvalidStateError, "The RTCPeerConnection's signalingState is 'closed'.");
        return true;
    }

    return false;
}

} // namespace

PassRefPtr<RTCConfiguration> RTCPeerConnection::parseConfiguration(const Dictionary& configuration, ExceptionState& exceptionState)
{
    if (configuration.isUndefinedOrNull())
        return nullptr;

    ArrayValue iceServers;
    bool ok = configuration.get("iceServers", iceServers);
    if (!ok || iceServers.isUndefinedOrNull()) {
        exceptionState.throwTypeError("Malformed RTCConfiguration");
        return nullptr;
    }

    size_t numberOfServers;
    ok = iceServers.length(numberOfServers);
    if (!ok) {
        exceptionState.throwTypeError("Malformed RTCConfiguration");
        return nullptr;
    }

    RefPtr<RTCConfiguration> rtcConfiguration = RTCConfiguration::create();

    for (size_t i = 0; i < numberOfServers; ++i) {
        Dictionary iceServer;
        ok = iceServers.get(i, iceServer);
        if (!ok) {
            exceptionState.throwTypeError("Malformed RTCIceServer");
            return nullptr;
        }

        Vector<String> names;
        iceServer.getOwnPropertyNames(names);

        Vector<String> urlStrings;
        if (names.contains("urls")) {
            if (!iceServer.get("urls", urlStrings) || !urlStrings.size()) {
                String urlString;
                if (iceServer.get("urls", urlString)) {
                    urlStrings.append(urlString);
                } else {
                    exceptionState.throwTypeError("Malformed RTCIceServer");
                    return nullptr;
                }
            }
        } else if (names.contains("url")) {
            String urlString;
            if (iceServer.get("url", urlString)) {
                urlStrings.append(urlString);
            } else {
                exceptionState.throwTypeError("Malformed RTCIceServer");
                return nullptr;
            }
        } else {
            exceptionState.throwTypeError("Malformed RTCIceServer");
            return nullptr;
        }

        String username, credential;
        iceServer.get("username", username);
        iceServer.get("credential", credential);

        for (Vector<String>::iterator iter = urlStrings.begin(); iter != urlStrings.end(); ++iter) {
            KURL url(KURL(), *iter);
            if (!url.isValid() || !(url.protocolIs("turn") || url.protocolIs("turns") || url.protocolIs("stun"))) {
                exceptionState.throwTypeError("Malformed URL");
                return nullptr;
            }

            rtcConfiguration->appendServer(RTCIceServer::create(url, username, credential));
        }
    }

    return rtcConfiguration.release();
}

PassRefPtrWillBeRawPtr<RTCPeerConnection> RTCPeerConnection::create(ExecutionContext* context, const Dictionary& rtcConfiguration, const Dictionary& mediaConstraints, ExceptionState& exceptionState)
{
    RefPtr<RTCConfiguration> configuration = parseConfiguration(rtcConfiguration, exceptionState);
    if (exceptionState.hadException())
        return nullptr;

    blink::WebMediaConstraints constraints = MediaConstraintsImpl::create(mediaConstraints, exceptionState);
    if (exceptionState.hadException())
        return nullptr;

    RefPtrWillBeRawPtr<RTCPeerConnection> peerConnection = adoptRefWillBeRefCountedGarbageCollected(new RTCPeerConnection(context, configuration.release(), constraints, exceptionState));
    peerConnection->suspendIfNeeded();
    if (exceptionState.hadException())
        return nullptr;

    return peerConnection.release();
}

RTCPeerConnection::RTCPeerConnection(ExecutionContext* context, PassRefPtr<RTCConfiguration> configuration, blink::WebMediaConstraints constraints, ExceptionState& exceptionState)
    : ActiveDOMObject(context)
    , m_signalingState(SignalingStateStable)
    , m_iceGatheringState(ICEGatheringStateNew)
    , m_iceConnectionState(ICEConnectionStateNew)
    , m_dispatchScheduledEventRunner(this, &RTCPeerConnection::dispatchScheduledEvent)
    , m_stopped(false)
    , m_closed(false)
{
    ScriptWrappable::init(this);
    Document* document = toDocument(executionContext());

    // If we fail, set |m_closed| and |m_stopped| to true, to avoid hitting the assert in the destructor.

    if (!document->frame()) {
        m_closed = true;
        m_stopped = true;
        exceptionState.throwDOMException(NotSupportedError, "PeerConnections may not be created in detached documents.");
        return;
    }

    m_peerHandler = adoptPtr(blink::Platform::current()->createRTCPeerConnectionHandler(this));
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
    ASSERT(m_closed || m_stopped);

#if !ENABLE(OILPAN)
    stop();
#endif
}

void RTCPeerConnection::createOffer(PassOwnPtr<RTCSessionDescriptionCallback> successCallback, PassOwnPtr<RTCErrorCallback> errorCallback, const Dictionary& mediaConstraints, ExceptionState& exceptionState)
{
    if (throwExceptionIfSignalingStateClosed(m_signalingState, exceptionState))
        return;

    ASSERT(successCallback);

    blink::WebMediaConstraints constraints = MediaConstraintsImpl::create(mediaConstraints, exceptionState);
    if (exceptionState.hadException())
        return;

    RefPtr<RTCSessionDescriptionRequest> request = RTCSessionDescriptionRequestImpl::create(executionContext(), this, successCallback, errorCallback);
    m_peerHandler->createOffer(request.release(), constraints);
}

void RTCPeerConnection::createAnswer(PassOwnPtr<RTCSessionDescriptionCallback> successCallback, PassOwnPtr<RTCErrorCallback> errorCallback, const Dictionary& mediaConstraints, ExceptionState& exceptionState)
{
    if (throwExceptionIfSignalingStateClosed(m_signalingState, exceptionState))
        return;

    ASSERT(successCallback);

    blink::WebMediaConstraints constraints = MediaConstraintsImpl::create(mediaConstraints, exceptionState);
    if (exceptionState.hadException())
        return;

    RefPtr<RTCSessionDescriptionRequest> request = RTCSessionDescriptionRequestImpl::create(executionContext(), this, successCallback, errorCallback);
    m_peerHandler->createAnswer(request.release(), constraints);
}

void RTCPeerConnection::setLocalDescription(PassRefPtrWillBeRawPtr<RTCSessionDescription> prpSessionDescription, PassOwnPtr<VoidCallback> successCallback, PassOwnPtr<RTCErrorCallback> errorCallback, ExceptionState& exceptionState)
{
    if (throwExceptionIfSignalingStateClosed(m_signalingState, exceptionState))
        return;

    RefPtrWillBeRawPtr<RTCSessionDescription> sessionDescription = prpSessionDescription;
    if (!sessionDescription) {
        exceptionState.throwDOMException(TypeMismatchError, ExceptionMessages::argumentNullOrIncorrectType(1, "RTCSessionDescription"));
        return;
    }

    RefPtr<RTCVoidRequest> request = RTCVoidRequestImpl::create(executionContext(), this, successCallback, errorCallback);
    m_peerHandler->setLocalDescription(request.release(), sessionDescription->webSessionDescription());
}

PassRefPtrWillBeRawPtr<RTCSessionDescription> RTCPeerConnection::localDescription(ExceptionState& exceptionState)
{
    blink::WebRTCSessionDescription webSessionDescription = m_peerHandler->localDescription();
    if (webSessionDescription.isNull())
        return nullptr;

    return RTCSessionDescription::create(webSessionDescription);
}

void RTCPeerConnection::setRemoteDescription(PassRefPtrWillBeRawPtr<RTCSessionDescription> prpSessionDescription, PassOwnPtr<VoidCallback> successCallback, PassOwnPtr<RTCErrorCallback> errorCallback, ExceptionState& exceptionState)
{
    if (throwExceptionIfSignalingStateClosed(m_signalingState, exceptionState))
        return;

    RefPtrWillBeRawPtr<RTCSessionDescription> sessionDescription = prpSessionDescription;
    if (!sessionDescription) {
        exceptionState.throwDOMException(TypeMismatchError, ExceptionMessages::argumentNullOrIncorrectType(1, "RTCSessionDescription"));
        return;
    }

    RefPtr<RTCVoidRequest> request = RTCVoidRequestImpl::create(executionContext(), this, successCallback, errorCallback);
    m_peerHandler->setRemoteDescription(request.release(), sessionDescription->webSessionDescription());
}

PassRefPtrWillBeRawPtr<RTCSessionDescription> RTCPeerConnection::remoteDescription(ExceptionState& exceptionState)
{
    blink::WebRTCSessionDescription webSessionDescription = m_peerHandler->remoteDescription();
    if (webSessionDescription.isNull())
        return nullptr;

    return RTCSessionDescription::create(webSessionDescription);
}

void RTCPeerConnection::updateIce(const Dictionary& rtcConfiguration, const Dictionary& mediaConstraints, ExceptionState& exceptionState)
{
    if (throwExceptionIfSignalingStateClosed(m_signalingState, exceptionState))
        return;

    RefPtr<RTCConfiguration> configuration = parseConfiguration(rtcConfiguration, exceptionState);
    if (exceptionState.hadException())
        return;

    blink::WebMediaConstraints constraints = MediaConstraintsImpl::create(mediaConstraints, exceptionState);
    if (exceptionState.hadException())
        return;

    bool valid = m_peerHandler->updateICE(configuration.release(), constraints);
    if (!valid)
        exceptionState.throwDOMException(SyntaxError, "Could not update the ICE Agent with the given configuration.");
}

void RTCPeerConnection::addIceCandidate(RTCIceCandidate* iceCandidate, ExceptionState& exceptionState)
{
    if (throwExceptionIfSignalingStateClosed(m_signalingState, exceptionState))
        return;

    if (!iceCandidate) {
        exceptionState.throwDOMException(TypeMismatchError, ExceptionMessages::argumentNullOrIncorrectType(1, "RTCIceCandidate"));
        return;
    }

    bool valid = m_peerHandler->addICECandidate(iceCandidate->webCandidate());
    if (!valid)
        exceptionState.throwDOMException(SyntaxError, "The ICE candidate could not be added.");
}

void RTCPeerConnection::addIceCandidate(RTCIceCandidate* iceCandidate, PassOwnPtr<VoidCallback> successCallback, PassOwnPtr<RTCErrorCallback> errorCallback, ExceptionState& exceptionState)
{
    if (throwExceptionIfSignalingStateClosed(m_signalingState, exceptionState))
        return;

    if (!iceCandidate) {
        exceptionState.throwDOMException(TypeMismatchError, ExceptionMessages::argumentNullOrIncorrectType(1, "RTCIceCandidate"));
        return;
    }
    ASSERT(successCallback);
    ASSERT(errorCallback);

    RefPtr<RTCVoidRequest> request = RTCVoidRequestImpl::create(executionContext(), this, successCallback, errorCallback);

    bool implemented = m_peerHandler->addICECandidate(request.release(), iceCandidate->webCandidate());
    if (!implemented) {
        exceptionState.throwDOMException(NotSupportedError, "This method is not yet implemented.");
    }
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

    ASSERT_NOT_REACHED();
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

    ASSERT_NOT_REACHED();
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

    ASSERT_NOT_REACHED();
    return String();
}

void RTCPeerConnection::addStream(PassRefPtrWillBeRawPtr<MediaStream> prpStream, const Dictionary& mediaConstraints, ExceptionState& exceptionState)
{
    if (throwExceptionIfSignalingStateClosed(m_signalingState, exceptionState))
        return;

    RefPtrWillBeRawPtr<MediaStream> stream = prpStream;
    if (!stream) {
        exceptionState.throwDOMException(TypeMismatchError, ExceptionMessages::argumentNullOrIncorrectType(1, "MediaStream"));
        return;
    }

    if (m_localStreams.contains(stream))
        return;

    blink::WebMediaConstraints constraints = MediaConstraintsImpl::create(mediaConstraints, exceptionState);
    if (exceptionState.hadException())
        return;

    m_localStreams.append(stream);

    bool valid = m_peerHandler->addStream(stream->descriptor(), constraints);
    if (!valid)
        exceptionState.throwDOMException(SyntaxError, "Unable to add the provided stream.");
}

void RTCPeerConnection::removeStream(PassRefPtrWillBeRawPtr<MediaStream> prpStream, ExceptionState& exceptionState)
{
    if (throwExceptionIfSignalingStateClosed(m_signalingState, exceptionState))
        return;

    if (!prpStream) {
        exceptionState.throwDOMException(TypeMismatchError, ExceptionMessages::argumentNullOrIncorrectType(1, "MediaStream"));
        return;
    }

    RefPtrWillBeRawPtr<MediaStream> stream = prpStream;

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

void RTCPeerConnection::getStats(PassOwnPtr<RTCStatsCallback> successCallback, PassRefPtr<MediaStreamTrack> selector)
{
    RefPtr<RTCStatsRequest> statsRequest = RTCStatsRequestImpl::create(executionContext(), this, successCallback, selector);
    // FIXME: Add passing selector as part of the statsRequest.
    m_peerHandler->getStats(statsRequest.release());
}

PassRefPtrWillBeRawPtr<RTCDataChannel> RTCPeerConnection::createDataChannel(String label, const Dictionary& options, ExceptionState& exceptionState)
{
    if (throwExceptionIfSignalingStateClosed(m_signalingState, exceptionState))
        return nullptr;

    blink::WebRTCDataChannelInit init;
    options.get("ordered", init.ordered);
    options.get("negotiated", init.negotiated);

    unsigned short value = 0;
    if (options.get("id", value))
        init.id = value;
    if (options.get("maxRetransmits", value))
        init.maxRetransmits = value;
    if (options.get("maxRetransmitTime", value))
        init.maxRetransmitTime = value;

    String protocolString;
    options.get("protocol", protocolString);
    init.protocol = protocolString;

    RefPtrWillBeRawPtr<RTCDataChannel> channel = RTCDataChannel::create(executionContext(), this, m_peerHandler.get(), label, init, exceptionState);
    if (exceptionState.hadException())
        return nullptr;
    m_dataChannels.append(channel);
    return channel.release();
}

bool RTCPeerConnection::hasLocalStreamWithTrackId(const String& trackId)
{
    for (MediaStreamVector::iterator iter = m_localStreams.begin(); iter != m_localStreams.end(); ++iter) {
        if ((*iter)->getTrackById(trackId))
            return true;
    }
    return false;
}

PassRefPtrWillBeRawPtr<RTCDTMFSender> RTCPeerConnection::createDTMFSender(PassRefPtrWillBeRawPtr<MediaStreamTrack> prpTrack, ExceptionState& exceptionState)
{
    if (throwExceptionIfSignalingStateClosed(m_signalingState, exceptionState))
        return nullptr;

    if (!prpTrack) {
        exceptionState.throwTypeError(ExceptionMessages::argumentNullOrIncorrectType(1, "MediaStreamTrack"));
        return nullptr;
    }

    RefPtrWillBeRawPtr<MediaStreamTrack> track = prpTrack;

    if (!hasLocalStreamWithTrackId(track->id())) {
        exceptionState.throwDOMException(SyntaxError, "No local stream is available for the track provided.");
        return nullptr;
    }

    RefPtrWillBeRawPtr<RTCDTMFSender> dtmfSender = RTCDTMFSender::create(executionContext(), m_peerHandler.get(), track.release(), exceptionState);
    if (exceptionState.hadException())
        return nullptr;
    return dtmfSender.release();
}

void RTCPeerConnection::close(ExceptionState& exceptionState)
{
    if (throwExceptionIfSignalingStateClosed(m_signalingState, exceptionState))
        return;

    m_peerHandler->stop();
    m_closed = true;

    changeIceConnectionState(ICEConnectionStateClosed);
    changeIceGatheringState(ICEGatheringStateComplete);
    changeSignalingState(SignalingStateClosed);
}

void RTCPeerConnection::negotiationNeeded()
{
    ASSERT(!m_closed);
    scheduleDispatchEvent(Event::create(EventTypeNames::negotiationneeded));
}

void RTCPeerConnection::didGenerateICECandidate(const blink::WebRTCICECandidate& webCandidate)
{
    ASSERT(!m_closed);
    ASSERT(executionContext()->isContextThread());
    if (webCandidate.isNull())
        scheduleDispatchEvent(RTCIceCandidateEvent::create(false, false, nullptr));
    else {
        RefPtrWillBeRawPtr<RTCIceCandidate> iceCandidate = RTCIceCandidate::create(webCandidate);
        scheduleDispatchEvent(RTCIceCandidateEvent::create(false, false, iceCandidate.release()));
    }
}

void RTCPeerConnection::didChangeSignalingState(SignalingState newState)
{
    ASSERT(!m_closed);
    ASSERT(executionContext()->isContextThread());
    changeSignalingState(newState);
}

void RTCPeerConnection::didChangeICEGatheringState(ICEGatheringState newState)
{
    ASSERT(!m_closed);
    ASSERT(executionContext()->isContextThread());
    changeIceGatheringState(newState);
}

void RTCPeerConnection::didChangeICEConnectionState(ICEConnectionState newState)
{
    ASSERT(!m_closed);
    ASSERT(executionContext()->isContextThread());
    changeIceConnectionState(newState);
}

void RTCPeerConnection::didAddRemoteStream(const blink::WebMediaStream& remoteStream)
{
    ASSERT(!m_closed);
    ASSERT(executionContext()->isContextThread());

    if (m_signalingState == SignalingStateClosed)
        return;

    RefPtrWillBeRawPtr<MediaStream> stream = MediaStream::create(executionContext(), remoteStream);
    m_remoteStreams.append(stream);

    scheduleDispatchEvent(MediaStreamEvent::create(EventTypeNames::addstream, false, false, stream.release()));
}

void RTCPeerConnection::didRemoveRemoteStream(const blink::WebMediaStream& remoteStream)
{
    ASSERT(!m_closed);
    ASSERT(executionContext()->isContextThread());

    MediaStreamDescriptor* streamDescriptor = remoteStream;
    ASSERT(streamDescriptor->client());

    RefPtrWillBeRawPtr<MediaStream> stream = static_cast<MediaStream*>(streamDescriptor->client());
    stream->streamEnded();

    if (m_signalingState == SignalingStateClosed)
        return;

    size_t pos = m_remoteStreams.find(stream);
    ASSERT(pos != kNotFound);
    m_remoteStreams.remove(pos);

    scheduleDispatchEvent(MediaStreamEvent::create(EventTypeNames::removestream, false, false, stream.release()));
}

void RTCPeerConnection::didAddRemoteDataChannel(blink::WebRTCDataChannelHandler* handler)
{
    ASSERT(!m_closed);
    ASSERT(executionContext()->isContextThread());

    if (m_signalingState == SignalingStateClosed)
        return;

    RefPtrWillBeRawPtr<RTCDataChannel> channel = RTCDataChannel::create(executionContext(), this, adoptPtr(handler));
    m_dataChannels.append(channel);

    scheduleDispatchEvent(RTCDataChannelEvent::create(EventTypeNames::datachannel, false, false, channel.release()));
}

void RTCPeerConnection::releasePeerConnectionHandler()
{
    stop();
}

const AtomicString& RTCPeerConnection::interfaceName() const
{
    return EventTargetNames::RTCPeerConnection;
}

ExecutionContext* RTCPeerConnection::executionContext() const
{
    return ActiveDOMObject::executionContext();
}

void RTCPeerConnection::suspend()
{
    m_dispatchScheduledEventRunner.suspend();
}

void RTCPeerConnection::resume()
{
    m_dispatchScheduledEventRunner.resume();
}

void RTCPeerConnection::stop()
{
    if (m_stopped)
        return;

    m_stopped = true;
    m_iceConnectionState = ICEConnectionStateClosed;
    m_signalingState = SignalingStateClosed;

    WillBeHeapVector<RefPtrWillBeMember<RTCDataChannel> >::iterator i = m_dataChannels.begin();
    for (; i != m_dataChannels.end(); ++i)
        (*i)->stop();
    m_dataChannels.clear();

    m_dispatchScheduledEventRunner.stop();

    m_peerHandler.clear();
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

void RTCPeerConnection::changeIceConnectionState(ICEConnectionState iceConnectionState)
{
    if (m_iceConnectionState != ICEConnectionStateClosed && m_iceConnectionState != iceConnectionState) {
        m_iceConnectionState = iceConnectionState;
        scheduleDispatchEvent(Event::create(EventTypeNames::iceconnectionstatechange));
    }
}

void RTCPeerConnection::scheduleDispatchEvent(PassRefPtrWillBeRawPtr<Event> event)
{
    m_scheduledEvents.append(event);

    m_dispatchScheduledEventRunner.runAsync();
}

void RTCPeerConnection::dispatchScheduledEvent()
{
    if (m_stopped)
        return;

    WillBeHeapVector<RefPtrWillBeMember<Event> > events;
    events.swap(m_scheduledEvents);

    WillBeHeapVector<RefPtrWillBeMember<Event> >::iterator it = events.begin();
    for (; it != events.end(); ++it)
        dispatchEvent((*it).release());

    events.clear();
}

void RTCPeerConnection::trace(Visitor* visitor)
{
    visitor->trace(m_localStreams);
    visitor->trace(m_remoteStreams);
    visitor->trace(m_dataChannels);
    visitor->trace(m_scheduledEvents);
    EventTargetWithInlineData::trace(visitor);
}

} // namespace WebCore
