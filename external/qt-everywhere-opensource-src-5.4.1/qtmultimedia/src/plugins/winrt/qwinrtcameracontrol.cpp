/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwinrtcameracontrol.h"
#include "qwinrtcameravideorenderercontrol.h"
#include "qwinrtvideodeviceselectorcontrol.h"
#include "qwinrtcameraimagecapturecontrol.h"

#include <QtCore/qfunctions_winrt.h>
#include <QtCore/QCoreApplication>
#include <QtCore/QPointer>

#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <wrl.h>
#include <windows.devices.enumeration.h>
#include <windows.media.capture.h>
#include <windows.storage.streams.h>

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

QT_USE_NAMESPACE

#define RETURN_VOID_AND_EMIT_ERROR(msg) \
    if (FAILED(hr)) { \
        emit error(QCamera::CameraError, qt_error_string(hr)); \
        RETURN_VOID_IF_FAILED(msg); \
    }

class CriticalSectionLocker
{
public:
    CriticalSectionLocker(CRITICAL_SECTION *section)
        : m_section(section)
    {
        EnterCriticalSection(m_section);
    }
    ~CriticalSectionLocker()
    {
        LeaveCriticalSection(m_section);
    }
private:
    CRITICAL_SECTION *m_section;
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
        m_videoRenderer->setActive(false);
        return m_presentationClock->Stop();
    }

    HRESULT __stdcall OnClockStart(MFTIME systemTime, LONGLONG clockStartOffset) Q_DECL_OVERRIDE
    {
        Q_UNUSED(systemTime);
        Q_UNUSED(clockStartOffset);

        m_videoRenderer->setActive(true);

        return S_OK;
    }

    HRESULT __stdcall OnClockStop(MFTIME systemTime) Q_DECL_OVERRIDE
    {
        Q_UNUSED(systemTime);

        m_videoRenderer->setActive(false);

        return m_stream->QueueEvent(MEStreamSinkStopped, GUID_NULL, S_OK, Q_NULLPTR);
    }

    HRESULT __stdcall OnClockPause(MFTIME systemTime) Q_DECL_OVERRIDE
    {
        Q_UNUSED(systemTime);

        m_videoRenderer->setActive(false);

        return m_stream->QueueEvent(MEStreamSinkPaused, GUID_NULL, S_OK, Q_NULLPTR);
    }

    HRESULT __stdcall OnClockRestart(MFTIME systemTime) Q_DECL_OVERRIDE
    {
        Q_UNUSED(systemTime);

        m_videoRenderer->setActive(true);

        return m_stream->QueueEvent(MEStreamSinkStarted, GUID_NULL, S_OK, Q_NULLPTR);
    }

    HRESULT __stdcall OnClockSetRate(MFTIME systemTime, float rate) Q_DECL_OVERRIDE
    {
        Q_UNUSED(systemTime);
        Q_UNUSED(rate);
        return E_NOTIMPL;
    }

private:
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

    QSize size;
    QPointer<QWinRTCameraVideoRendererControl> videoRenderer;
    QPointer<QWinRTVideoDeviceSelectorControl> videoDeviceSelector;
    QPointer<QWinRTCameraImageCaptureControl> imageCaptureControl;
};

QWinRTCameraControl::QWinRTCameraControl(QObject *parent)
    : QCameraControl(parent), d_ptr(new QWinRTCameraControlPrivate)
{
    Q_D(QWinRTCameraControl);

    d->state = QCamera::UnloadedState;
    d->status = QCamera::UnloadedStatus;
    d->captureMode = QCamera::CaptureStillImage;
    d->captureFailedCookie.value = 0;
    d->recordLimitationCookie.value = 0;
    d->videoRenderer = new QWinRTCameraVideoRendererControl(d->size, this);
    connect(d->videoRenderer, &QWinRTCameraVideoRendererControl::bufferRequested,
            this, &QWinRTCameraControl::onBufferRequested);
    d->videoDeviceSelector = new QWinRTVideoDeviceSelectorControl(this);
    d->imageCaptureControl = new QWinRTCameraImageCaptureControl(this);
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

        d->mediaSink = Make<MediaSink>(d->encodingProfile.Get(), d->videoRenderer);
        ComPtr<IAsyncAction> op;
        hr = d->capturePreview->StartPreviewToCustomSinkAsync(d->encodingProfile.Get(), d->mediaSink.Get(), &op);
        RETURN_VOID_AND_EMIT_ERROR("Failed to initiate capture");
        if (d->status != QCamera::StartingStatus) {
            d->status = QCamera::StartingStatus;
            emit statusChanged(d->status);
        }

        hr = QWinRTFunctions::await(op);
        if (FAILED(hr)) {
            emit error(QCamera::CameraError, qt_error_string(hr));
            setState(QCamera::UnloadedState); // Unload everything, as initialize() will need be called again
            return;
        }

        d->state = QCamera::ActiveState;
        emit stateChanged(d->state);
        d->status = QCamera::ActiveStatus;
        emit statusChanged(d->status);
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
            ComPtr<IAsyncAction> op;
            hr = d->capturePreview->StopPreviewAsync(&op);
            RETURN_VOID_AND_EMIT_ERROR("Failed to stop camera preview");
            if (d->status != QCamera::StoppingStatus) {
                d->status = QCamera::StoppingStatus;
                emit statusChanged(d->status);
            }
            Q_ASSERT_SUCCEEDED(hr);
            hr = QWinRTFunctions::await(op); // Synchronize unloading
            if (FAILED(hr))
                emit error(QCamera::InvalidRequestError, qt_error_string(hr));

            d->mediaSink->Shutdown();
            d->mediaSink.Reset();

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
            RETURN_VOID_AND_EMIT_ERROR("Failed to close the capture manger");
            d->capture.Reset();
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

IMediaCapture *QWinRTCameraControl::handle() const
{
    Q_D(const QWinRTCameraControl);
    return d->capture.Get();
}

QSize QWinRTCameraControl::imageSize() const
{
    Q_D(const QWinRTCameraControl);
    return d->size;
}

void QWinRTCameraControl::onBufferRequested()
{
    Q_D(QWinRTCameraControl);

    if (d->mediaSink)
        d->mediaSink->RequestSample();
}

HRESULT QWinRTCameraControl::initialize()
{
    Q_D(QWinRTCameraControl);

    if (d->status != QCamera::LoadingStatus) {
        d->status = QCamera::LoadingStatus;
        emit statusChanged(d->status);
    }

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

    if (d->videoDeviceSelector->cameraPosition(deviceName) == QCamera::FrontFace)
        d->videoRenderer->setScanLineDirection(QVideoSurfaceFormat::BottomToTop);

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
    Q_ASSERT_SUCCEEDED(hr);
    ComPtr<IMediaDeviceController> deviceController;
    hr = videoDeviceController.As(&deviceController);
    Q_ASSERT_SUCCEEDED(hr);
    ComPtr<IVectorView<IMediaEncodingProperties *>> encodingPropertiesList;
    MediaStreamType mediaStreamType;
    switch (d->captureMode) {
    default:
    case QCamera::CaptureViewfinder:
        mediaStreamType = MediaStreamType_VideoPreview;
        break;
    case QCamera::CaptureStillImage:
        mediaStreamType = MediaStreamType_Photo;
        break;
    case QCamera::CaptureVideo:
        mediaStreamType = MediaStreamType_VideoRecord;
        break;
    }
    hr = deviceController->GetAvailableMediaStreamProperties(mediaStreamType, &encodingPropertiesList);
    Q_ASSERT_SUCCEEDED(hr);

    d->size = QSize();
    ComPtr<IVideoEncodingProperties> videoEncodingProperties;
    quint32 encodingPropertiesListSize;
    hr = encodingPropertiesList->get_Size(&encodingPropertiesListSize);
    Q_ASSERT_SUCCEEDED(hr);
    for (quint32 i = 0; i < encodingPropertiesListSize; ++i) {
        ComPtr<IMediaEncodingProperties> properties;
        hr = encodingPropertiesList->GetAt(i, &properties);
        Q_ASSERT_SUCCEEDED(hr);
        ComPtr<IVideoEncodingProperties> videoProperties;
        hr = properties.As(&videoProperties);
        Q_ASSERT_SUCCEEDED(hr);
        UINT32 width, height;
        hr = videoProperties->get_Width(&width);
        Q_ASSERT_SUCCEEDED(hr);
        hr = videoProperties->get_Height(&height);
        Q_ASSERT_SUCCEEDED(hr);
        // Choose the highest-quality format
        if (int(width * height) > d->size.width() * d->size.height()) {
            d->size = QSize(width, height);
            videoEncodingProperties = videoProperties;
        }
    }

    if (!videoEncodingProperties || d->size.isEmpty()) {
        hr = MF_E_INVALID_FORMAT;
        RETURN_HR_IF_FAILED("Failed to find a suitable video format");
    }

    hr = RoActivateInstance(HString::MakeReference(RuntimeClass_Windows_Media_MediaProperties_MediaEncodingProfile).Get(),
                            &d->encodingProfile);
    Q_ASSERT_SUCCEEDED(hr);
    hr = d->encodingProfile->put_Video(videoEncodingProperties.Get());
    Q_ASSERT_SUCCEEDED(hr);
    if (d->videoRenderer)
        d->videoRenderer->setSize(d->size);

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

HRESULT QWinRTCameraControl::onCaptureFailed(IMediaCapture *, IMediaCaptureFailedEventArgs *args)
{
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
    emit error(QCamera::CameraError, QStringLiteral("Recording limit exceeded."));
    setState(QCamera::LoadedState);
    return S_OK;
}
