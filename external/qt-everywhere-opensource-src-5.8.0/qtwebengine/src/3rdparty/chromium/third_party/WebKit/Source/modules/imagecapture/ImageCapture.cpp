// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/imagecapture/ImageCapture.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/fileapi/Blob.h"
#include "core/frame/ImageBitmap.h"
#include "modules/EventTargetModules.h"
#include "modules/imagecapture/MediaSettingsRange.h"
#include "modules/imagecapture/PhotoCapabilities.h"
#include "modules/imagecapture/PhotoSettings.h"
#include "modules/mediastream/MediaStreamTrack.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/Platform.h"
#include "public/platform/ServiceRegistry.h"
#include "public/platform/WebImageCaptureFrameGrabber.h"
#include "public/platform/WebMediaStreamTrack.h"
#include "wtf/PtrUtil.h"

namespace blink {

namespace {

const char kNoServiceError[] = "ImageCapture service unavailable.";

bool trackIsInactive(const MediaStreamTrack& track)
{
    // Spec instructs to return an exception if the Track's readyState() is not
    // "live". Also reject if the track is disabled or muted.
    return track.readyState() != "live" || !track.enabled() || track.muted();
}

} // anonymous namespace

ImageCapture* ImageCapture::create(ExecutionContext* context, MediaStreamTrack* track, ExceptionState& exceptionState)
{
    if (track->kind() != "video") {
        exceptionState.throwDOMException(NotSupportedError, "Cannot create an ImageCapturer from a non-video Track.");
        return nullptr;
    }

    return new ImageCapture(context, track);
}

ImageCapture::~ImageCapture()
{
    DCHECK(!hasEventListeners());
    // There should be no more outstanding |m_serviceRequests| at this point
    // since each of them holds a persistent handle to this object.
    DCHECK(m_serviceRequests.isEmpty());
}

const AtomicString& ImageCapture::interfaceName() const
{
    return EventTargetNames::ImageCapture;
}

ExecutionContext* ImageCapture::getExecutionContext() const
{
    return ContextLifecycleObserver::getExecutionContext();
}

bool ImageCapture::hasPendingActivity() const
{
    return hasEventListeners();
}

void ImageCapture::contextDestroyed()
{
    removeAllEventListeners();
    m_serviceRequests.clear();
    DCHECK(!hasEventListeners());
}

ScriptPromise ImageCapture::getPhotoCapabilities(ScriptState* scriptState, ExceptionState& exceptionState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    if (!m_service) {
        resolver->reject(DOMException::create(NotFoundError, kNoServiceError));
        return promise;
    }

    m_serviceRequests.add(resolver);

    // m_streamTrack->component()->source()->id() is the renderer "name" of the camera;
    // TODO(mcasas) consider sending the security origin as well:
    // scriptState->getExecutionContext()->getSecurityOrigin()->toString()
    m_service->GetCapabilities(m_streamTrack->component()->source()->id(), createBaseCallback(WTF::bind(&ImageCapture::onCapabilities, wrapPersistent(this), wrapPersistent(resolver))));
    return promise;
}

ScriptPromise ImageCapture::setOptions(ScriptState* scriptState, const PhotoSettings& photoSettings, ExceptionState& exceptionState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    if (trackIsInactive(*m_streamTrack)) {
        resolver->reject(DOMException::create(InvalidStateError, "The associated Track is in an invalid state."));
        return promise;
    }

    if (!m_service) {
        resolver->reject(DOMException::create(NotFoundError, kNoServiceError));
        return promise;
    }

    m_serviceRequests.add(resolver);

    // TODO(mcasas): should be using a mojo::StructTraits instead.
    media::mojom::blink::PhotoSettingsPtr settings = media::mojom::blink::PhotoSettings::New();
    settings->has_zoom = photoSettings.hasZoom();
    if (settings->has_zoom)
        settings->zoom = photoSettings.zoom();

    m_service->SetOptions(m_streamTrack->component()->source()->id(), std::move(settings), createBaseCallback(WTF::bind(&ImageCapture::onSetOptions, wrapPersistent(this), wrapPersistent(resolver))));
    return promise;
}


ScriptPromise ImageCapture::takePhoto(ScriptState* scriptState, ExceptionState& exceptionState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    if (trackIsInactive(*m_streamTrack)) {
        resolver->reject(DOMException::create(InvalidStateError, "The associated Track is in an invalid state."));
        return promise;
    }

    if (!m_service) {
        resolver->reject(DOMException::create(NotFoundError, kNoServiceError));
        return promise;
    }

    m_serviceRequests.add(resolver);

    // m_streamTrack->component()->source()->id() is the renderer "name" of the camera;
    // TODO(mcasas) consider sending the security origin as well:
    // scriptState->getExecutionContext()->getSecurityOrigin()->toString()
    m_service->TakePhoto(m_streamTrack->component()->source()->id(), createBaseCallback(WTF::bind(&ImageCapture::onTakePhoto, wrapPersistent(this), wrapPersistent(resolver))));
    return promise;
}

ScriptPromise ImageCapture::grabFrame(ScriptState* scriptState, ExceptionState& exceptionState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    if (trackIsInactive(*m_streamTrack)) {
        resolver->reject(DOMException::create(InvalidStateError, "The associated Track is in an invalid state."));
        return promise;
    }

    // Create |m_frameGrabber| the first time.
    if (!m_frameGrabber)
        m_frameGrabber = wrapUnique(Platform::current()->createImageCaptureFrameGrabber());

    if (!m_frameGrabber) {
        resolver->reject(DOMException::create(UnknownError, "Couldn't create platform resources"));
        return promise;
    }

    // The platform does not know about MediaStreamTrack, so we wrap it up.
    WebMediaStreamTrack track(m_streamTrack->component());
    m_frameGrabber->grabFrame(&track, new CallbackPromiseAdapter<ImageBitmap, void>(resolver));

    return promise;
}

ImageCapture::ImageCapture(ExecutionContext* context, MediaStreamTrack* track)
    : ActiveScriptWrappable(this)
    , ContextLifecycleObserver(context)
    , m_streamTrack(track)
{
    DCHECK(m_streamTrack);
    DCHECK(!m_service.is_bound());

    Platform::current()->serviceRegistry()->connectToRemoteService(mojo::GetProxy(&m_service));

    m_service.set_connection_error_handler(createBaseCallback(WTF::bind(&ImageCapture::onServiceConnectionError, wrapWeakPersistent(this))));

}

void ImageCapture::onCapabilities(ScriptPromiseResolver* resolver, media::mojom::blink::PhotoCapabilitiesPtr capabilities)
{
    DVLOG(1) << __FUNCTION__;
    if (!m_serviceRequests.contains(resolver))
        return;
    if (capabilities.is_null()) {
        resolver->reject(DOMException::create(UnknownError, "platform error"));
    } else {
        // TODO(mcasas): Should be using a mojo::StructTraits.
        MediaSettingsRange* zoom = MediaSettingsRange::create(capabilities->zoom->max, capabilities->zoom->min, capabilities->zoom->current);
        PhotoCapabilities* caps = PhotoCapabilities::create();
        caps->setZoom(zoom);
        resolver->resolve(caps);
    }
    m_serviceRequests.remove(resolver);
}

void ImageCapture::onSetOptions(ScriptPromiseResolver* resolver, bool result)
{
    if (!m_serviceRequests.contains(resolver))
        return;

    if (result)
        resolver->resolve();
    else
        resolver->reject(DOMException::create(UnknownError, "setOptions failed"));
    m_serviceRequests.remove(resolver);
}

void ImageCapture::onTakePhoto(ScriptPromiseResolver* resolver, const String& mimeType, mojo::WTFArray<uint8_t> data)
{
    if (!m_serviceRequests.contains(resolver))
        return;

    if (data.is_null() || data.empty()) {
        resolver->reject(DOMException::create(UnknownError, "platform error"));
    } else {
        const auto& storage = data.storage();
        resolver->resolve(Blob::create(storage.data(), storage.size(), mimeType));
    }
    m_serviceRequests.remove(resolver);
}

void ImageCapture::onServiceConnectionError()
{
    m_service.reset();
    for (ScriptPromiseResolver* resolver : m_serviceRequests)
        resolver->reject(DOMException::create(NotFoundError, kNoServiceError));
    m_serviceRequests.clear();
}

DEFINE_TRACE(ImageCapture)
{
    visitor->trace(m_streamTrack);
    visitor->trace(m_serviceRequests);
    EventTargetWithInlineData::trace(visitor);
    ContextLifecycleObserver::trace(visitor);
}

} // namespace blink
