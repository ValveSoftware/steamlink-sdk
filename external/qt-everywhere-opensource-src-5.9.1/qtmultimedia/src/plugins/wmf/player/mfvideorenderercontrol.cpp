/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Mobility Components.
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

#include "mfvideorenderercontrol.h"
#include "mfactivate.h"

#include "evrcustompresenter.h"

#include <qabstractvideosurface.h>
#include <qvideosurfaceformat.h>
#include <qtcore/qtimer.h>
#include <qtcore/qmutex.h>
#include <qtcore/qcoreevent.h>
#include <qtcore/qcoreapplication.h>
#include <qtcore/qthread.h>
#include "guiddef.h"
#include <qtcore/qdebug.h>
#include <QtMultimedia/private/qmediaopenglhelper_p.h>

//#define DEBUG_MEDIAFOUNDATION
#define PAD_TO_DWORD(x)  (((x) + 3) & ~3)

namespace
{
    class MediaSampleVideoBuffer : public QAbstractVideoBuffer
    {
    public:
        MediaSampleVideoBuffer(IMFMediaBuffer *buffer, int bytesPerLine)
            : QAbstractVideoBuffer(NoHandle)
            , m_buffer(buffer)
            , m_bytesPerLine(bytesPerLine)
            , m_mapMode(NotMapped)
        {
            buffer->AddRef();
        }

        ~MediaSampleVideoBuffer()
        {
            m_buffer->Release();
        }

        uchar *map(MapMode mode, int *numBytes, int *bytesPerLine)
        {
            if (m_mapMode == NotMapped && mode != NotMapped) {
                BYTE *bytes;
                DWORD length;
                HRESULT hr = m_buffer->Lock(&bytes, NULL, &length);
                if (SUCCEEDED(hr)) {
                    if (numBytes)
                        *numBytes = int(length);

                    if (bytesPerLine)
                        *bytesPerLine = m_bytesPerLine;

                    m_mapMode = mode;
                    return reinterpret_cast<uchar *>(bytes);
                } else {
                    qWarning("Faild to lock mf buffer!");
                }
            }
            return 0;
        }

        void unmap()
        {
            if (m_mapMode == NotMapped)
                return;
            m_mapMode = NotMapped;
            m_buffer->Unlock();
        }

        MapMode mapMode() const
        {
            return m_mapMode;
        }

    private:
        IMFMediaBuffer *m_buffer;
        int m_bytesPerLine;
        MapMode m_mapMode;
    };

    // Custom interface for handling IMFStreamSink::PlaceMarker calls asynchronously.
    MIDL_INTERFACE("a3ff32de-1031-438a-8b47-82f8acda59b7")
    IMarker : public IUnknown
    {
        virtual STDMETHODIMP GetMarkerType(MFSTREAMSINK_MARKER_TYPE *pType) = 0;
        virtual STDMETHODIMP GetMarkerValue(PROPVARIANT *pvar) = 0;
        virtual STDMETHODIMP GetContext(PROPVARIANT *pvar) = 0;
    };

    class Marker : public IMarker
    {
    public:
        static HRESULT Create(
            MFSTREAMSINK_MARKER_TYPE eMarkerType,
            const PROPVARIANT* pvarMarkerValue,  // Can be NULL.
            const PROPVARIANT* pvarContextValue, // Can be NULL.
            IMarker **ppMarker)
        {
            if (ppMarker == NULL)
                return E_POINTER;

            HRESULT hr = S_OK;
            Marker *pMarker = new Marker(eMarkerType);
            if (pMarker == NULL)
                hr = E_OUTOFMEMORY;

            // Copy the marker data.
            if (SUCCEEDED(hr) && pvarMarkerValue)
                hr = PropVariantCopy(&pMarker->m_varMarkerValue, pvarMarkerValue);

            if (SUCCEEDED(hr) && pvarContextValue)
                hr = PropVariantCopy(&pMarker->m_varContextValue, pvarContextValue);

            if (SUCCEEDED(hr)) {
                *ppMarker = pMarker;
                (*ppMarker)->AddRef();
            }

            if (pMarker)
                pMarker->Release();

            return hr;
        }

        // IUnknown methods.
        STDMETHODIMP QueryInterface(REFIID iid, void** ppv)
        {
            if (!ppv)
                return E_POINTER;
            if (iid == IID_IUnknown) {
                *ppv = static_cast<IUnknown*>(this);
            } else if (iid == __uuidof(IMarker)) {
                *ppv = static_cast<IMarker*>(this);
            } else {
                *ppv = NULL;
                return E_NOINTERFACE;
            }
            AddRef();
            return S_OK;
        }

        STDMETHODIMP_(ULONG) AddRef()
        {
            return InterlockedIncrement(&m_cRef);
        }

        STDMETHODIMP_(ULONG) Release()
        {
            LONG cRef = InterlockedDecrement(&m_cRef);
            if (cRef == 0)
                delete this;
            // For thread safety, return a temporary variable.
            return cRef;
        }

        STDMETHODIMP GetMarkerType(MFSTREAMSINK_MARKER_TYPE *pType)
        {
            if (pType == NULL)
                return E_POINTER;
            *pType = m_eMarkerType;
            return S_OK;
        }

        STDMETHODIMP GetMarkerValue(PROPVARIANT *pvar)
        {
            if (pvar == NULL)
                return E_POINTER;
            return PropVariantCopy(pvar, &m_varMarkerValue);
        }

        STDMETHODIMP GetContext(PROPVARIANT *pvar)
        {
            if (pvar == NULL)
                return E_POINTER;
            return PropVariantCopy(pvar, &m_varContextValue);
        }

    protected:
        MFSTREAMSINK_MARKER_TYPE m_eMarkerType;
        PROPVARIANT m_varMarkerValue;
        PROPVARIANT m_varContextValue;

    private:
        long    m_cRef;

        Marker(MFSTREAMSINK_MARKER_TYPE eMarkerType) : m_cRef(1), m_eMarkerType(eMarkerType)
        {
            PropVariantInit(&m_varMarkerValue);
            PropVariantInit(&m_varContextValue);
        }

        virtual ~Marker()
        {
            PropVariantClear(&m_varMarkerValue);
            PropVariantClear(&m_varContextValue);
        }
    };

    class MediaStream : public QObject, public IMFStreamSink, public IMFMediaTypeHandler
    {
        Q_OBJECT
        friend class MFVideoRendererControl;
    public:
        static const DWORD DEFAULT_MEDIA_STREAM_ID = 0x0;

        MediaStream(IMFMediaSink *parent, MFVideoRendererControl *rendererControl)
            : m_cRef(1)
            , m_eventQueue(0)
            , m_shutdown(false)
            , m_surface(0)
            , m_state(State_TypeNotSet)
            , m_currentFormatIndex(-1)
            , m_bytesPerLine(0)
            , m_workQueueId(0)
            , m_workQueueCB(this, &MediaStream::onDispatchWorkItem)
            , m_finalizeResult(0)
            , m_scheduledBuffer(0)
            , m_bufferStartTime(-1)
            , m_bufferDuration(-1)
            , m_presentationClock(0)
            , m_sampleRequested(false)
            , m_currentMediaType(0)
            , m_prerolling(false)
            , m_prerollTargetTime(0)
            , m_startTime(0)
            , m_rendererControl(rendererControl)
            , m_rate(1.f)
        {
            m_sink = parent;

            if (FAILED(MFCreateEventQueue(&m_eventQueue)))
                qWarning("Failed to create mf event queue!");
            if (FAILED(MFAllocateWorkQueue(&m_workQueueId)))
                qWarning("Failed to allocated mf work queue!");
        }

        ~MediaStream()
        {
            Q_ASSERT(m_shutdown);
        }

        //from IUnknown
        STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject)
        {
            if (!ppvObject)
                return E_POINTER;
            if (riid == IID_IMFStreamSink) {
                *ppvObject = static_cast<IMFStreamSink*>(this);
            } else if (riid == IID_IMFMediaEventGenerator) {
                *ppvObject = static_cast<IMFMediaEventGenerator*>(this);
            } else if (riid == IID_IMFMediaTypeHandler) {
                *ppvObject = static_cast<IMFMediaTypeHandler*>(this);
            } else if (riid == IID_IUnknown) {
                *ppvObject = static_cast<IUnknown*>(static_cast<IMFStreamSink*>(this));
            } else {
                *ppvObject =  NULL;
                return E_NOINTERFACE;
            }
            AddRef();
            return S_OK;
        }

        STDMETHODIMP_(ULONG) AddRef(void)
        {
            return InterlockedIncrement(&m_cRef);
        }

        STDMETHODIMP_(ULONG) Release(void)
        {
            LONG cRef = InterlockedDecrement(&m_cRef);
            if (cRef == 0)
                delete this;
            // For thread safety, return a temporary variable.
            return cRef;
        }

        //from IMFMediaEventGenerator
        STDMETHODIMP GetEvent(
            DWORD dwFlags,
            IMFMediaEvent **ppEvent)
        {
            // GetEvent can block indefinitely, so we don't hold the lock.
            // This requires some juggling with the event queue pointer.
            HRESULT hr = S_OK;
            IMFMediaEventQueue *queue = NULL;

            m_mutex.lock();
            if (m_shutdown)
                hr = MF_E_SHUTDOWN;
            if (SUCCEEDED(hr)) {
                queue = m_eventQueue;
                queue->AddRef();
            }
            m_mutex.unlock();

            // Now get the event.
            if (SUCCEEDED(hr)) {
                hr = queue->GetEvent(dwFlags, ppEvent);
                queue->Release();
            }

            return hr;
        }

        STDMETHODIMP BeginGetEvent(
            IMFAsyncCallback *pCallback,
            IUnknown *punkState)
        {
            QMutexLocker locker(&m_mutex);
            if (m_shutdown)
                return MF_E_SHUTDOWN;
            return m_eventQueue->BeginGetEvent(pCallback, punkState);
        }

        STDMETHODIMP EndGetEvent(
            IMFAsyncResult *pResult,
            IMFMediaEvent **ppEvent)
        {
            QMutexLocker locker(&m_mutex);
            if (m_shutdown)
                return MF_E_SHUTDOWN;
            return m_eventQueue->EndGetEvent(pResult, ppEvent);
        }

        STDMETHODIMP QueueEvent(
            MediaEventType met,
            REFGUID guidExtendedType,
            HRESULT hrStatus,
            const PROPVARIANT *pvValue)
        {
#ifdef DEBUG_MEDIAFOUNDATION
            qDebug() << "MediaStream::QueueEvent" << met;
#endif
            QMutexLocker locker(&m_mutex);
            if (m_shutdown)
                return MF_E_SHUTDOWN;
            return m_eventQueue->QueueEventParamVar(met, guidExtendedType, hrStatus, pvValue);
        }

        //from IMFStreamSink
        STDMETHODIMP GetMediaSink(
            IMFMediaSink **ppMediaSink)
        {
            QMutexLocker locker(&m_mutex);
            if (m_shutdown)
                return MF_E_SHUTDOWN;
            else if (!ppMediaSink)
                return E_INVALIDARG;

            m_sink->AddRef();
            *ppMediaSink = m_sink;
            return S_OK;
        }

        STDMETHODIMP GetIdentifier(
            DWORD *pdwIdentifier)
        {
            *pdwIdentifier = MediaStream::DEFAULT_MEDIA_STREAM_ID;
            return S_OK;
        }

        STDMETHODIMP GetMediaTypeHandler(
            IMFMediaTypeHandler **ppHandler)
        {
            LPVOID handler = NULL;
            HRESULT hr = QueryInterface(IID_IMFMediaTypeHandler, &handler);
            *ppHandler = (IMFMediaTypeHandler*)(handler);
            return hr;
        }

        STDMETHODIMP ProcessSample(
            IMFSample *pSample)
        {
            if (pSample == NULL)
                return E_INVALIDARG;
            HRESULT hr = S_OK;
            QMutexLocker locker(&m_mutex);
            if (m_shutdown)
                return MF_E_SHUTDOWN;

            if (!m_prerolling) {
                hr = validateOperation(OpProcessSample);
                if (FAILED(hr))
                    return hr;
            }

            pSample->AddRef();
            m_sampleQueue.push_back(pSample);

            // Unless we are paused, start an async operation to dispatch the next sample.
            if (m_state != State_Paused)
                hr = queueAsyncOperation(OpProcessSample);

            return hr;
        }

        STDMETHODIMP PlaceMarker(
            MFSTREAMSINK_MARKER_TYPE eMarkerType,
            const PROPVARIANT *pvarMarkerValue,
            const PROPVARIANT *pvarContextValue)
        {
            HRESULT hr = S_OK;
            QMutexLocker locker(&m_mutex);
            IMarker *pMarker = NULL;
            if (m_shutdown)
                return MF_E_SHUTDOWN;

            hr = validateOperation(OpPlaceMarker);
            if (FAILED(hr))
                return hr;

            // Create a marker object and put it on the sample queue.
            hr = Marker::Create(eMarkerType, pvarMarkerValue, pvarContextValue, &pMarker);
            if (FAILED(hr))
                return hr;

            m_sampleQueue.push_back(pMarker);

            // Unless we are paused, start an async operation to dispatch the next sample/marker.
            if (m_state != State_Paused)
                hr = queueAsyncOperation(OpPlaceMarker); // Increments ref count on pOp.
            return hr;
        }

        STDMETHODIMP Flush( void)
        {
#ifdef DEBUG_MEDIAFOUNDATION
            qDebug() << "MediaStream::Flush";
#endif
            QMutexLocker locker(&m_mutex);
            if (m_shutdown)
                return MF_E_SHUTDOWN;
            // Note: Even though we are flushing data, we still need to send
            // any marker events that were queued.
            clearBufferCache();
            return processSamplesFromQueue(DropSamples);
        }

        //from IMFMediaTypeHandler
        STDMETHODIMP IsMediaTypeSupported(
            IMFMediaType *pMediaType,
            IMFMediaType **ppMediaType)
        {
            if (ppMediaType)
                *ppMediaType = NULL;
            QMutexLocker locker(&m_mutex);
            if (m_shutdown)
                return MF_E_SHUTDOWN;

            int index = getMediaTypeIndex(pMediaType);
            if (index < 0) {
                if (ppMediaType && m_mediaTypes.size() > 0) {
                    *ppMediaType = m_mediaTypes[0];
                    (*ppMediaType)->AddRef();
                }
                return MF_E_INVALIDMEDIATYPE;
            }

            BOOL compressed = TRUE;
            pMediaType->IsCompressedFormat(&compressed);
            if (compressed) {
                if (ppMediaType && (SUCCEEDED(MFCreateMediaType(ppMediaType)))) {
                    (*ppMediaType)->CopyAllItems(pMediaType);
                    (*ppMediaType)->SetUINT32(MF_MT_FIXED_SIZE_SAMPLES, TRUE);
                    (*ppMediaType)->SetUINT32(MF_MT_COMPRESSED, FALSE);
                    (*ppMediaType)->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
                }
                return MF_E_INVALIDMEDIATYPE;
            }

            return S_OK;
        }

        STDMETHODIMP GetMediaTypeCount(
            DWORD *pdwTypeCount)
        {
            if (pdwTypeCount == NULL)
                return E_INVALIDARG;
            QMutexLocker locker(&m_mutex);
            *pdwTypeCount = DWORD(m_mediaTypes.size());
            return S_OK;
        }

        STDMETHODIMP GetMediaTypeByIndex(
            DWORD dwIndex,
            IMFMediaType **ppType)
        {
            if (ppType == NULL)
                return E_INVALIDARG;
            HRESULT hr = S_OK;
            QMutexLocker locker(&m_mutex);
            if (m_shutdown)
                hr = MF_E_SHUTDOWN;

            if (SUCCEEDED(hr)) {
                if (dwIndex >= DWORD(m_mediaTypes.size()))
                    hr = MF_E_NO_MORE_TYPES;
            }

            if (SUCCEEDED(hr)) {
                *ppType = m_mediaTypes[dwIndex];
                (*ppType)->AddRef();
            }
            return hr;
        }

        STDMETHODIMP SetCurrentMediaType(
            IMFMediaType *pMediaType)
        {
            HRESULT hr = S_OK;
            QMutexLocker locker(&m_mutex);
            if (m_shutdown)
                return MF_E_SHUTDOWN;

            DWORD flag = MF_MEDIATYPE_EQUAL_MAJOR_TYPES |
                         MF_MEDIATYPE_EQUAL_FORMAT_TYPES |
                         MF_MEDIATYPE_EQUAL_FORMAT_DATA;

            if (m_currentMediaType && (m_currentMediaType->IsEqual(pMediaType, &flag) == S_OK))
                return S_OK;

            hr = validateOperation(OpSetMediaType);

            if (SUCCEEDED(hr)) {
                int index = getMediaTypeIndex(pMediaType);
                if (index >= 0) {
                    UINT64 size;
                    hr = pMediaType->GetUINT64(MF_MT_FRAME_SIZE, &size);
                    if (SUCCEEDED(hr)) {
                        m_currentFormatIndex = index;
                        int width = int(HI32(size));
                        int height = int(LO32(size));
                        QVideoSurfaceFormat format(QSize(width, height), m_pixelFormats[index]);
                        m_surfaceFormat = format;

                        MFVideoArea viewport;
                        if (SUCCEEDED(pMediaType->GetBlob(MF_MT_GEOMETRIC_APERTURE,
                                                          reinterpret_cast<UINT8*>(&viewport),
                                                          sizeof(MFVideoArea),
                                                          NULL))) {

                            m_surfaceFormat.setViewport(QRect(viewport.OffsetX.value,
                                                              viewport.OffsetY.value,
                                                              viewport.Area.cx,
                                                              viewport.Area.cy));
                        }

                        if (FAILED(pMediaType->GetUINT32(MF_MT_DEFAULT_STRIDE, (UINT32*)&m_bytesPerLine))) {
                            m_bytesPerLine = getBytesPerLine(format);
                        }

                        m_state = State_Ready;
                        if (m_currentMediaType)
                            m_currentMediaType->Release();
                        m_currentMediaType = pMediaType;
                        pMediaType->AddRef();
                    }
                } else {
                    hr = MF_E_INVALIDREQUEST;
                }
            }
            return hr;
        }

        STDMETHODIMP GetCurrentMediaType(
            IMFMediaType **ppMediaType)
        {
            if (ppMediaType == NULL)
                return E_INVALIDARG;
            QMutexLocker locker(&m_mutex);
            if (m_shutdown)
                return MF_E_SHUTDOWN;
            if (m_currentFormatIndex < 0)
                return MF_E_NOT_INITIALIZED;
            *ppMediaType = m_currentMediaType;
            (*ppMediaType)->AddRef();
            return S_OK;
        }

        STDMETHODIMP GetMajorType(
            GUID *pguidMajorType)
        {
            if (pguidMajorType == NULL)
                return E_INVALIDARG;
            *pguidMajorType = MFMediaType_Video;
            return S_OK;
        }

        //
        void setSurface(QAbstractVideoSurface *surface)
        {
            m_mutex.lock();
            m_surface = surface;
            m_mutex.unlock();
            supportedFormatsChanged();
        }

        void setClock(IMFPresentationClock *presentationClock)
        {
            QMutexLocker locker(&m_mutex);
            if (!m_shutdown) {
                if (m_presentationClock)
                    m_presentationClock->Release();
                m_presentationClock = presentationClock;
                if (m_presentationClock)
                    m_presentationClock->AddRef();
            }
        }

        void shutdown()
        {
            QMutexLocker locker(&m_mutex);
            Q_ASSERT(!m_shutdown);

            if (m_currentMediaType) {
                m_currentMediaType->Release();
                m_currentMediaType = NULL;
                m_currentFormatIndex = -1;
            }

            if (m_eventQueue)
                m_eventQueue->Shutdown();

            MFUnlockWorkQueue(m_workQueueId);

            if (m_presentationClock) {
                m_presentationClock->Release();
                m_presentationClock = NULL;
            }

            clearMediaTypes();
            clearSampleQueue();
            clearBufferCache();

            if (m_eventQueue) {
                m_eventQueue->Release();
                m_eventQueue = NULL;
            }

            m_shutdown = true;
        }

        HRESULT startPreroll(MFTIME hnsUpcomingStartTime)
        {
            QMutexLocker locker(&m_mutex);
            HRESULT hr = validateOperation(OpPreroll);
            if (SUCCEEDED(hr)) {
                m_state = State_Prerolling;
                m_prerollTargetTime = hnsUpcomingStartTime;
                hr = queueAsyncOperation(OpPreroll);
            }
            return hr;
        }

        HRESULT finalize(IMFAsyncCallback *pCallback, IUnknown *punkState)
        {
            QMutexLocker locker(&m_mutex);
            HRESULT hr = S_OK;
            hr = validateOperation(OpFinalize);
            if (SUCCEEDED(hr) && m_finalizeResult != NULL)
                hr = MF_E_INVALIDREQUEST;  // The operation is already pending.

            // Create and store the async result object.
            if (SUCCEEDED(hr))
                hr = MFCreateAsyncResult(NULL, pCallback, punkState, &m_finalizeResult);

            if (SUCCEEDED(hr)) {
                m_state = State_Finalized;
                hr = queueAsyncOperation(OpFinalize);
            }
            return hr;
        }

        HRESULT start(MFTIME start)
        {
#ifdef DEBUG_MEDIAFOUNDATION
            qDebug() << "MediaStream::start" << start;
#endif
            HRESULT hr = S_OK;
            QMutexLocker locker(&m_mutex);
            if (m_rate != 0)
                hr = validateOperation(OpStart);

            if (SUCCEEDED(hr)) {
                MFTIME sysTime;
                if (start != PRESENTATION_CURRENT_POSITION)
                    m_startTime = start;        // Cache the start time.
                else
                    m_presentationClock->GetCorrelatedTime(0, &m_startTime, &sysTime);
                m_state = State_Started;
                hr = queueAsyncOperation(OpStart);
            }
            return hr;
        }

        HRESULT restart()
        {
#ifdef DEBUG_MEDIAFOUNDATION
            qDebug() << "MediaStream::restart";
#endif
            QMutexLocker locker(&m_mutex);
            HRESULT hr = validateOperation(OpRestart);
            if (SUCCEEDED(hr)) {
                m_state = State_Started;
                hr = queueAsyncOperation(OpRestart);
            }
            return hr;
        }

        HRESULT stop()
        {
#ifdef DEBUG_MEDIAFOUNDATION
            qDebug() << "MediaStream::stop";
#endif
            QMutexLocker locker(&m_mutex);
            HRESULT hr = validateOperation(OpStop);
            if (SUCCEEDED(hr)) {
                m_state = State_Stopped;
                hr = queueAsyncOperation(OpStop);
            }
            return hr;
        }

        HRESULT pause()
        {
#ifdef DEBUG_MEDIAFOUNDATION
            qDebug() << "MediaStream::pause";
#endif
            QMutexLocker locker(&m_mutex);
            HRESULT hr = validateOperation(OpPause);
            if (SUCCEEDED(hr)) {
                m_state = State_Paused;
                hr = queueAsyncOperation(OpPause);
            }
            return hr;
        }

        HRESULT setRate(float rate)
        {
#ifdef DEBUG_MEDIAFOUNDATION
            qDebug() << "MediaStream::setRate" << rate;
#endif
            QMutexLocker locker(&m_mutex);
            HRESULT hr = validateOperation(OpSetRate);
            if (SUCCEEDED(hr)) {
                m_rate = rate;
                hr = queueAsyncOperation(OpSetRate);
            }
            return hr;
        }

        void supportedFormatsChanged()
        {
            QMutexLocker locker(&m_mutex);
            m_pixelFormats.clear();
            clearMediaTypes();
            if (!m_surface)
                return;
            const QList<QVideoFrame::PixelFormat> formats = m_surface->supportedPixelFormats();
            for (QVideoFrame::PixelFormat format : formats) {
                IMFMediaType *mediaType;
                if (FAILED(MFCreateMediaType(&mediaType))) {
                    qWarning("Failed to create mf media type!");
                    continue;
                }
                mediaType->SetUINT32(MF_MT_FIXED_SIZE_SAMPLES, TRUE);
                mediaType->SetUINT32(MF_MT_COMPRESSED, FALSE);
                mediaType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
                mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
                switch (format) {
                    case QVideoFrame::Format_ARGB32:
                    case QVideoFrame::Format_ARGB32_Premultiplied:
                        mediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_ARGB32);
                        break;
                    case QVideoFrame::Format_RGB32:
                        mediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
                        break;
                    case QVideoFrame::Format_BGR24: // MFVideoFormat_RGB24 has a BGR layout
                        mediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB24);
                        break;
                    case QVideoFrame::Format_RGB565:
                        mediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB565);
                        break;
                    case QVideoFrame::Format_RGB555:
                        mediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB555);
                        break;
                    case QVideoFrame::Format_AYUV444:
                    case QVideoFrame::Format_AYUV444_Premultiplied:
                        mediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_AYUV);
                        break;
                    case QVideoFrame::Format_YUV420P:
                        mediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_I420);
                        break;
                    case QVideoFrame::Format_UYVY:
                        mediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_UYVY);
                        break;
                    case QVideoFrame::Format_YV12:
                        mediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_YV12);
                        break;
                    case QVideoFrame::Format_NV12:
                        mediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
                        break;
                    default:
                        mediaType->Release();
                        continue;
                }
                // QAbstractVideoSurface::supportedPixelFormats() returns formats in descending
                // order of preference, while IMFMediaTypeHandler is supposed to return supported
                // formats in ascending order of preference. We need to reverse the list.
                m_pixelFormats.prepend(format);
                m_mediaTypes.prepend(mediaType);
            }
        }

        void present()
        {
            QMutexLocker locker(&m_mutex);
            if (!m_scheduledBuffer)
                return;
            QVideoFrame frame = QVideoFrame(
                        new MediaSampleVideoBuffer(m_scheduledBuffer, m_bytesPerLine),
                        m_surfaceFormat.frameSize(),
                        m_surfaceFormat.pixelFormat());
            frame.setStartTime(m_bufferStartTime * 0.1);
            frame.setEndTime((m_bufferStartTime + m_bufferDuration) * 0.1);
            m_surface->present(frame);
            m_scheduledBuffer->Release();
            m_scheduledBuffer = NULL;
            if (m_rate != 0)
                schedulePresentation(true);
        }

        void clearScheduledFrame()
        {
            QMutexLocker locker(&m_mutex);
            if (m_scheduledBuffer) {
                m_scheduledBuffer->Release();
                m_scheduledBuffer = NULL;
                schedulePresentation(true);
            }
        }

        enum
        {
            StartSurface = QEvent::User,
            StopSurface,
            FlushSurface,
            PresentSurface
        };

        class PresentEvent : public QEvent
        {
        public:
            PresentEvent(MFTIME targetTime)
                : QEvent(QEvent::Type(PresentSurface))
                , m_time(targetTime)
            {
            }

            MFTIME targetTime()
            {
                return m_time;
            }

        private:
            MFTIME m_time;
        };

    protected:
        void customEvent(QEvent *event)
        {
            QMutexLocker locker(&m_mutex);
            if (event->type() == StartSurface) {
                if (m_state == State_WaitForSurfaceStart) {
                    m_startResult = startSurface();
                    queueAsyncOperation(OpStart);
                }
            } else if (event->type() == StopSurface) {
                stopSurface();
            } else {
               QObject::customEvent(event);
            }
        }
        HRESULT m_startResult;

    private:
        HRESULT startSurface()
        {
            if (!m_surface->isFormatSupported(m_surfaceFormat))
                return S_FALSE;
            if (!m_surface->start(m_surfaceFormat))
                return S_FALSE;
            return S_OK;
        }

        void stopSurface()
        {
            m_surface->stop();
        }

        enum FlushState
        {
            DropSamples = 0,
            WriteSamples
        };

        // State enum: Defines the current state of the stream.
        enum State
        {
            State_TypeNotSet = 0,    // No media type is set
            State_Ready,             // Media type is set, Start has never been called.
            State_Prerolling,
            State_Started,
            State_Paused,
            State_Stopped,
            State_WaitForSurfaceStart,
            State_Finalized,
            State_Count = State_Finalized + 1    // Number of states
        };

        // StreamOperation: Defines various operations that can be performed on the stream.
        enum StreamOperation
        {
            OpSetMediaType = 0,
            OpStart,
            OpPreroll,
            OpRestart,
            OpPause,
            OpStop,
            OpSetRate,
            OpProcessSample,
            OpPlaceMarker,
            OpFinalize,

            Op_Count = OpFinalize + 1  // Number of operations
        };

        // AsyncOperation:
        // Used to queue asynchronous operations. When we call MFPutWorkItem, we use this
        // object for the callback state (pState). Then, when the callback is invoked,
        // we can use the object to determine which asynchronous operation to perform.
        class AsyncOperation : public IUnknown
        {
        public:
            AsyncOperation(StreamOperation op)
                :m_cRef(1), m_op(op)
            {
            }

            StreamOperation m_op;   // The operation to perform.

            //from IUnknown
            STDMETHODIMP QueryInterface(REFIID iid, void** ppv)
            {
                if (!ppv)
                    return E_POINTER;
                if (iid == IID_IUnknown) {
                    *ppv = static_cast<IUnknown*>(this);
                } else {
                    *ppv = NULL;
                    return E_NOINTERFACE;
                }
                AddRef();
                return S_OK;
            }
            STDMETHODIMP_(ULONG) AddRef()
            {
                return InterlockedIncrement(&m_cRef);
            }
            STDMETHODIMP_(ULONG) Release()
            {
                ULONG uCount = InterlockedDecrement(&m_cRef);
                if (uCount == 0)
                    delete this;
                // For thread safety, return a temporary variable.
                return uCount;
            }

        private:
            long    m_cRef;
            virtual ~AsyncOperation()
            {
                Q_ASSERT(m_cRef == 0);
            }
        };

        // ValidStateMatrix: Defines a look-up table that says which operations
        // are valid from which states.
        static BOOL ValidStateMatrix[State_Count][Op_Count];

        long m_cRef;
        QMutex m_mutex;

        IMFMediaType *m_currentMediaType;
        State m_state;
        IMFMediaSink *m_sink;
        IMFMediaEventQueue *m_eventQueue;
        DWORD m_workQueueId;
        AsyncCallback<MediaStream> m_workQueueCB;
        QList<IUnknown*> m_sampleQueue;
        IMFAsyncResult   *m_finalizeResult;         // Result object for Finalize operation.
        MFTIME           m_startTime;               // Presentation time when the clock started.

        bool m_shutdown;
        QList<IMFMediaType*> m_mediaTypes;
        QList<QVideoFrame::PixelFormat> m_pixelFormats;
        int m_currentFormatIndex;
        int m_bytesPerLine;
        QVideoSurfaceFormat m_surfaceFormat;
        QAbstractVideoSurface* m_surface;
        MFVideoRendererControl *m_rendererControl;

        void clearMediaTypes()
        {
            for (IMFMediaType* mediaType : qAsConst(m_mediaTypes))
                mediaType->Release();
            m_mediaTypes.clear();
        }

        int getMediaTypeIndex(IMFMediaType *mt)
        {
            GUID majorType;
            if (FAILED(mt->GetMajorType(&majorType)))
                return -1;
            if (majorType != MFMediaType_Video)
                return -1;

            GUID subType;
            if (FAILED(mt->GetGUID(MF_MT_SUBTYPE, &subType)))
                return -1;

            for (int index = 0; index < m_mediaTypes.size(); ++index) {
                GUID st;
                m_mediaTypes[index]->GetGUID(MF_MT_SUBTYPE, &st);
                if (st == subType)
                    return index;
            }
            return -1;
        }

        int getBytesPerLine(const QVideoSurfaceFormat &format)
        {
            switch (format.pixelFormat()) {
            // 32 bpp packed formats.
            case QVideoFrame::Format_RGB32:
            case QVideoFrame::Format_AYUV444:
                return format.frameWidth() * 4;
            // 24 bpp packed formats.
            case QVideoFrame::Format_RGB24:
            case QVideoFrame::Format_BGR24:
                return PAD_TO_DWORD(format.frameWidth() * 3);
            // 16 bpp packed formats.
            case QVideoFrame::Format_RGB565:
            case QVideoFrame::Format_RGB555:
            case QVideoFrame::Format_YUYV:
            case QVideoFrame::Format_UYVY:
                return PAD_TO_DWORD(format.frameWidth() * 2);
            // Planar formats.
            case QVideoFrame::Format_IMC1:
            case QVideoFrame::Format_IMC2:
            case QVideoFrame::Format_IMC3:
            case QVideoFrame::Format_IMC4:
            case QVideoFrame::Format_YV12:
            case QVideoFrame::Format_NV12:
            case QVideoFrame::Format_YUV420P:
                return PAD_TO_DWORD(format.frameWidth());
            default:
                return 0;
            }
        }

        // Callback for MFPutWorkItem.
        HRESULT onDispatchWorkItem(IMFAsyncResult* pAsyncResult)
        {
            QMutexLocker locker(&m_mutex);
            if (m_shutdown)
                return MF_E_SHUTDOWN;

            HRESULT hr = S_OK;
            IUnknown *pState = NULL;
            hr = pAsyncResult->GetState(&pState);
            if (SUCCEEDED(hr)) {
                // The state object is an AsncOperation object.
                AsyncOperation *pOp = (AsyncOperation*)pState;
                StreamOperation op = pOp->m_op;
                switch (op)    {
                case OpStart:
                    endPreroll(S_FALSE);
                    if (m_state == State_WaitForSurfaceStart) {
                        hr = m_startResult;
                        m_state = State_Started;
                    } else if (!m_surface->isActive()) {
                        if (thread() == QThread::currentThread()) {
                            hr = startSurface();
                        }
                        else {
                            m_state = State_WaitForSurfaceStart;
                            QCoreApplication::postEvent(m_rendererControl, new QChildEvent(QEvent::Type(StartSurface), this));
                            break;
                        }
                    }

                    if (m_state == State_Started)
                        schedulePresentation(true);
                case OpRestart:
                    endPreroll(S_FALSE);
                    if (SUCCEEDED(hr)) {
                        // Send MEStreamSinkStarted.
                        hr = queueEvent(MEStreamSinkStarted, GUID_NULL, hr, NULL);
                        // Kick things off by requesting samples...
                        schedulePresentation(true);
                        // There might be samples queue from earlier (ie, while paused).
                        if (SUCCEEDED(hr))
                            hr = processSamplesFromQueue(WriteSamples);
                    }
                    break;
                case OpPreroll:
                    beginPreroll();
                    break;
                case OpStop:
                    // Drop samples from queue.
                    hr = processSamplesFromQueue(DropSamples);
                    clearBufferCache();
                    // Send the event even if the previous call failed.
                    hr = queueEvent(MEStreamSinkStopped, GUID_NULL, hr, NULL);
                    if (m_surface->isActive()) {
                        if (thread() == QThread::currentThread()) {
                            stopSurface();
                        }
                        else {
                            QCoreApplication::postEvent(m_rendererControl, new QChildEvent(QEvent::Type(StopSurface), this));
                        }
                    }
                    break;
                case OpPause:
                    hr = queueEvent(MEStreamSinkPaused, GUID_NULL, hr, NULL);
                    break;
                case OpSetRate:
                    hr = queueEvent(MEStreamSinkRateChanged, GUID_NULL, S_OK, NULL);
                    break;
                case OpProcessSample:
                case OpPlaceMarker:
                    hr = dispatchProcessSample(pOp);
                    break;
                case OpFinalize:
                    endPreroll(S_FALSE);
                    hr = dispatchFinalize(pOp);
                    break;
                }
            }

            if (pState)
                pState->Release();
            return hr;
        }


        HRESULT queueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT* pvValue)
        {
            HRESULT hr = S_OK;
            if (m_shutdown)
                hr = MF_E_SHUTDOWN;
            if (SUCCEEDED(hr))
                hr = m_eventQueue->QueueEventParamVar(met, guidExtendedType, hrStatus, pvValue);
            return hr;
        }

        HRESULT validateOperation(StreamOperation op)
        {
            Q_ASSERT(!m_shutdown);
            if (ValidStateMatrix[m_state][op])
                return S_OK;
            else
                return MF_E_INVALIDREQUEST;
        }

        HRESULT queueAsyncOperation(StreamOperation op)
        {
            HRESULT hr = S_OK;
            AsyncOperation *asyncOp = new AsyncOperation(op);
            if (asyncOp == NULL)
                hr = E_OUTOFMEMORY;

            if (SUCCEEDED(hr))
                hr = MFPutWorkItem(m_workQueueId, &m_workQueueCB, asyncOp);

            if (asyncOp)
                asyncOp->Release();

            return hr;
        }

        HRESULT processSamplesFromQueue(FlushState bFlushData)
        {
            HRESULT hr = S_OK;
            QList<IUnknown*>::Iterator pos = m_sampleQueue.begin();
            // Enumerate all of the samples/markers in the queue.
            while (pos != m_sampleQueue.end()) {
                IUnknown *pUnk = NULL;
                IMarker  *pMarker = NULL;
                IMFSample *pSample = NULL;
                pUnk = *pos;
                // Figure out if this is a marker or a sample.
                if (SUCCEEDED(hr))    {
                    hr = pUnk->QueryInterface(__uuidof(IMarker), (void**)&pMarker);
                    if (hr == E_NOINTERFACE)
                        hr = pUnk->QueryInterface(IID_IMFSample, (void**)&pSample);
                }

                // Now handle the sample/marker appropriately.
                if (SUCCEEDED(hr)) {
                    if (pMarker) {
                        hr = sendMarkerEvent(pMarker, bFlushData);
                    } else {
                        Q_ASSERT(pSample != NULL);    // Not a marker, must be a sample
                        if (bFlushData == WriteSamples)
                            hr = processSampleData(pSample);
                    }
                }
                if (pMarker)
                    pMarker->Release();
                if (pSample)
                    pSample->Release();

                if (FAILED(hr))
                    break;

                pos++;
            }

            clearSampleQueue();
            return hr;
        }

        void beginPreroll()
        {
            if (m_prerolling)
                return;
            m_prerolling = true;
            clearSampleQueue();
            clearBufferCache();
            queueEvent(MEStreamSinkRequestSample, GUID_NULL, S_OK, NULL);
        }

        void endPreroll(HRESULT hrStatus)
        {
            if (!m_prerolling)
                return;
            m_prerolling = false;
            queueEvent(MEStreamSinkPrerolled, GUID_NULL, hrStatus, NULL);
        }
        MFTIME m_prerollTargetTime;
        bool m_prerolling;

        void clearSampleQueue() {
            for (IUnknown* sample : qAsConst(m_sampleQueue))
                sample->Release();
            m_sampleQueue.clear();
        }

        HRESULT sendMarkerEvent(IMarker *pMarker, FlushState FlushState)
        {
            HRESULT hr = S_OK;
            HRESULT hrStatus = S_OK;  // Status code for marker event.
            if (FlushState == DropSamples)
                hrStatus = E_ABORT;

            PROPVARIANT var;
            PropVariantInit(&var);

            // Get the context data.
            hr = pMarker->GetContext(&var);

            if (SUCCEEDED(hr))
                hr = queueEvent(MEStreamSinkMarker, GUID_NULL, hrStatus, &var);

            PropVariantClear(&var);
            return hr;
        }

        HRESULT dispatchProcessSample(AsyncOperation* pOp)
        {
            HRESULT hr = S_OK;
            Q_ASSERT(pOp != NULL);
            Q_UNUSED(pOp)
            hr = processSamplesFromQueue(WriteSamples);
            // We are in the middle of an asynchronous operation, so if something failed, send an error.
            if (FAILED(hr))
                hr = queueEvent(MEError, GUID_NULL, hr, NULL);

            return hr;
        }

        HRESULT dispatchFinalize(AsyncOperation*)
        {
            HRESULT hr = S_OK;
            // Write any samples left in the queue...
            hr = processSamplesFromQueue(WriteSamples);

            // Set the async status and invoke the callback.
            m_finalizeResult->SetStatus(hr);
            hr = MFInvokeCallback(m_finalizeResult);
            return hr;
        }

        HRESULT processSampleData(IMFSample *pSample)
        {
            m_sampleRequested = false;

            LONGLONG time, duration = -1;
            HRESULT hr = pSample->GetSampleTime(&time);
            if (SUCCEEDED(hr))
               pSample->GetSampleDuration(&duration);

            if (m_prerolling) {
                if (SUCCEEDED(hr) && ((time - m_prerollTargetTime) * m_rate) >= 0) {
                    IMFMediaBuffer *pBuffer = NULL;
                    hr = pSample->ConvertToContiguousBuffer(&pBuffer);
                    if (SUCCEEDED(hr)) {
                        SampleBuffer sb;
                        sb.m_buffer = pBuffer;
                        sb.m_time = time;
                        sb.m_duration = duration;
                        m_bufferCache.push_back(sb);
                        endPreroll(S_OK);
                    }
                } else {
                    queueEvent(MEStreamSinkRequestSample, GUID_NULL, S_OK, NULL);
                }
            } else {
                bool requestSample = true;
                // If the time stamp is too early, just discard this sample.
                if (SUCCEEDED(hr) && ((time - m_startTime) * m_rate) >= 0) {
                    IMFMediaBuffer *pBuffer = NULL;
                    hr = pSample->ConvertToContiguousBuffer(&pBuffer);
                    if (SUCCEEDED(hr)) {
                        SampleBuffer sb;
                        sb.m_buffer = pBuffer;
                        sb.m_time = time;
                        sb.m_duration = duration;
                        m_bufferCache.push_back(sb);
                    }
                    if (m_rate == 0)
                        requestSample = false;
                }
                schedulePresentation(requestSample);
            }
            return hr;
        }

        class SampleBuffer
        {
        public:
            IMFMediaBuffer *m_buffer;
            LONGLONG m_time;
            LONGLONG m_duration;
        };
        QList<SampleBuffer> m_bufferCache;
        static const int BUFFER_CACHE_SIZE = 2;

        void clearBufferCache()
        {
            for (SampleBuffer sb : qAsConst(m_bufferCache))
                sb.m_buffer->Release();
            m_bufferCache.clear();

            if (m_scheduledBuffer) {
                m_scheduledBuffer->Release();
                m_scheduledBuffer = NULL;
            }
        }

        void schedulePresentation(bool requestSample)
        {
            if (m_state == State_Paused || m_state == State_Prerolling)
                return;
            if (!m_scheduledBuffer) {
                //get time from presentation time
                MFTIME currentTime = m_startTime, sysTime;
                bool timeOK = true;
                if (m_rate != 0) {
                    if (FAILED(m_presentationClock->GetCorrelatedTime(0, &currentTime, &sysTime)))
                        timeOK = false;
                }
                while (!m_bufferCache.isEmpty()) {
                    SampleBuffer sb = m_bufferCache.takeFirst();
                    if (timeOK && ((sb.m_time - currentTime) * m_rate) < 0) {
                        sb.m_buffer->Release();
#ifdef DEBUG_MEDIAFOUNDATION
                        qDebug() << "currentPresentTime =" << float(currentTime / 10000) * 0.001f << " and sampleTime is" << float(sb.m_time / 10000) * 0.001f;
#endif
                        continue;
                    }
                    m_scheduledBuffer = sb.m_buffer;
                    m_bufferStartTime = sb.m_time;
                    m_bufferDuration = sb.m_duration;
                    QCoreApplication::postEvent(m_rendererControl, new PresentEvent(sb.m_time));
                    if (m_rate == 0)
                        queueEvent(MEStreamSinkScrubSampleComplete, GUID_NULL, S_OK, NULL);
                    break;
                }
            }
            if (requestSample && !m_sampleRequested && m_bufferCache.size() < BUFFER_CACHE_SIZE) {
                m_sampleRequested = true;
                queueEvent(MEStreamSinkRequestSample, GUID_NULL, S_OK, NULL);
            }
        }
        IMFMediaBuffer *m_scheduledBuffer;
        MFTIME m_bufferStartTime;
        MFTIME m_bufferDuration;
        IMFPresentationClock *m_presentationClock;
        bool m_sampleRequested;
        float m_rate;
    };

    BOOL MediaStream::ValidStateMatrix[MediaStream::State_Count][MediaStream::Op_Count] =
    {
        // States:    Operations:
        //                         SetType  Start   Preroll, Restart   Pause     Stop        SetRate  Sample    Marker    Finalize
        /* NotSet */                TRUE,   FALSE,  FALSE,    FALSE,    FALSE,    FALSE,    FALSE,      FALSE,    FALSE,    FALSE,

        /* Ready */                 TRUE,   TRUE,   TRUE,       FALSE,    TRUE,     TRUE,     TRUE,      FALSE,    TRUE,     TRUE,

        /* Prerolling */            TRUE,   TRUE,   FALSE,    FALSE,     TRUE,    TRUE,      TRUE,      TRUE,      TRUE,     TRUE,

        /* Start */                 FALSE,  TRUE,   TRUE,      FALSE,    TRUE,     TRUE,     TRUE,       TRUE,     TRUE,     TRUE,

        /* Pause */                 FALSE,  TRUE,   TRUE,      TRUE,     TRUE,     TRUE,     TRUE,       TRUE,     TRUE,     TRUE,

        /* Stop */                  FALSE,  TRUE,   TRUE,       FALSE,    FALSE,    TRUE,     TRUE,      FALSE,    TRUE,     TRUE,

        /*WaitForSurfaceStart*/     FALSE,  FALSE,  TRUE,        FALSE,    FALSE,    TRUE,     TRUE,      FALSE,    FALSE,    TRUE,

        /* Final */                 FALSE,  FALSE,  FALSE,      FALSE,    FALSE,    FALSE,    FALSE,      FALSE,    FALSE,    FALSE

        // Note about states:
        // 1. OnClockRestart should only be called from paused state.
        // 2. While paused, the sink accepts samples but does not process them.
    };

    class MediaSink : public IMFFinalizableMediaSink,
                      public IMFClockStateSink,
                      public IMFMediaSinkPreroll,
                      public IMFGetService,
                      public IMFRateSupport
    {
    public:
        MediaSink(MFVideoRendererControl *rendererControl)
            : m_cRef(1)
            , m_shutdown(false)
            , m_presentationClock(0)
            , m_playRate(1)
        {
            m_stream = new MediaStream(this, rendererControl);
        }

        ~MediaSink()
        {
            Q_ASSERT(m_shutdown);
        }

        void setSurface(QAbstractVideoSurface *surface)
        {
            QMutexLocker locker(&m_mutex);
            if (m_shutdown)
                return;
            m_stream->setSurface(surface);
        }

        void supportedFormatsChanged()
        {
            QMutexLocker locker(&m_mutex);
            if (m_shutdown)
                return;
            m_stream->supportedFormatsChanged();
        }

        void present()
        {
            QMutexLocker locker(&m_mutex);
            if (m_shutdown)
                return;
            m_stream->present();
        }

        void clearScheduledFrame()
        {
            QMutexLocker locker(&m_mutex);
            if (m_shutdown)
                return;
            m_stream->clearScheduledFrame();
        }

        MFTIME getTime()
        {
            QMutexLocker locker(&m_mutex);
            if (!m_presentationClock)
                return 0;
            MFTIME time, sysTime;
            m_presentationClock->GetCorrelatedTime(0, &time, &sysTime);
            return time;
        }

        float getPlayRate()
        {
            QMutexLocker locker(&m_mutex);
            return m_playRate;
        }

        //from IUnknown
        STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject)
        {
            if (!ppvObject)
                return E_POINTER;
            if (riid == IID_IMFMediaSink) {
                *ppvObject = static_cast<IMFMediaSink*>(this);
            } else if (riid == IID_IMFGetService) {
                *ppvObject = static_cast<IMFGetService*>(this);
            } else if (riid == IID_IMFMediaSinkPreroll) {
                *ppvObject = static_cast<IMFMediaSinkPreroll*>(this);
            } else if (riid == IID_IMFClockStateSink) {
                *ppvObject = static_cast<IMFClockStateSink*>(this);
            } else if (riid == IID_IMFRateSupport) {
                *ppvObject = static_cast<IMFRateSupport*>(this);
            } else if (riid == IID_IUnknown) {
                *ppvObject = static_cast<IUnknown*>(static_cast<IMFFinalizableMediaSink*>(this));
            } else {
                *ppvObject =  NULL;
                return E_NOINTERFACE;
            }
            AddRef();
            return S_OK;
        }

        STDMETHODIMP_(ULONG) AddRef(void)
        {
            return InterlockedIncrement(&m_cRef);
        }

        STDMETHODIMP_(ULONG) Release(void)
        {
            LONG cRef = InterlockedDecrement(&m_cRef);
            if (cRef == 0)
                delete this;
            // For thread safety, return a temporary variable.
            return cRef;
        }

        // IMFGetService methods
        STDMETHODIMP GetService(const GUID &guidService,
                                const IID &riid,
                                LPVOID *ppvObject)
        {
            if (!ppvObject)
                return E_POINTER;

            if (guidService != MF_RATE_CONTROL_SERVICE)
                return MF_E_UNSUPPORTED_SERVICE;

            return QueryInterface(riid, ppvObject);
        }

        //IMFMediaSinkPreroll
        STDMETHODIMP NotifyPreroll(MFTIME hnsUpcomingStartTime)
        {
            QMutexLocker locker(&m_mutex);
            if (m_shutdown)
                return MF_E_SHUTDOWN;
            return m_stream->startPreroll(hnsUpcomingStartTime);
        }

        //from IMFFinalizableMediaSink
        STDMETHODIMP BeginFinalize(IMFAsyncCallback *pCallback, IUnknown *punkState)
        {
            QMutexLocker locker(&m_mutex);
            if (m_shutdown)
                return MF_E_SHUTDOWN;
            return m_stream->finalize(pCallback, punkState);
        }

        STDMETHODIMP EndFinalize(IMFAsyncResult *pResult)
        {
            HRESULT hr = S_OK;
            // Return the status code from the async result.
            if (pResult == NULL)
                hr = E_INVALIDARG;
            else
                hr = pResult->GetStatus();
            return hr;
        }

        //from IMFMediaSink
        STDMETHODIMP GetCharacteristics(
            DWORD *pdwCharacteristics)
        {
            QMutexLocker locker(&m_mutex);
            if (m_shutdown)
                return MF_E_SHUTDOWN;
            *pdwCharacteristics = MEDIASINK_FIXED_STREAMS | MEDIASINK_CAN_PREROLL;
            return S_OK;
        }

        STDMETHODIMP AddStreamSink(
            DWORD,
            IMFMediaType *,
            IMFStreamSink **)
        {
            QMutexLocker locker(&m_mutex);
            return m_shutdown ? MF_E_SHUTDOWN : MF_E_STREAMSINKS_FIXED;
        }

        STDMETHODIMP RemoveStreamSink(
            DWORD)
        {
            QMutexLocker locker(&m_mutex);
            return m_shutdown ? MF_E_SHUTDOWN : MF_E_STREAMSINKS_FIXED;
        }

        STDMETHODIMP GetStreamSinkCount(
            DWORD *pcStreamSinkCount)
        {
            QMutexLocker locker(&m_mutex);
            if (m_shutdown)
                return MF_E_SHUTDOWN;
            *pcStreamSinkCount = 1;
            return S_OK;
        }

        STDMETHODIMP GetStreamSinkByIndex(
            DWORD dwIndex,
            IMFStreamSink **ppStreamSink)
        {
            QMutexLocker locker(&m_mutex);
            if (m_shutdown)
                return MF_E_SHUTDOWN;

            if (dwIndex != 0)
                return MF_E_INVALIDINDEX;

            *ppStreamSink = m_stream;
            m_stream->AddRef();
            return S_OK;
        }

        STDMETHODIMP GetStreamSinkById(
            DWORD dwStreamSinkIdentifier,
            IMFStreamSink **ppStreamSink)
        {
            if (ppStreamSink == NULL)
                return E_INVALIDARG;
            if (dwStreamSinkIdentifier != MediaStream::DEFAULT_MEDIA_STREAM_ID)
                return MF_E_INVALIDSTREAMNUMBER;

            QMutexLocker locker(&m_mutex);
            if (m_shutdown)
                return MF_E_SHUTDOWN;

            *ppStreamSink = m_stream;
            m_stream->AddRef();
            return S_OK;
        }

        STDMETHODIMP SetPresentationClock(
            IMFPresentationClock *pPresentationClock)
        {
            QMutexLocker locker(&m_mutex);
            if (m_shutdown)
                return MF_E_SHUTDOWN;

            if (m_presentationClock) {
                m_presentationClock->RemoveClockStateSink(this);
                m_presentationClock->Release();
            }
            m_presentationClock = pPresentationClock;
            if (m_presentationClock) {
                m_presentationClock->AddRef();
                m_presentationClock->AddClockStateSink(this);
            }
            m_stream->setClock(m_presentationClock);
            return S_OK;
        }

        STDMETHODIMP GetPresentationClock(
            IMFPresentationClock **ppPresentationClock)
        {
            QMutexLocker locker(&m_mutex);
            if (m_shutdown)
                return MF_E_SHUTDOWN;
            *ppPresentationClock = m_presentationClock;
            if (m_presentationClock) {
                m_presentationClock->AddRef();
                return S_OK;
            }
            return MF_E_NO_CLOCK;
        }

        STDMETHODIMP Shutdown(void)
        {
            QMutexLocker locker(&m_mutex);
            if (m_shutdown)
                return MF_E_SHUTDOWN;

            m_stream->shutdown();
            if (m_presentationClock) {
                m_presentationClock->Release();
                m_presentationClock = NULL;
            }
            m_stream->Release();
            m_stream = NULL;
            m_shutdown = true;
            return S_OK;
        }

        // IMFClockStateSink methods
        STDMETHODIMP OnClockStart(MFTIME, LONGLONG llClockStartOffset)
        {
            QMutexLocker locker(&m_mutex);
            if (m_shutdown)
                return MF_E_SHUTDOWN;
            return m_stream->start(llClockStartOffset);
        }

        STDMETHODIMP OnClockStop(MFTIME)
        {
            QMutexLocker locker(&m_mutex);
            if (m_shutdown)
                return MF_E_SHUTDOWN;
            return m_stream->stop();
        }

        STDMETHODIMP OnClockPause(MFTIME)
        {
            QMutexLocker locker(&m_mutex);
            if (m_shutdown)
                return MF_E_SHUTDOWN;
            return m_stream->pause();
        }

        STDMETHODIMP OnClockRestart(MFTIME)
        {
            QMutexLocker locker(&m_mutex);
            if (m_shutdown)
                return MF_E_SHUTDOWN;
            return m_stream->restart();
        }

        STDMETHODIMP OnClockSetRate(MFTIME, float flRate)
        {
            QMutexLocker locker(&m_mutex);
            if (m_shutdown)
                return MF_E_SHUTDOWN;
            m_playRate = flRate;
            return m_stream->setRate(flRate);
        }

        // IMFRateSupport methods
        STDMETHODIMP GetFastestRate(MFRATE_DIRECTION eDirection,
                                    BOOL fThin,
                                    float *pflRate)
        {
            if (!pflRate)
                return E_POINTER;

            *pflRate = (fThin ? 8.f : 2.0f) * (eDirection == MFRATE_FORWARD ? 1 : -1) ;

            return S_OK;
        }

        STDMETHODIMP GetSlowestRate(MFRATE_DIRECTION eDirection,
                                    BOOL fThin,
                                    float *pflRate)
        {
            Q_UNUSED(eDirection);
            Q_UNUSED(fThin);

            if (!pflRate)
                return E_POINTER;

            // we support any rate
            *pflRate = 0.f;

            return S_OK;
        }

        STDMETHODIMP IsRateSupported(BOOL fThin,
                                     float flRate,
                                     float *pflNearestSupportedRate)
        {
            HRESULT hr = S_OK;

            if (!qFuzzyIsNull(flRate)) {
                MFRATE_DIRECTION direction = flRate > 0.f ? MFRATE_FORWARD
                                                          : MFRATE_REVERSE;

                float fastestRate = 0.f;
                float slowestRate = 0.f;
                GetFastestRate(direction, fThin, &fastestRate);
                GetSlowestRate(direction, fThin, &slowestRate);

                if (direction == MFRATE_REVERSE)
                    qSwap(fastestRate, slowestRate);

                if (flRate < slowestRate || flRate > fastestRate) {
                    hr = MF_E_UNSUPPORTED_RATE;
                    if (pflNearestSupportedRate) {
                        *pflNearestSupportedRate = qBound(slowestRate,
                                                          flRate,
                                                          fastestRate);
                    }
                }
            } else if (pflNearestSupportedRate) {
                *pflNearestSupportedRate = flRate;
            }

            return hr;
        }

    private:
        long    m_cRef;
        QMutex  m_mutex;
        bool    m_shutdown;
        IMFPresentationClock *m_presentationClock;
        MediaStream *m_stream;
        float   m_playRate;
    };

    class VideoRendererActivate : public IMFActivate
    {
    public:
        VideoRendererActivate(MFVideoRendererControl *rendererControl)
            : m_cRef(1)
            , m_sink(0)
            , m_rendererControl(rendererControl)
            , m_attributes(0)
            , m_surface(0)
        {
            MFCreateAttributes(&m_attributes, 0);
            m_sink = new MediaSink(rendererControl);
        }

        ~VideoRendererActivate()
        {
            m_attributes->Release();
        }

        //from IUnknown
        STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject)
        {
            if (!ppvObject)
                return E_POINTER;
            if (riid == IID_IMFActivate) {
                *ppvObject = static_cast<IMFActivate*>(this);
            } else if (riid == IID_IMFAttributes) {
                *ppvObject = static_cast<IMFAttributes*>(this);
            } else if (riid == IID_IUnknown) {
                *ppvObject = static_cast<IUnknown*>(static_cast<IMFActivate*>(this));
            } else {
                *ppvObject =  NULL;
                return E_NOINTERFACE;
            }
            AddRef();
            return S_OK;
        }

        STDMETHODIMP_(ULONG) AddRef(void)
        {
            return InterlockedIncrement(&m_cRef);
        }

        STDMETHODIMP_(ULONG) Release(void)
        {
            LONG cRef = InterlockedDecrement(&m_cRef);
            if (cRef == 0)
                delete this;
            // For thread safety, return a temporary variable.
            return cRef;
        }

        //from IMFActivate
        STDMETHODIMP ActivateObject(REFIID riid, void **ppv)
        {
            if (!ppv)
                return E_INVALIDARG;
            QMutexLocker locker(&m_mutex);
            if (!m_sink) {
                m_sink = new MediaSink(m_rendererControl);
                if (m_surface)
                    m_sink->setSurface(m_surface);
            }
            return m_sink->QueryInterface(riid, ppv);
        }

        STDMETHODIMP ShutdownObject(void)
        {
            QMutexLocker locker(&m_mutex);
            HRESULT hr = S_OK;
            if (m_sink) {
                hr = m_sink->Shutdown();
                m_sink->Release();
                m_sink = NULL;
            }
            return hr;
        }

        STDMETHODIMP DetachObject(void)
        {
            QMutexLocker locker(&m_mutex);
            if (m_sink) {
                m_sink->Release();
                m_sink = NULL;
            }
            return S_OK;
        }

        //from IMFAttributes
        STDMETHODIMP GetItem(
            REFGUID guidKey,
            PROPVARIANT *pValue)
        {
            return m_attributes->GetItem(guidKey, pValue);
        }

        STDMETHODIMP GetItemType(
            REFGUID guidKey,
            MF_ATTRIBUTE_TYPE *pType)
        {
            return m_attributes->GetItemType(guidKey, pType);
        }

        STDMETHODIMP CompareItem(
            REFGUID guidKey,
            REFPROPVARIANT Value,
            BOOL *pbResult)
        {
            return m_attributes->CompareItem(guidKey, Value, pbResult);
        }

        STDMETHODIMP Compare(
            IMFAttributes *pTheirs,
            MF_ATTRIBUTES_MATCH_TYPE MatchType,
            BOOL *pbResult)
        {
            return m_attributes->Compare(pTheirs, MatchType, pbResult);
        }

        STDMETHODIMP GetUINT32(
            REFGUID guidKey,
            UINT32 *punValue)
        {
            return m_attributes->GetUINT32(guidKey, punValue);
        }

        STDMETHODIMP GetUINT64(
            REFGUID guidKey,
            UINT64 *punValue)
        {
            return m_attributes->GetUINT64(guidKey, punValue);
        }

        STDMETHODIMP GetDouble(
            REFGUID guidKey,
            double *pfValue)
        {
            return m_attributes->GetDouble(guidKey, pfValue);
        }

        STDMETHODIMP GetGUID(
            REFGUID guidKey,
            GUID *pguidValue)
        {
            return m_attributes->GetGUID(guidKey, pguidValue);
        }

        STDMETHODIMP GetStringLength(
            REFGUID guidKey,
            UINT32 *pcchLength)
        {
            return m_attributes->GetStringLength(guidKey, pcchLength);
        }

        STDMETHODIMP GetString(
            REFGUID guidKey,
            LPWSTR pwszValue,
            UINT32 cchBufSize,
            UINT32 *pcchLength)
        {
            return m_attributes->GetString(guidKey, pwszValue, cchBufSize, pcchLength);
        }

        STDMETHODIMP GetAllocatedString(
            REFGUID guidKey,
            LPWSTR *ppwszValue,
            UINT32 *pcchLength)
        {
            return m_attributes->GetAllocatedString(guidKey, ppwszValue, pcchLength);
        }

        STDMETHODIMP GetBlobSize(
            REFGUID guidKey,
            UINT32 *pcbBlobSize)
        {
            return m_attributes->GetBlobSize(guidKey, pcbBlobSize);
        }

        STDMETHODIMP GetBlob(
            REFGUID guidKey,
            UINT8 *pBuf,
            UINT32 cbBufSize,
            UINT32 *pcbBlobSize)
        {
            return m_attributes->GetBlob(guidKey, pBuf, cbBufSize, pcbBlobSize);
        }

        STDMETHODIMP GetAllocatedBlob(
            REFGUID guidKey,
            UINT8 **ppBuf,
            UINT32 *pcbSize)
        {
            return m_attributes->GetAllocatedBlob(guidKey, ppBuf, pcbSize);
        }

        STDMETHODIMP GetUnknown(
            REFGUID guidKey,
            REFIID riid,
            LPVOID *ppv)
        {
            return m_attributes->GetUnknown(guidKey, riid, ppv);
        }

        STDMETHODIMP SetItem(
            REFGUID guidKey,
            REFPROPVARIANT Value)
        {
            return m_attributes->SetItem(guidKey, Value);
        }

        STDMETHODIMP DeleteItem(
            REFGUID guidKey)
        {
            return m_attributes->DeleteItem(guidKey);
        }

        STDMETHODIMP DeleteAllItems(void)
        {
            return m_attributes->DeleteAllItems();
        }

        STDMETHODIMP SetUINT32(
            REFGUID guidKey,
            UINT32 unValue)
        {
            return m_attributes->SetUINT32(guidKey, unValue);
        }

        STDMETHODIMP SetUINT64(
            REFGUID guidKey,
            UINT64 unValue)
        {
            return m_attributes->SetUINT64(guidKey, unValue);
        }

        STDMETHODIMP SetDouble(
            REFGUID guidKey,
            double fValue)
         {
            return m_attributes->SetDouble(guidKey, fValue);
        }

        STDMETHODIMP SetGUID(
            REFGUID guidKey,
            REFGUID guidValue)
        {
            return m_attributes->SetGUID(guidKey, guidValue);
        }

        STDMETHODIMP SetString(
            REFGUID guidKey,
            LPCWSTR wszValue)
        {
            return m_attributes->SetString(guidKey, wszValue);
        }

        STDMETHODIMP SetBlob(
            REFGUID guidKey,
            const UINT8 *pBuf,
            UINT32 cbBufSize)
        {
            return m_attributes->SetBlob(guidKey, pBuf, cbBufSize);
        }

        STDMETHODIMP SetUnknown(
            REFGUID guidKey,
            IUnknown *pUnknown)
        {
            return m_attributes->SetUnknown(guidKey, pUnknown);
        }

        STDMETHODIMP LockStore(void)
        {
            return m_attributes->LockStore();
        }

        STDMETHODIMP UnlockStore(void)
        {
            return m_attributes->UnlockStore();
        }

        STDMETHODIMP GetCount(
             UINT32 *pcItems)
        {
            return m_attributes->GetCount(pcItems);
        }

        STDMETHODIMP GetItemByIndex(
            UINT32 unIndex,
            GUID *pguidKey,
            PROPVARIANT *pValue)
        {
            return m_attributes->GetItemByIndex(unIndex, pguidKey, pValue);
        }

        STDMETHODIMP CopyAllItems(
           IMFAttributes *pDest)
        {
            return m_attributes->CopyAllItems(pDest);
        }

        /////////////////////////////////
        void setSurface(QAbstractVideoSurface *surface)
        {
            QMutexLocker locker(&m_mutex);
            if (m_surface == surface)
                return;

            m_surface = surface;

            if (!m_sink)
                return;
            m_sink->setSurface(m_surface);
        }

        void supportedFormatsChanged()
        {
            QMutexLocker locker(&m_mutex);
            if (!m_sink)
                return;
            m_sink->supportedFormatsChanged();
        }

        void present()
        {
            QMutexLocker locker(&m_mutex);
            if (!m_sink)
                return;
            m_sink->present();
        }

        void clearScheduledFrame()
        {
            QMutexLocker locker(&m_mutex);
            if (!m_sink)
                return;
            m_sink->clearScheduledFrame();
        }

        MFTIME getTime()
        {
            if (m_sink)
                return m_sink->getTime();
            return 0;
        }

        float getPlayRate()
        {
            if (m_sink)
                return m_sink->getPlayRate();
            return 1;
        }

    private:
        long    m_cRef;
        bool    m_shutdown;
        MediaSink *m_sink;
        MFVideoRendererControl *m_rendererControl;
        IMFAttributes *m_attributes;
        QAbstractVideoSurface *m_surface;
        QMutex m_mutex;
    };
}


class EVRCustomPresenterActivate : public MFAbstractActivate
{
public:
    EVRCustomPresenterActivate();
    ~EVRCustomPresenterActivate()
    { }

    STDMETHODIMP ActivateObject(REFIID riid, void **ppv);
    STDMETHODIMP ShutdownObject();
    STDMETHODIMP DetachObject();

    void setSurface(QAbstractVideoSurface *surface);

private:
    EVRCustomPresenter *m_presenter;
    QAbstractVideoSurface *m_surface;
    QMutex m_mutex;
};


MFVideoRendererControl::MFVideoRendererControl(QObject *parent)
    : QVideoRendererControl(parent)
    , m_surface(0)
    , m_currentActivate(0)
    , m_callback(0)
    , m_presenterActivate(0)
{
}

MFVideoRendererControl::~MFVideoRendererControl()
{
    clear();
}

void MFVideoRendererControl::clear()
{
    if (m_surface)
        m_surface->stop();

    if (m_presenterActivate) {
        m_presenterActivate->ShutdownObject();
        m_presenterActivate->Release();
        m_presenterActivate = NULL;
    }

    if (m_currentActivate) {
        m_currentActivate->ShutdownObject();
        m_currentActivate->Release();
    }
    m_currentActivate = NULL;
}

void MFVideoRendererControl::releaseActivate()
{
    clear();
}

QAbstractVideoSurface *MFVideoRendererControl::surface() const
{
    return m_surface;
}

void MFVideoRendererControl::setSurface(QAbstractVideoSurface *surface)
{
    if (m_surface)
        disconnect(m_surface, SIGNAL(supportedFormatsChanged()), this, SLOT(supportedFormatsChanged()));
    m_surface = surface;

    if (m_surface) {
        connect(m_surface, SIGNAL(supportedFormatsChanged()), this, SLOT(supportedFormatsChanged()));
    }

    if (m_presenterActivate)
        m_presenterActivate->setSurface(m_surface);
    else if (m_currentActivate)
        static_cast<VideoRendererActivate*>(m_currentActivate)->setSurface(m_surface);
}

void MFVideoRendererControl::customEvent(QEvent *event)
{
    if (m_presenterActivate)
        return;

    if (!m_currentActivate)
        return;

    if (event->type() == MediaStream::PresentSurface) {
        MFTIME targetTime = static_cast<MediaStream::PresentEvent*>(event)->targetTime();
        MFTIME currentTime = static_cast<VideoRendererActivate*>(m_currentActivate)->getTime();
        float playRate = static_cast<VideoRendererActivate*>(m_currentActivate)->getPlayRate();
        if (!qFuzzyIsNull(playRate) && targetTime != currentTime) {
            // If the scheduled frame is too late, skip it
            const int interval = ((targetTime - currentTime) / 10000) / playRate;
            if (interval < 0)
                static_cast<VideoRendererActivate*>(m_currentActivate)->clearScheduledFrame();
            else
                QTimer::singleShot(interval, this, SLOT(present()));
        } else {
            present();
        }
        return;
    }
    if (event->type() >= MediaStream::StartSurface) {
        QChildEvent *childEvent = static_cast<QChildEvent*>(event);
        static_cast<MediaStream*>(childEvent->child())->customEvent(event);
    } else {
        QObject::customEvent(event);
    }
}

void MFVideoRendererControl::supportedFormatsChanged()
{
    if (m_presenterActivate)
        return;

    if (m_currentActivate)
        static_cast<VideoRendererActivate*>(m_currentActivate)->supportedFormatsChanged();
}

void MFVideoRendererControl::present()
{
    if (m_presenterActivate)
        return;

    if (m_currentActivate)
        static_cast<VideoRendererActivate*>(m_currentActivate)->present();
}

IMFActivate* MFVideoRendererControl::createActivate()
{
    Q_ASSERT(m_surface);

    clear();

    // Create the EVR media sink, but replace the presenter with our own
    if (SUCCEEDED(MFCreateVideoRendererActivate(::GetShellWindow(), &m_currentActivate))) {
        m_presenterActivate = new EVRCustomPresenterActivate;
        m_currentActivate->SetUnknown(MF_ACTIVATE_CUSTOM_VIDEO_PRESENTER_ACTIVATE, m_presenterActivate);
    } else {
        m_currentActivate = new VideoRendererActivate(this);
    }

    setSurface(m_surface);

    return m_currentActivate;
}


EVRCustomPresenterActivate::EVRCustomPresenterActivate()
    : MFAbstractActivate()
    , m_presenter(0)
    , m_surface(0)
{ }

HRESULT EVRCustomPresenterActivate::ActivateObject(REFIID riid, void **ppv)
{
    if (!ppv)
        return E_INVALIDARG;
    QMutexLocker locker(&m_mutex);
    if (!m_presenter) {
        m_presenter = new EVRCustomPresenter;
        if (m_surface)
            m_presenter->setSurface(m_surface);
    }
    return m_presenter->QueryInterface(riid, ppv);
}

HRESULT EVRCustomPresenterActivate::ShutdownObject()
{
    // The presenter does not implement IMFShutdown so
    // this function is the same as DetachObject()
    return DetachObject();
}

HRESULT EVRCustomPresenterActivate::DetachObject()
{
    QMutexLocker locker(&m_mutex);
    if (m_presenter) {
        m_presenter->Release();
        m_presenter = 0;
    }
    return S_OK;
}

void EVRCustomPresenterActivate::setSurface(QAbstractVideoSurface *surface)
{
    QMutexLocker locker(&m_mutex);
    if (m_surface == surface)
        return;

    m_surface = surface;

    if (m_presenter)
        m_presenter->setSurface(surface);
}

#include "moc_mfvideorenderercontrol.cpp"
#include "mfvideorenderercontrol.moc"
