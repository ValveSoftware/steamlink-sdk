/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwinrtcameracontrol.h"
#include "qwinrtcameravideorenderercontrol.h"
#include "qwinrtvideodeviceselectorcontrol.h"
#include "qwinrtcameraimagecapturecontrol.h"
#include "qwinrtimageencodercontrol.h"
#include "qwinrtcameraflashcontrol.h"
#include "qwinrtcamerafocuscontrol.h"
#include "qwinrtcameralockscontrol.h"

#include <QtCore/qfunctions_winrt.h>
#include <QtCore/QPointer>
#include <QtGui/QGuiApplication>
#include <private/qeventdispatcher_winrt_p.h>

#include <functional>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <wrl.h>
#include <windows.devices.enumeration.h>
#include <windows.media.capture.h>
#include <windows.storage.streams.h>
#include <windows.media.devices.h>

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Devices::Enumeration;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::Media;
using namespace ABI::Windows::Media::Capture;
using namespace ABI::Windows::Media::Devices;
using namespace ABI::Windows::Media::MediaProperties;
using namespace ABI::Windows::Storage::Streams;

QT_BEGIN_NAMESPACE

#define RETURN_VOID_AND_EMIT_ERROR(msg) \
    if (FAILED(hr)) { \
        emit error(QCamera::CameraError, qt_error_string(hr)); \
        RETURN_VOID_IF_FAILED(msg); \
    }

#define FOCUS_RECT_SIZE         0.01f
#define FOCUS_RECT_HALF_SIZE    0.005f // FOCUS_RECT_SIZE / 2
#define FOCUS_RECT_BOUNDARY     1.0f
#define FOCUS_RECT_POSITION_MIN 0.0f
#define FOCUS_RECT_POSITION_MAX 0.995f // FOCUS_RECT_BOUNDARY - FOCUS_RECT_HALF_SIZE
#define ASPECTRATIO_EPSILON 0.01f

Q_LOGGING_CATEGORY(lcMMCamera, "qt.mm.camera")

HRESULT getMediaStreamResolutions(IMediaDeviceController *device,
                                  MediaStreamType type,
                                  IVectorView<IMediaEncodingProperties *> **propertiesList,
                                  QVector<QSize> *resolutions)
{
    HRESULT hr;
    hr = device->GetAvailableMediaStreamProperties(type, propertiesList);
    Q_ASSERT_SUCCEEDED(hr);
    quint32 listSize;
    hr = (*propertiesList)->get_Size(&listSize);
    Q_ASSERT_SUCCEEDED(hr);
    resolutions->reserve(listSize);
    for (quint32 index = 0; index < listSize; ++index) {
        ComPtr<IMediaEncodingProperties> properties;
        hr = (*propertiesList)->GetAt(index, &properties);
        Q_ASSERT_SUCCEEDED(hr);
        HString propertyType;
        hr = properties->get_Type(propertyType.GetAddressOf());
        Q_ASSERT_SUCCEEDED(hr);

        const HStringReference videoRef = HString::MakeReference(L"Video");
        const HStringReference imageRef = HString::MakeReference(L"Image");
        if (propertyType == videoRef) {
            ComPtr<IVideoEncodingProperties> videoProperties;
            hr = properties.As(&videoProperties);
            Q_ASSERT_SUCCEEDED(hr);
            UINT32 width, height;
            hr = videoProperties->get_Width(&width);
            Q_ASSERT_SUCCEEDED(hr);
            hr = videoProperties->get_Height(&height);
            Q_ASSERT_SUCCEEDED(hr);
            resolutions->append(QSize(width, height));
        } else if (propertyType == imageRef) {
            ComPtr<IImageEncodingProperties> imageProperties;
            hr = properties.As(&imageProperties);
            Q_ASSERT_SUCCEEDED(hr);
            UINT32 width, height;
            hr = imageProperties->get_Width(&width);
            Q_ASSERT_SUCCEEDED(hr);
            hr = imageProperties->get_Height(&height);
            Q_ASSERT_SUCCEEDED(hr);
            resolutions->append(QSize(width, height));
        }
    }
    return resolutions->isEmpty() ? MF_E_INVALID_FORMAT : hr;
}

template<typename T, size_t typeSize> struct CustomPropertyValue;

inline static ComPtr<IPropertyValueStatics> propertyValueStatics()
{
    ComPtr<IPropertyValueStatics> valueStatics;
    GetActivationFactory(HString::MakeReference(RuntimeClass_Windows_Foundation_PropertyValue).Get(), &valueStatics);
    return valueStatics;
}

template <typename T>
struct CustomPropertyValue<T, 4>
{
    static ComPtr<IReference<T>> create(T value)
    {
        ComPtr<IInspectable> propertyValueObject;
        HRESULT hr = propertyValueStatics()->CreateUInt32(value, &propertyValueObject);
        ComPtr<IReference<UINT32>> uint32Object;
        Q_ASSERT_SUCCEEDED(hr);
        hr = propertyValueObject.As(&uint32Object);
        Q_ASSERT_SUCCEEDED(hr);
        return reinterpret_cast<IReference<T> *>(uint32Object.Get());
    }
};

template <typename T>
struct CustomPropertyValue<T, 8>
{
    static ComPtr<IReference<T>> create(T value)
    {
        ComPtr<IInspectable> propertyValueObject;
        HRESULT hr = propertyValueStatics()->CreateUInt64(value, &propertyValueObject);
        ComPtr<IReference<UINT64>> uint64Object;
        Q_ASSERT_SUCCEEDED(hr);
        hr = propertyValueObject.As(&uint64Object);
        Q_ASSERT_SUCCEEDED(hr);
        return reinterpret_cast<IReference<T> *>(uint64Object.Get());
    }
};

// Required camera point focus
class WindowsRegionOfInterestIterableIterator : public RuntimeClass<IIterator<RegionOfInterest *>>
{
public:
    explicit WindowsRegionOfInterestIterableIterator(const ComPtr<IRegionOfInterest> &item)
    {
        regionOfInterest = item;
    }
    HRESULT __stdcall get_Current(IRegionOfInterest **current)
    {
        *current = regionOfInterest.Detach();
        return S_OK;
    }
    HRESULT __stdcall get_HasCurrent(boolean *hasCurrent)
    {
        *hasCurrent = true;
        return S_OK;
    }
    HRESULT __stdcall MoveNext(boolean *hasCurrent)
    {
        *hasCurrent = false;
        return S_OK;
    }
private:
    ComPtr<IRegionOfInterest> regionOfInterest;
};

class WindowsRegionOfInterestIterable : public RuntimeClass<IIterable<RegionOfInterest *>>
{
public:
    explicit WindowsRegionOfInterestIterable(const ComPtr<IRegionOfInterest> &item)
    {
        regionOfInterest = item;
    }
    HRESULT __stdcall First(IIterator<RegionOfInterest *> **first)
    {
        ComPtr<WindowsRegionOfInterestIterableIterator> iterator = Make<WindowsRegionOfInterestIterableIterator>(regionOfInterest);
        *first = iterator.Detach();
        return S_OK;
    }
private:
    ComPtr<IRegionOfInterest> regionOfInterest;
};

class MediaStream : public RuntimeClass<RuntimeClassFlags<WinRtClassicComMix>, IMFStreamSink, IMFMediaEventGenerator, IMFMediaTypeHandler>
{
public:
    MediaStream(IMFMediaType *type, IMFMediaSink *mediaSink, QWinRTCameraVideoRendererControl *videoRenderer)
        : m_type(type), m_sink(mediaSink), m_videoRenderer(videoRenderer)
    {
        Q_ASSERT(m_videoRenderer);

        InitializeCriticalSectionEx(&m_mutex, 0, 0);

        HRESULT hr;
        hr = MFCreateEventQueue(&m_eventQueue);
        Q_ASSERT_SUCCEEDED(hr);
        hr = MFAllocateSerialWorkQueue(MFASYNC_CALLBACK_QUEUE_STANDARD, &m_workQueueId);
        Q_ASSERT_SUCCEEDED(hr);
    }

    ~MediaStream()
    {
        CriticalSectionLocker locker(&m_mutex);
        m_eventQueue->Shutdown();
        DeleteCriticalSection(&m_mutex);
    }

    HRESULT RequestSample()
    {
        if (m_pendingSamples.load() < 3) {
            m_pendingSamples.ref();
            return QueueEvent(MEStreamSinkRequestSample, GUID_NULL, S_OK, Q_NULLPTR);
        }
        return S_OK;
    }

    HRESULT __stdcall GetEvent(DWORD flags, IMFMediaEvent **event) Q_DECL_OVERRIDE
    {
        EnterCriticalSection(&m_mutex);
        // Create an extra reference to avoid deadlock
        ComPtr<IMFMediaEventQueue> eventQueue = m_eventQueue;
        LeaveCriticalSection(&m_mutex);

        return eventQueue->GetEvent(flags, event);
    }

    HRESULT __stdcall BeginGetEvent(IMFAsyncCallback *callback, IUnknown *state) Q_DECL_OVERRIDE
    {
        CriticalSectionLocker locker(&m_mutex);
        HRESULT hr = m_eventQueue->BeginGetEvent(callback, state);
        return hr;
    }

    HRESULT __stdcall EndGetEvent(IMFAsyncResult *result, IMFMediaEvent **event) Q_DECL_OVERRIDE
    {
        CriticalSectionLocker locker(&m_mutex);
        return m_eventQueue->EndGetEvent(result, event);
    }

    HRESULT __stdcall QueueEvent(MediaEventType eventType, const GUID &extendedType, HRESULT status, const PROPVARIANT *value) Q_DECL_OVERRIDE
    {
        CriticalSectionLocker locker(&m_mutex);
        return m_eventQueue->QueueEventParamVar(eventType, extendedType, status, value);
    }

    HRESULT __stdcall GetMediaSink(IMFMediaSink **mediaSink) Q_DECL_OVERRIDE
    {
        *mediaSink = m_sink;
        return S_OK;
    }

    HRESULT __stdcall GetIdentifier(DWORD *identifier) Q_DECL_OVERRIDE
    {
        *identifier = 0;
        return S_OK;
    }

    HRESULT __stdcall GetMediaTypeHandler(IMFMediaTypeHandler **handler) Q_DECL_OVERRIDE
    {
        return QueryInterface(IID_PPV_ARGS(handler));
    }

    HRESULT __stdcall ProcessSample(IMFSample *sample) Q_DECL_OVERRIDE
    {
        ComPtr<IMFMediaBuffer> buffer;
        HRESULT hr = sample->GetBufferByIndex(0, &buffer);
        RETURN_HR_IF_FAILED("Failed to get buffer from camera sample");
        ComPtr<IMF2DBuffer> buffer2d;
        hr = buffer.As(&buffer2d);
        RETURN_HR_IF_FAILED("Failed to cast camera sample buffer to 2D buffer");

        m_pendingSamples.deref();
        m_videoRenderer->queueBuffer(buffer2d.Get());

        return hr;
    }

    HRESULT __stdcall PlaceMarker(MFSTREAMSINK_MARKER_TYPE type, const PROPVARIANT *value, const PROPVARIANT *context) Q_DECL_OVERRIDE
    {
        Q_UNUSED(type);
        Q_UNUSED(value);
        QueueEvent(MEStreamSinkMarker, GUID_NULL, S_OK, context);
        return S_OK;
    }

    HRESULT __stdcall Flush() Q_DECL_OVERRIDE
    {
        m_videoRenderer->discardBuffers();
        m_pendingSamples.store(0);
        return S_OK;
    }

    HRESULT __stdcall IsMediaTypeSupported(IMFMediaType *type, IMFMediaType **) Q_DECL_OVERRIDE
    {
        HRESULT hr;
        GUID majorType;
        hr = type->GetMajorType(&majorType);
        Q_ASSERT_SUCCEEDED(hr);
        if (!IsEqualGUID(majorType, MFMediaType_Video))
            return MF_E_INVALIDMEDIATYPE;
        return S_OK;
    }

    HRESULT __stdcall GetMediaTypeCount(DWORD *typeCount) Q_DECL_OVERRIDE
    {
        *typeCount = 1;
        return S_OK;
    }

    HRESULT __stdcall GetMediaTypeByIndex(DWORD index, IMFMediaType **type) Q_DECL_OVERRIDE
    {
        if (index == 0)
            return m_type.CopyTo(type);
        return E_BOUNDS;
    }

    HRESULT __stdcall SetCurrentMediaType(IMFMediaType *type) Q_DECL_OVERRIDE
    {
        if (FAILED(IsMediaTypeSupported(type, Q_NULLPTR)))
            return MF_E_INVALIDREQUEST;

        m_type = type;
        return S_OK;
    }

    HRESULT __stdcall GetCurrentMediaType(IMFMediaType **type) Q_DECL_OVERRIDE
    {
        return m_type.CopyTo(type);
    }

    HRESULT __stdcall GetMajorType(GUID *majorType) Q_DECL_OVERRIDE
    {
        return m_type->GetMajorType(majorType);
    }

private:
    CRITICAL_SECTION m_mutex;
    ComPtr<IMFMediaType> m_type;
    IMFMediaSink *m_sink;
    ComPtr<IMFMediaEventQueue> m_eventQueue;
    DWORD m_workQueueId;

    QWinRTCameraVideoRendererControl *m_videoRenderer;
    QAtomicInt m_pendingSamples;
};

class MediaSink : public RuntimeClass<RuntimeClassFlags<WinRtClassicComMix>, IMediaExtension, IMFMediaSink, IMFClockStateSink>
{
public:
    MediaSink(IMediaEncodingProfile *encodingProfile, QWinRTCameraVideoRendererControl *videoRenderer)
        : m_videoRenderer(videoRenderer)
    {
        HRESULT hr;
        ComPtr<IVideoEncodingProperties> videoProperties;
        hr = encodingProfile->get_Video(&videoProperties);
        RETURN_VOID_IF_FAILED("Failed to get video properties");
        ComPtr<IMFMediaType> videoType;
        hr = MFCreateMediaTypeFromProperties(videoProperties.Get(), &videoType);
        RETURN_VOID_IF_FAILED("Failed to create video type");
        m_stream = Make<MediaStream>(videoType.Get(), this, videoRenderer);
    }

    ~MediaSink()
    {
    }

    HRESULT RequestSample()
    {
        return m_stream->RequestSample();
    }

    HRESULT __stdcall SetProperties(Collections::IPropertySet *configuration) Q_DECL_OVERRIDE
    {
        Q_UNUSED(configuration);
        return E_NOTIMPL;
    }

    HRESULT __stdcall GetCharacteristics(DWORD *characteristics) Q_DECL_OVERRIDE
    {
        *characteristics = MEDIASINK_FIXED_STREAMS | MEDIASINK_RATELESS;
        return S_OK;
    }

    HRESULT __stdcall AddStreamSink(DWORD streamSinkIdentifier, IMFMediaType *mediaType, IMFStreamSink **streamSink) Q_DECL_OVERRIDE
    {
        Q_UNUSED(streamSinkIdentifier);
        Q_UNUSED(mediaType);
        Q_UNUSED(streamSink);
        return E_NOTIMPL;
    }

    HRESULT __stdcall RemoveStreamSink(DWORD streamSinkIdentifier) Q_DECL_OVERRIDE
    {
        Q_UNUSED(streamSinkIdentifier);
        return E_NOTIMPL;
    }

    HRESULT __stdcall GetStreamSinkCount(DWORD *streamSinkCount) Q_DECL_OVERRIDE
    {
        *streamSinkCount = 1;
        return S_OK;
    }

    HRESULT __stdcall GetStreamSinkByIndex(DWORD index, IMFStreamSink **streamSink) Q_DECL_OVERRIDE
    {
        if (index == 0)
            return m_stream.CopyTo(streamSink);
        return MF_E_INVALIDINDEX;
    }

    HRESULT __stdcall GetStreamSinkById(DWORD streamSinkIdentifier, IMFStreamSink **streamSink) Q_DECL_OVERRIDE
    {
        // ID and index are always 0
        HRESULT hr = GetStreamSinkByIndex(streamSinkIdentifier, streamSink);
        return hr == MF_E_INVALIDINDEX ? MF_E_INVALIDSTREAMNUMBER : hr;
    }

    HRESULT __stdcall SetPresentationClock(IMFPresentationClock *presentationClock) Q_DECL_OVERRIDE
    {
        HRESULT hr = S_OK;
        m_presentationClock = presentationClock;
        if (m_presentationClock)
            hr = m_presentationClock->AddClockStateSink(this);
        return hr;
    }

    HRESULT __stdcall GetPresentationClock(IMFPresentationClock **presentationClock) Q_DECL_OVERRIDE
    {
        return m_presentationClock.CopyTo(presentationClock);
    }

    HRESULT __stdcall Shutdown() Q_DECL_OVERRIDE
    {
        m_stream->Flush();
        scheduleSetActive(false);
        return m_presentationClock ? m_presentationClock->Stop() : S_OK;
    }

    HRESULT __stdcall OnClockStart(MFTIME systemTime, LONGLONG clockStartOffset) Q_DECL_OVERRIDE
    {
        Q_UNUSED(systemTime);
        Q_UNUSED(clockStartOffset);

        scheduleSetActive(true);

        return S_OK;
    }

    HRESULT __stdcall OnClockStop(MFTIME systemTime) Q_DECL_OVERRIDE
    {
        Q_UNUSED(systemTime);

        scheduleSetActive(false);

        return m_stream->QueueEvent(MEStreamSinkStopped, GUID_NULL, S_OK, Q_NULLPTR);
    }

    HRESULT __stdcall OnClockPause(MFTIME systemTime) Q_DECL_OVERRIDE
    {
        Q_UNUSED(systemTime);

        scheduleSetActive(false);

        return m_stream->QueueEvent(MEStreamSinkPaused, GUID_NULL, S_OK, Q_NULLPTR);
    }

    HRESULT __stdcall OnClockRestart(MFTIME systemTime) Q_DECL_OVERRIDE
    {
        Q_UNUSED(systemTime);

        scheduleSetActive(true);

        return m_stream->QueueEvent(MEStreamSinkStarted, GUID_NULL, S_OK, Q_NULLPTR);
    }

    HRESULT __stdcall OnClockSetRate(MFTIME systemTime, float rate) Q_DECL_OVERRIDE
    {
        Q_UNUSED(systemTime);
        Q_UNUSED(rate);
        return E_NOTIMPL;
    }

private:

    inline void scheduleSetActive(bool active)
    {
        QMetaObject::invokeMethod(m_videoRenderer, "setActive", Qt::QueuedConnection, Q_ARG(bool, active));
    }

    ComPtr<MediaStream> m_stream;
    ComPtr<IMFPresentationClock> m_presentationClock;

    QWinRTCameraVideoRendererControl *m_videoRenderer;
};

class QWinRTCameraControlPrivate
{
public:
    QCamera::State state;
    QCamera::Status status;
    QCamera::CaptureModes captureMode;

    ComPtr<IMediaCapture> capture;
    ComPtr<IMediaCaptureVideoPreview> capturePreview;
    EventRegistrationToken captureFailedCookie;
    EventRegistrationToken recordLimitationCookie;

    ComPtr<IMediaEncodingProfileStatics> encodingProfileFactory;

    ComPtr<IMediaEncodingProfile> encodingProfile;
    ComPtr<MediaSink> mediaSink;
    ComPtr<IFocusControl> focusControl;
    ComPtr<IRegionsOfInterestControl> regionsOfInterestControl;
    ComPtr<IAsyncAction> focusOperation;

    QPointer<QWinRTCameraVideoRendererControl> videoRenderer;
    QPointer<QWinRTVideoDeviceSelectorControl> videoDeviceSelector;
    QPointer<QWinRTCameraImageCaptureControl> imageCaptureControl;
    QPointer<QWinRTImageEncoderControl> imageEncoderControl;
    QPointer<QWinRTCameraFlashControl> cameraFlashControl;
    QPointer<QWinRTCameraFocusControl> cameraFocusControl;
    QPointer<QWinRTCameraLocksControl> cameraLocksControl;
    QAtomicInt framesMapped;
    QEventLoop *delayClose;
};

QWinRTCameraControl::QWinRTCameraControl(QObject *parent)
    : QCameraControl(parent), d_ptr(new QWinRTCameraControlPrivate)
{
    qCDebug(lcMMCamera) << __FUNCTION__ << parent;
    Q_D(QWinRTCameraControl);

    d->delayClose = nullptr;
    d->state = QCamera::UnloadedState;
    d->status = QCamera::UnloadedStatus;
    d->captureMode = QCamera::CaptureStillImage;
    d->captureFailedCookie.value = 0;
    d->recordLimitationCookie.value = 0;
    d->videoRenderer = new QWinRTCameraVideoRendererControl(QSize(), this);
    connect(d->videoRenderer, &QWinRTCameraVideoRendererControl::bufferRequested,
            this, &QWinRTCameraControl::onBufferRequested);
    d->videoDeviceSelector = new QWinRTVideoDeviceSelectorControl(this);
    d->imageCaptureControl = new QWinRTCameraImageCaptureControl(this);
    d->imageEncoderControl = new QWinRTImageEncoderControl(this);
    d->cameraFlashControl = new QWinRTCameraFlashControl(this);
    d->cameraFocusControl = new QWinRTCameraFocusControl(this);
    d->cameraLocksControl = new QWinRTCameraLocksControl(this);

    if (qGuiApp) {
        connect(qGuiApp, &QGuiApplication::applicationStateChanged,
                this, &QWinRTCameraControl::onApplicationStateChanged);
    }
}

QWinRTCameraControl::~QWinRTCameraControl()
{
    setState(QCamera::UnloadedState);
}

QCamera::State QWinRTCameraControl::state() const
{
    Q_D(const QWinRTCameraControl);
    return d->state;
}

void QWinRTCameraControl::setState(QCamera::State state)
{
    qCDebug(lcMMCamera) << __FUNCTION__ << state;
    Q_D(QWinRTCameraControl);

    if (d->state == state)
        return;

    HRESULT hr;
    switch (state) {
    case QCamera::ActiveState: {
        // Capture has not been created or initialized
        if (d->state == QCamera::UnloadedState) {
            hr = initialize();
            RETURN_VOID_AND_EMIT_ERROR("Failed to initialize media capture");
        }
        Q_ASSERT(d->state == QCamera::LoadedState);

        ComPtr<IAsyncAction> op;
        hr = QEventDispatcherWinRT::runOnXamlThread([d, &op]() {
            d->mediaSink = Make<MediaSink>(d->encodingProfile.Get(), d->videoRenderer);
            HRESULT hr = d->capturePreview->StartPreviewToCustomSinkAsync(d->encodingProfile.Get(), d->mediaSink.Get(), &op);
            return hr;
        });
        RETURN_VOID_AND_EMIT_ERROR("Failed to initiate capture.");
        if (d->status != QCamera::StartingStatus) {
            d->status = QCamera::StartingStatus;
            emit statusChanged(d->status);
        }

        hr = QEventDispatcherWinRT::runOnXamlThread([&op]() {
            return QWinRTFunctions::await(op);
        });
        if (FAILED(hr)) {
            emit error(QCamera::CameraError, qt_error_string(hr));
            setState(QCamera::UnloadedState); // Unload everything, as initialize() will need be called again
            return;
        }

        QCameraFocus::FocusModes focusMode = d->cameraFocusControl->focusMode();
        if (focusMode != 0 && setFocus(focusMode) && focusMode == QCameraFocus::ContinuousFocus)
            focus();

        d->state = QCamera::ActiveState;
        emit stateChanged(d->state);
        d->status = QCamera::ActiveStatus;
        emit statusChanged(d->status);
        QEventDispatcherWinRT::runOnXamlThread([d]() { d->mediaSink->RequestSample(); return S_OK;});
        break;
    }
    case QCamera::LoadedState: {
        // If moving from unloaded, initialize the camera
        if (d->state == QCamera::UnloadedState) {
            hr = initialize();
            RETURN_VOID_AND_EMIT_ERROR("Failed to initialize media capture");
        }
        // fall through
    }
    case QCamera::UnloadedState: {
        // Stop the camera if it is running (transition to LoadedState)
        if (d->status == QCamera::ActiveStatus) {
            HRESULT hr;
            if (d->focusOperation) {
                hr = QWinRTFunctions::await(d->focusOperation);
                Q_ASSERT_SUCCEEDED(hr);
            }
            if (d->framesMapped > 0) {
                qWarning("%d QVideoFrame(s) mapped when closing down camera. Camera will wait for unmap before closing down.",
                         d->framesMapped);
                if (!d->delayClose)
                    d->delayClose = new QEventLoop(this);
                d->delayClose->exec();
            }

            ComPtr<IAsyncAction> op;
            hr = QEventDispatcherWinRT::runOnXamlThread([d, &op]() {
                HRESULT hr = d->capturePreview->StopPreviewAsync(&op);
                return hr;
            });
            RETURN_VOID_AND_EMIT_ERROR("Failed to stop camera preview");
            if (d->status != QCamera::StoppingStatus) {
                d->status = QCamera::StoppingStatus;
                emit statusChanged(d->status);
            }
            Q_ASSERT_SUCCEEDED(hr);
            hr = QEventDispatcherWinRT::runOnXamlThread([&op]() {
                return QWinRTFunctions::await(op); // Synchronize unloading
            });
            if (FAILED(hr))
                emit error(QCamera::InvalidRequestError, qt_error_string(hr));

            if (d->mediaSink) {
                hr = QEventDispatcherWinRT::runOnXamlThread([d]() {
                d->mediaSink->Shutdown();
                d->mediaSink.Reset();
                return S_OK;
                });
            }

            d->state = QCamera::LoadedState;
            emit stateChanged(d->state);

            d->status = QCamera::LoadedStatus;
            emit statusChanged(d->status);
        }
        // Completely unload if needed
        if (state == QCamera::UnloadedState) {
            if (!d->capture) // Already unloaded
                break;

            if (d->status != QCamera::UnloadingStatus) {
                d->status = QCamera::UnloadingStatus;
                emit statusChanged(d->status);
            }

            hr = QEventDispatcherWinRT::runOnXamlThread([d]() {
                HRESULT hr;
                if (d->capture && d->captureFailedCookie.value) {
                    hr = d->capture->remove_Failed(d->captureFailedCookie);
                    Q_ASSERT_SUCCEEDED(hr);
                    d->captureFailedCookie.value = 0;
                }
                if (d->capture && d->recordLimitationCookie.value) {
                    d->capture->remove_RecordLimitationExceeded(d->recordLimitationCookie);
                    Q_ASSERT_SUCCEEDED(hr);
                    d->recordLimitationCookie.value = 0;
                }
                ComPtr<IClosable> capture;
                hr = d->capture.As(&capture);
                Q_ASSERT_SUCCEEDED(hr);
                hr = capture->Close();
                RETURN_HR_IF_FAILED("");
                d->capture.Reset();
                return hr;
            });
            RETURN_VOID_AND_EMIT_ERROR("Failed to close the capture manger");
            if (d->state != QCamera::UnloadedState) {
                d->state = QCamera::UnloadedState;
                emit stateChanged(d->state);
            }
            if (d->status != QCamera::UnloadedStatus) {
                d->status = QCamera::UnloadedStatus;
                emit statusChanged(d->status);
            }
        }
        break;
    }
    default:
        break;
    }
}

QCamera::Status QWinRTCameraControl::status() const
{
    Q_D(const QWinRTCameraControl);
    return d->status;
}

QCamera::CaptureModes QWinRTCameraControl::captureMode() const
{
    Q_D(const QWinRTCameraControl);
    return d->captureMode;
}

void QWinRTCameraControl::setCaptureMode(QCamera::CaptureModes mode)
{
    qCDebug(lcMMCamera) << __FUNCTION__ << mode;
    Q_D(QWinRTCameraControl);

    if (d->captureMode == mode)
        return;

    if (!isCaptureModeSupported(mode)) {
        qWarning("Unsupported capture mode: %d", mode);
        return;
    }

    d->captureMode = mode;
    emit captureModeChanged(d->captureMode);
}

bool QWinRTCameraControl::isCaptureModeSupported(QCamera::CaptureModes mode) const
{
    return mode >= QCamera::CaptureViewfinder && mode <= QCamera::CaptureStillImage;
}

bool QWinRTCameraControl::canChangeProperty(QCameraControl::PropertyChangeType changeType, QCamera::Status status) const
{
    Q_UNUSED(changeType);

    return status == QCamera::UnloadedStatus; // For now, assume shutdown is required for all property changes
}

QVideoRendererControl *QWinRTCameraControl::videoRenderer() const
{
    Q_D(const QWinRTCameraControl);
    return d->videoRenderer;
}

QVideoDeviceSelectorControl *QWinRTCameraControl::videoDeviceSelector() const
{
    Q_D(const QWinRTCameraControl);
    return d->videoDeviceSelector;
}

QCameraImageCaptureControl *QWinRTCameraControl::imageCaptureControl() const
{
    Q_D(const QWinRTCameraControl);
    return d->imageCaptureControl;
}

QImageEncoderControl *QWinRTCameraControl::imageEncoderControl() const
{
    Q_D(const QWinRTCameraControl);
    return d->imageEncoderControl;
}

QCameraFlashControl *QWinRTCameraControl::cameraFlashControl() const
{
    Q_D(const QWinRTCameraControl);
    return d->cameraFlashControl;
}

QCameraFocusControl *QWinRTCameraControl::cameraFocusControl() const
{
    Q_D(const QWinRTCameraControl);
    return d->cameraFocusControl;
}

QCameraLocksControl *QWinRTCameraControl::cameraLocksControl() const
{
    Q_D(const QWinRTCameraControl);
    return d->cameraLocksControl;
}

Microsoft::WRL::ComPtr<ABI::Windows::Media::Capture::IMediaCapture> QWinRTCameraControl::handle() const
{
    Q_D(const QWinRTCameraControl);
    return d->capture;
}

void QWinRTCameraControl::onBufferRequested()
{
    Q_D(QWinRTCameraControl);

    if (d->mediaSink)
        d->mediaSink->RequestSample();
}

void QWinRTCameraControl::onApplicationStateChanged(Qt::ApplicationState state)
{
    qCDebug(lcMMCamera) << __FUNCTION__ << state;
#ifdef _DEBUG
    return;
#else // !_DEBUG
    Q_D(QWinRTCameraControl);
    static QCamera::State savedState = d->state;
    switch (state) {
    case Qt::ApplicationInactive:
        if (d->state != QCamera::UnloadedState) {
            savedState = d->state;
            setState(QCamera::UnloadedState);
        }
        break;
    case Qt::ApplicationActive:
        setState(QCamera::State(savedState));
        break;
    default:
        break;
    }
#endif // _DEBUG
}

HRESULT QWinRTCameraControl::initialize()
{
    qCDebug(lcMMCamera) << __FUNCTION__;
    Q_D(QWinRTCameraControl);

    if (d->status != QCamera::LoadingStatus) {
        d->status = QCamera::LoadingStatus;
        emit statusChanged(d->status);
    }

    boolean isFocusSupported;
    HRESULT hr = QEventDispatcherWinRT::runOnXamlThread([this, d, &isFocusSupported]() {
        HRESULT hr;
        ComPtr<IInspectable> capture;
        hr = RoActivateInstance(Wrappers::HString::MakeReference(RuntimeClass_Windows_Media_Capture_MediaCapture).Get(),
                                &capture);
        Q_ASSERT_SUCCEEDED(hr);
        hr = capture.As(&d->capture);
        Q_ASSERT_SUCCEEDED(hr);
        hr = d->capture.As(&d->capturePreview);
        Q_ASSERT_SUCCEEDED(hr);
        hr = d->capture->add_Failed(Callback<IMediaCaptureFailedEventHandler>(this, &QWinRTCameraControl::onCaptureFailed).Get(),
                                    &d->captureFailedCookie);
        Q_ASSERT_SUCCEEDED(hr);
        hr = d->capture->add_RecordLimitationExceeded(Callback<IRecordLimitationExceededEventHandler>(this, &QWinRTCameraControl::onRecordLimitationExceeded).Get(),
                                                      &d->recordLimitationCookie);
        Q_ASSERT_SUCCEEDED(hr);
        hr = RoGetActivationFactory(HString::MakeReference(RuntimeClass_Windows_Media_MediaProperties_MediaEncodingProfile).Get(),
                                    IID_PPV_ARGS(&d->encodingProfileFactory));
        Q_ASSERT_SUCCEEDED(hr);

        int deviceIndex = d->videoDeviceSelector->selectedDevice();
        if (deviceIndex < 0)
            deviceIndex = d->videoDeviceSelector->defaultDevice();

        const QString deviceName = d->videoDeviceSelector->deviceName(deviceIndex);
        if (deviceName.isEmpty()) {
            qWarning("No video device available or selected.");
            return E_FAIL;
        }

        const QCamera::Position position = d->videoDeviceSelector->cameraPosition(deviceName);
        d->videoRenderer->setScanLineDirection(position == QCamera::BackFace ? QVideoSurfaceFormat::TopToBottom
                                                                             : QVideoSurfaceFormat::BottomToTop);
        ComPtr<IMediaCaptureInitializationSettings> settings;
        hr = RoActivateInstance(HString::MakeReference(RuntimeClass_Windows_Media_Capture_MediaCaptureInitializationSettings).Get(),
                                &settings);
        Q_ASSERT_SUCCEEDED(hr);
        HStringReference deviceId(reinterpret_cast<LPCWSTR>(deviceName.utf16()), deviceName.length());
        hr = settings->put_VideoDeviceId(deviceId.Get());
        Q_ASSERT_SUCCEEDED(hr);

        hr = settings->put_StreamingCaptureMode(StreamingCaptureMode_Video);
        Q_ASSERT_SUCCEEDED(hr);

        hr = settings->put_PhotoCaptureSource(PhotoCaptureSource_Auto);
        Q_ASSERT_SUCCEEDED(hr);

        ComPtr<IAsyncAction> op;
        hr = d->capture->InitializeWithSettingsAsync(settings.Get(), &op);
        RETURN_HR_IF_FAILED("Failed to begin initialization of media capture manager");
        hr = QWinRTFunctions::await(op, QWinRTFunctions::ProcessThreadEvents);
        if (hr == E_ACCESSDENIED) {
            qWarning("Access denied when initializing the media capture manager. "
                     "Check your manifest settings for microphone and webcam access.");
        }
        RETURN_HR_IF_FAILED("Failed to initialize media capture manager");

        ComPtr<IVideoDeviceController> videoDeviceController;
        hr = d->capture->get_VideoDeviceController(&videoDeviceController);
        ComPtr<IAdvancedVideoCaptureDeviceController2> advancedVideoDeviceController;
        hr = videoDeviceController.As(&advancedVideoDeviceController);
        Q_ASSERT_SUCCEEDED(hr);
        hr = advancedVideoDeviceController->get_FocusControl(&d->focusControl);
        Q_ASSERT_SUCCEEDED(hr);

        d->cameraFlashControl->initialize(advancedVideoDeviceController);

        hr = d->focusControl->get_Supported(&isFocusSupported);
        Q_ASSERT_SUCCEEDED(hr);
        if (isFocusSupported) {
            hr = advancedVideoDeviceController->get_RegionsOfInterestControl(&d->regionsOfInterestControl);
            if (FAILED(hr))
                qCDebug(lcMMCamera) << "Focus supported, but no control for regions of interest available";
            hr = initializeFocus();
            Q_ASSERT_SUCCEEDED(hr);
        }

        Q_ASSERT_SUCCEEDED(hr);
        ComPtr<IMediaDeviceController> deviceController;
        hr = videoDeviceController.As(&deviceController);
        Q_ASSERT_SUCCEEDED(hr);

        // Get preview stream properties.
        ComPtr<IVectorView<IMediaEncodingProperties *>> previewPropertiesList;
        QVector<QSize> previewResolutions;
        hr = getMediaStreamResolutions(deviceController.Get(),
                                       MediaStreamType_VideoPreview,
                                       &previewPropertiesList,
                                       &previewResolutions);
        RETURN_HR_IF_FAILED("Failed to find a suitable video format");

        MediaStreamType mediaStreamType =
                d->captureMode == QCamera::CaptureVideo ? MediaStreamType_VideoRecord : MediaStreamType_Photo;

        // Get capture stream properties.
        ComPtr<IVectorView<IMediaEncodingProperties *>> capturePropertiesList;
        QVector<QSize> captureResolutions;
        hr = getMediaStreamResolutions(deviceController.Get(),
                                       mediaStreamType,
                                       &capturePropertiesList,
                                       &captureResolutions);
        RETURN_HR_IF_FAILED("Failed to find a suitable video format");

        // Set capture resolutions.
        d->imageEncoderControl->setSupportedResolutionsList(captureResolutions.toList());
        const QSize captureResolution = d->imageEncoderControl->imageSettings().resolution();
        const quint32 captureResolutionIndex = captureResolutions.indexOf(captureResolution);
        ComPtr<IMediaEncodingProperties> captureProperties;
        hr = capturePropertiesList->GetAt(captureResolutionIndex, &captureProperties);
        Q_ASSERT_SUCCEEDED(hr);
        hr = deviceController->SetMediaStreamPropertiesAsync(mediaStreamType, captureProperties.Get(), &op);
        Q_ASSERT_SUCCEEDED(hr);
        hr = QWinRTFunctions::await(op);
        Q_ASSERT_SUCCEEDED(hr);

        // Set preview resolution.
        QVector<QSize> filtered;
        const float captureAspectRatio = float(captureResolution.width()) / captureResolution.height();
        for (const QSize &resolution : qAsConst(previewResolutions)) {
            const float aspectRatio = float(resolution.width()) / resolution.height();
            if (qAbs(aspectRatio - captureAspectRatio) <= ASPECTRATIO_EPSILON)
                filtered.append(resolution);
        }
        qSort(filtered.begin(),
              filtered.end(),
              [](QSize size1, QSize size2) { return size1.width() * size1.height() < size2.width() * size2.height(); });

        const QSize &viewfinderResolution = filtered.first();
        const quint32 viewfinderResolutionIndex = previewResolutions.indexOf(viewfinderResolution);
        hr = RoActivateInstance(HString::MakeReference(RuntimeClass_Windows_Media_MediaProperties_MediaEncodingProfile).Get(),
                                &d->encodingProfile);
        Q_ASSERT_SUCCEEDED(hr);
        ComPtr<IMediaEncodingProperties> previewProperties;
        hr = previewPropertiesList->GetAt(viewfinderResolutionIndex, &previewProperties);
        Q_ASSERT_SUCCEEDED(hr);
        hr = deviceController->SetMediaStreamPropertiesAsync(MediaStreamType_VideoPreview, previewProperties.Get(), &op);
        Q_ASSERT_SUCCEEDED(hr);
        hr = QWinRTFunctions::await(op);
        Q_ASSERT_SUCCEEDED(hr);
        ComPtr<IVideoEncodingProperties> videoPreviewProperties;
        hr = previewProperties.As(&videoPreviewProperties);
        Q_ASSERT_SUCCEEDED(hr);
        hr = d->encodingProfile->put_Video(videoPreviewProperties.Get());
        Q_ASSERT_SUCCEEDED(hr);

        if (d->videoRenderer)
            d->videoRenderer->setSize(viewfinderResolution);

        return S_OK;
    });

    if (!isFocusSupported) {
        d->cameraFocusControl->setSupportedFocusMode(0);
        d->cameraFocusControl->setSupportedFocusPointMode(QSet<QCameraFocus::FocusPointMode>());
    }
    d->cameraLocksControl->initialize();

    if (SUCCEEDED(hr) && d->state != QCamera::LoadedState) {
        d->state = QCamera::LoadedState;
        emit stateChanged(d->state);
    }
    if (SUCCEEDED(hr) && d->status != QCamera::LoadedStatus) {
        d->status = QCamera::LoadedStatus;
        emit statusChanged(d->status);
    }
    return hr;
}

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_PHONE_APP)

HRESULT QWinRTCameraControl::initializeFocus()
{
    Q_D(QWinRTCameraControl);
    ComPtr<IFocusControl2> focusControl2;
    HRESULT hr = d->focusControl.As(&focusControl2);
    Q_ASSERT_SUCCEEDED(hr);
    ComPtr<IVectorView<enum FocusMode>> focusModes;
    hr = focusControl2->get_SupportedFocusModes(&focusModes);
    if (FAILED(hr)) {
        d->cameraFocusControl->setSupportedFocusMode(0);
        d->cameraFocusControl->setSupportedFocusPointMode(QSet<QCameraFocus::FocusPointMode>());
        qErrnoWarning(hr, "Failed to get camera supported focus mode list");
        return hr;
    }
    quint32 size;
    hr = focusModes->get_Size(&size);
    Q_ASSERT_SUCCEEDED(hr);
    QCameraFocus::FocusModes supportedModeFlag = 0;
    for (quint32 i = 0; i < size; ++i) {
        FocusMode mode;
        hr = focusModes->GetAt(i, &mode);
        Q_ASSERT_SUCCEEDED(hr);
        switch (mode) {
        case FocusMode_Continuous:
            supportedModeFlag |= QCameraFocus::ContinuousFocus;
            break;
        case FocusMode_Single:
            supportedModeFlag |= QCameraFocus::AutoFocus;
            break;
        default:
            break;
        }
    }

    ComPtr<IVectorView<enum AutoFocusRange>> focusRange;
    hr = focusControl2->get_SupportedFocusRanges(&focusRange);
    if (FAILED(hr)) {
        qErrnoWarning(hr, "Failed to get camera supported focus range list");
    } else {
        hr = focusRange->get_Size(&size);
        Q_ASSERT_SUCCEEDED(hr);
        for (quint32 i = 0; i < size; ++i) {
            AutoFocusRange range;
            hr = focusRange->GetAt(i, &range);
            Q_ASSERT_SUCCEEDED(hr);
            switch (range) {
            case AutoFocusRange_Macro:
                supportedModeFlag |= QCameraFocus::MacroFocus;
                break;
            case AutoFocusRange_FullRange:
                supportedModeFlag |= QCameraFocus::InfinityFocus;
                break;
            default:
                break;
            }
        }
    }
    d->cameraFocusControl->setSupportedFocusMode(supportedModeFlag);
    if (!d->regionsOfInterestControl) {
        d->cameraFocusControl->setSupportedFocusPointMode(QSet<QCameraFocus::FocusPointMode>());
        return S_OK;
    }
    boolean isRegionsfocusSupported = false;
    hr = d->regionsOfInterestControl->get_AutoFocusSupported(&isRegionsfocusSupported);
    Q_ASSERT_SUCCEEDED(hr);
    UINT32 maxRegions;
    hr = d->regionsOfInterestControl->get_MaxRegions(&maxRegions);
    Q_ASSERT_SUCCEEDED(hr);
    if (!isRegionsfocusSupported || maxRegions == 0) {
        d->cameraFocusControl->setSupportedFocusPointMode(QSet<QCameraFocus::FocusPointMode>());
        return S_OK;
    }
    QSet<QCameraFocus::FocusPointMode> supportedFocusPointModes;
    supportedFocusPointModes << QCameraFocus::FocusPointCustom
                             << QCameraFocus::FocusPointCenter
                             << QCameraFocus::FocusPointAuto;
    d->cameraFocusControl->setSupportedFocusPointMode(supportedFocusPointModes);
    return S_OK;
}

bool QWinRTCameraControl::setFocus(QCameraFocus::FocusModes modes)
{
    Q_D(QWinRTCameraControl);
    if (d->status == QCamera::UnloadedStatus)
        return false;

    bool result = false;
    HRESULT hr = QEventDispatcherWinRT::runOnXamlThread([modes, &result, d, this]() {
        ComPtr<IFocusSettings> focusSettings;
        ComPtr<IInspectable> focusSettingsObject;
        HRESULT hr = RoActivateInstance(HString::MakeReference(RuntimeClass_Windows_Media_Devices_FocusSettings).Get(), &focusSettingsObject);
        Q_ASSERT_SUCCEEDED(hr);
        hr = focusSettingsObject.As(&focusSettings);
        Q_ASSERT_SUCCEEDED(hr);
        FocusMode mode;
        if (modes.testFlag(QCameraFocus::ContinuousFocus)) {
            mode = FocusMode_Continuous;
        } else if (modes.testFlag(QCameraFocus::AutoFocus)
                   || modes.testFlag(QCameraFocus::MacroFocus)
                   || modes.testFlag(QCameraFocus::InfinityFocus)) {
            // The Macro and infinity focus modes are only supported in auto focus mode on WinRT.
            // QML camera focus doesn't support combined focus flags settings. In the case of macro
            // and infinity Focus modes, the auto focus setting is applied.
            mode = FocusMode_Single;
        } else {
            emit error(QCamera::NotSupportedFeatureError, QStringLiteral("Unsupported camera focus modes."));
            result = false;
            return S_OK;
        }
        hr = focusSettings->put_Mode(mode);
        Q_ASSERT_SUCCEEDED(hr);
        AutoFocusRange range = AutoFocusRange_Normal;
        if (modes.testFlag(QCameraFocus::MacroFocus))
            range = AutoFocusRange_Macro;
        else if (modes.testFlag(QCameraFocus::InfinityFocus))
            range = AutoFocusRange_FullRange;
        hr = focusSettings->put_AutoFocusRange(range);
        Q_ASSERT_SUCCEEDED(hr);
        hr = focusSettings->put_WaitForFocus(true);
        Q_ASSERT_SUCCEEDED(hr);
        hr = focusSettings->put_DisableDriverFallback(false);
        Q_ASSERT_SUCCEEDED(hr);

        ComPtr<IFocusControl2> focusControl2;
        hr = d->focusControl.As(&focusControl2);
        Q_ASSERT_SUCCEEDED(hr);
        hr = focusControl2->Configure(focusSettings.Get());
        result = SUCCEEDED(hr);
        RETURN_OK_IF_FAILED("Failed to configure camera focus control");
        return S_OK;
    });
    Q_ASSERT_SUCCEEDED(hr);
    Q_UNUSED(hr); // Silence release build
    return result;
}

bool QWinRTCameraControl::setFocusPoint(const QPointF &focusPoint)
{
    Q_D(QWinRTCameraControl);
    if (focusPoint.x() < FOCUS_RECT_POSITION_MIN || focusPoint.x() > FOCUS_RECT_BOUNDARY) {
        emit error(QCamera::CameraError, QStringLiteral("Focus horizontal location should be between 0.0 and 1.0."));
        return false;
    }

    if (focusPoint.y() < FOCUS_RECT_POSITION_MIN || focusPoint.y() > FOCUS_RECT_BOUNDARY) {
        emit error(QCamera::CameraError, QStringLiteral("Focus vertical location should be between 0.0 and 1.0."));
        return false;
    }

    ABI::Windows::Foundation::Rect rect;
    rect.X = qBound<float>(FOCUS_RECT_POSITION_MIN, focusPoint.x() - FOCUS_RECT_HALF_SIZE, FOCUS_RECT_POSITION_MAX);
    rect.Y = qBound<float>(FOCUS_RECT_POSITION_MIN, focusPoint.y() - FOCUS_RECT_HALF_SIZE, FOCUS_RECT_POSITION_MAX);
    rect.Width  = (rect.X + FOCUS_RECT_SIZE) < FOCUS_RECT_BOUNDARY ? FOCUS_RECT_SIZE : FOCUS_RECT_BOUNDARY - rect.X;
    rect.Height = (rect.Y + FOCUS_RECT_SIZE) < FOCUS_RECT_BOUNDARY ? FOCUS_RECT_SIZE : FOCUS_RECT_BOUNDARY - rect.Y;

    ComPtr<IRegionOfInterest> regionOfInterest;
    ComPtr<IInspectable> regionOfInterestObject;
    HRESULT hr = RoActivateInstance(HString::MakeReference(RuntimeClass_Windows_Media_Devices_RegionOfInterest).Get(), &regionOfInterestObject);
    Q_ASSERT_SUCCEEDED(hr);
    hr = regionOfInterestObject.As(&regionOfInterest);
    Q_ASSERT_SUCCEEDED(hr);
    ComPtr<IRegionOfInterest2> regionOfInterest2;
    hr = regionOfInterestObject.As(&regionOfInterest2);
    Q_ASSERT_SUCCEEDED(hr);
    hr = regionOfInterest2->put_BoundsNormalized(true);
    Q_ASSERT_SUCCEEDED(hr);
    hr = regionOfInterest2->put_Weight(1);
    Q_ASSERT_SUCCEEDED(hr);
    hr = regionOfInterest2->put_Type(RegionOfInterestType_Unknown);
    Q_ASSERT_SUCCEEDED(hr);
    hr = regionOfInterest->put_AutoFocusEnabled(true);
    Q_ASSERT_SUCCEEDED(hr);
    hr = regionOfInterest->put_Bounds(rect);
    Q_ASSERT_SUCCEEDED(hr);

    ComPtr<WindowsRegionOfInterestIterable> regionOfInterestIterable = Make<WindowsRegionOfInterestIterable>(regionOfInterest);
    ComPtr<IAsyncAction> op;
    hr = d->regionsOfInterestControl->SetRegionsAsync(regionOfInterestIterable.Get(), &op);
    Q_ASSERT_SUCCEEDED(hr);
    return QWinRTFunctions::await(op) == S_OK;
}

bool QWinRTCameraControl::focus()
{
    Q_D(QWinRTCameraControl);
    HRESULT hr;
    if (!d->focusControl)
        return false;

    QEventDispatcherWinRT::runOnXamlThread([&d, &hr]() {
        if (d->focusOperation) {
            ComPtr<IAsyncInfo> info;
            hr = d->focusOperation.As(&info);
            Q_ASSERT_SUCCEEDED(hr);

            AsyncStatus status = AsyncStatus::Completed;
            hr = info->get_Status(&status);
            Q_ASSERT_SUCCEEDED(hr);
            if (status == AsyncStatus::Started)
                return E_ASYNC_OPERATION_NOT_STARTED;
        }

        hr = d->focusControl->FocusAsync(&d->focusOperation);
        Q_ASSERT_SUCCEEDED(hr);

        const long errorCode = HRESULT_CODE(hr);
        if (errorCode == ERROR_OPERATION_IN_PROGRESS
                || errorCode == ERROR_WRITE_PROTECT) {
            return E_ASYNC_OPERATION_NOT_STARTED;
        }
        Q_ASSERT_SUCCEEDED(hr);
        return S_OK;
    });

    return hr == S_OK;
}

void QWinRTCameraControl::clearFocusPoint()
{
    Q_D(QWinRTCameraControl);
    if (!d->focusControl)
        return;
    ComPtr<IAsyncAction> op;
    HRESULT hr = d->regionsOfInterestControl->ClearRegionsAsync(&op);
    Q_ASSERT_SUCCEEDED(hr);
    hr = QWinRTFunctions::await(op);
    Q_ASSERT_SUCCEEDED(hr);
}

bool QWinRTCameraControl::lockFocus()
{
    Q_D(QWinRTCameraControl);
    if (!d->focusControl)
        return false;

    bool result = false;
    ComPtr<IAsyncAction> op;
    HRESULT hr;
    hr = QEventDispatcherWinRT::runOnXamlThread([d, &result, &op]() {
        ComPtr<IFocusControl2> focusControl2;
        HRESULT hr = d->focusControl.As(&focusControl2);
        Q_ASSERT_SUCCEEDED(hr);
        hr = focusControl2->LockAsync(&op);
        if (HRESULT_CODE(hr) == ERROR_WRITE_PROTECT)
            return S_OK;
        Q_ASSERT_SUCCEEDED(hr);
        result = true;
        return hr;
    });
    return result ? (QWinRTFunctions::await(op) == S_OK) : false;
}

bool QWinRTCameraControl::unlockFocus()
{
    Q_D(QWinRTCameraControl);
    if (!d->focusControl)
        return false;

    bool result = false;
    ComPtr<IAsyncAction> op;
    HRESULT hr;
    hr = QEventDispatcherWinRT::runOnXamlThread([d, &result, &op]() {
        ComPtr<IFocusControl2> focusControl2;
        HRESULT hr = d->focusControl.As(&focusControl2);
        Q_ASSERT_SUCCEEDED(hr);
        hr = focusControl2->UnlockAsync(&op);
        if (HRESULT_CODE(hr) == ERROR_WRITE_PROTECT)
            return S_OK;
        Q_ASSERT_SUCCEEDED(hr);
        result = true;
        return hr;
    });
    return result ? (QWinRTFunctions::await(op) == S_OK) : false;
}

#else // !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_PHONE_APP)

HRESULT QWinRTCameraControl::initializeFocus()
{
    Q_D(QWinRTCameraControl);
    d->cameraFocusControl->setSupportedFocusMode(0);
    d->cameraFocusControl->setSupportedFocusPointMode(QSet<QCameraFocus::FocusPointMode>());
    return S_OK;
}

bool QWinRTCameraControl::setFocus(QCameraFocus::FocusModes modes)
{
    Q_UNUSED(modes)
    return false;
}

bool QWinRTCameraControl::setFocusPoint(const QPointF &focusPoint)
{
    Q_UNUSED(focusPoint)
    return false;
}

bool QWinRTCameraControl::focus()
{
    return false;
}

void QWinRTCameraControl::clearFocusPoint()
{
}

bool QWinRTCameraControl::lockFocus()
{
    return false;
}

bool QWinRTCameraControl::unlockFocus()
{
    return false;
}

#endif // !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_PHONE_APP)

void QWinRTCameraControl::frameMapped()
{
    Q_D(QWinRTCameraControl);
    ++d->framesMapped;
}

void QWinRTCameraControl::frameUnmapped()
{
    Q_D(QWinRTCameraControl);
    --d->framesMapped;
    Q_ASSERT(d->framesMapped >= 0);
    if (!d->framesMapped && d->delayClose && d->delayClose->isRunning())
        d->delayClose->exit();
}

HRESULT QWinRTCameraControl::onCaptureFailed(IMediaCapture *, IMediaCaptureFailedEventArgs *args)
{
    qCDebug(lcMMCamera) << __FUNCTION__ << args;
    HRESULT hr;
    UINT32 code;
    hr = args->get_Code(&code);
    RETURN_HR_IF_FAILED("Failed to get error code");
    HString message;
    args->get_Message(message.GetAddressOf());
    RETURN_HR_IF_FAILED("Failed to get error message");
    quint32 messageLength;
    const wchar_t *messageBuffer = message.GetRawBuffer(&messageLength);
    emit error(QCamera::CameraError, QString::fromWCharArray(messageBuffer, messageLength));
    setState(QCamera::LoadedState);
    return S_OK;
}

HRESULT QWinRTCameraControl::onRecordLimitationExceeded(IMediaCapture *)
{
    qCDebug(lcMMCamera) << __FUNCTION__;
    emit error(QCamera::CameraError, QStringLiteral("Recording limit exceeded."));
    setState(QCamera::LoadedState);
    return S_OK;
}

void QWinRTCameraControl::emitError(int errorCode, const QString &errorString)
{
    qCDebug(lcMMCamera) << __FUNCTION__ << errorString << errorCode;
    emit error(errorCode, errorString);
}

QT_END_NAMESPACE
