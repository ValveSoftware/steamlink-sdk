/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "videosurfacefilter.h"

#include "directshoweventloop.h"
#include "directshowglobal.h"
#include "directshowvideobuffer.h"

#include <QtCore/qthread.h>
#include <QtCore/qloggingcategory.h>
#include <qabstractvideosurface.h>

#include <initguid.h>

Q_LOGGING_CATEGORY(qLcRenderFilter, "qt.multimedia.plugins.directshow.renderfilter")

// { e23cad72-153d-406c-bf3f-4c4b523d96f2 }
DEFINE_GUID(CLSID_VideoSurfaceFilter,
0xe23cad72, 0x153d, 0x406c, 0xbf, 0x3f, 0x4c, 0x4b, 0x52, 0x3d, 0x96, 0xf2);

class VideoSurfaceInputPin : public DirectShowInputPin
{
    DIRECTSHOW_OBJECT

public:
    VideoSurfaceInputPin(VideoSurfaceFilter *filter);

    // DirectShowPin
    bool isMediaTypeSupported(const DirectShowMediaType *type);
    bool setMediaType(const DirectShowMediaType *type);

    HRESULT completeConnection(IPin *pin);
    HRESULT connectionEnded();

    // IPin
    STDMETHODIMP ReceiveConnection(IPin *pConnector, const AM_MEDIA_TYPE *pmt);
    STDMETHODIMP Disconnect();
    STDMETHODIMP EndOfStream();
    STDMETHODIMP BeginFlush();
    STDMETHODIMP EndFlush();

    // IMemInputPin
    STDMETHODIMP GetAllocatorRequirements(ALLOCATOR_PROPERTIES *pProps);
    STDMETHODIMP Receive(IMediaSample *pMediaSample);

private:
    VideoSurfaceFilter *m_videoSurfaceFilter;
};

VideoSurfaceInputPin::VideoSurfaceInputPin(VideoSurfaceFilter *filter)
    : DirectShowInputPin(filter, QStringLiteral("Input"))
    , m_videoSurfaceFilter(filter)
{
}

bool VideoSurfaceInputPin::isMediaTypeSupported(const DirectShowMediaType *type)
{
    return m_videoSurfaceFilter->isMediaTypeSupported(type);
}

bool VideoSurfaceInputPin::setMediaType(const DirectShowMediaType *type)
{
    if (!DirectShowInputPin::setMediaType(type))
        return false;

    return m_videoSurfaceFilter->setMediaType(type);
}

HRESULT VideoSurfaceInputPin::completeConnection(IPin *pin)
{
    HRESULT hr = DirectShowInputPin::completeConnection(pin);
    if (FAILED(hr))
        return hr;

    return m_videoSurfaceFilter->completeConnection(pin);
}

HRESULT VideoSurfaceInputPin::connectionEnded()
{
    HRESULT hr = DirectShowInputPin::connectionEnded();
    if (FAILED(hr))
        return hr;

    return m_videoSurfaceFilter->connectionEnded();
}

HRESULT VideoSurfaceInputPin::ReceiveConnection(IPin *pConnector, const AM_MEDIA_TYPE *pmt)
{
    QMutexLocker lock(&m_videoSurfaceFilter->m_mutex);
    return DirectShowInputPin::ReceiveConnection(pConnector, pmt);
}

HRESULT VideoSurfaceInputPin::Disconnect()
{
    QMutexLocker lock(&m_videoSurfaceFilter->m_mutex);
    return DirectShowInputPin::Disconnect();
}

HRESULT VideoSurfaceInputPin::EndOfStream()
{
    QMutexLocker lock(&m_videoSurfaceFilter->m_mutex);
    QMutexLocker renderLock(&m_videoSurfaceFilter->m_renderMutex);

    HRESULT hr = DirectShowInputPin::EndOfStream();
    if (hr != S_OK)
        return hr;

    return m_videoSurfaceFilter->EndOfStream();
}

HRESULT VideoSurfaceInputPin::BeginFlush()
{
    QMutexLocker lock(&m_videoSurfaceFilter->m_mutex);
    {
        QMutexLocker renderLock(&m_videoSurfaceFilter->m_renderMutex);
        DirectShowInputPin::BeginFlush();
        m_videoSurfaceFilter->BeginFlush();
    }
    m_videoSurfaceFilter->resetEOS();

    return S_OK;
}

HRESULT VideoSurfaceInputPin::EndFlush()
{
    QMutexLocker lock(&m_videoSurfaceFilter->m_mutex);
    QMutexLocker renderLock(&m_videoSurfaceFilter->m_renderMutex);

    HRESULT hr = m_videoSurfaceFilter->EndFlush();
    if (SUCCEEDED(hr))
        hr = DirectShowInputPin::EndFlush();
    return hr;
}

HRESULT VideoSurfaceInputPin::GetAllocatorRequirements(ALLOCATOR_PROPERTIES *pProps)
{
    if (!pProps)
        return E_POINTER;

    // We need at least two allocated buffers, one for holding the frame currently being
    // rendered and another one to decode the following frame at the same time.
    pProps->cBuffers = 2;

    return S_OK;
}

HRESULT VideoSurfaceInputPin::Receive(IMediaSample *pMediaSample)
{
    HRESULT hr = m_videoSurfaceFilter->Receive(pMediaSample);
    if (FAILED(hr)) {
        QMutexLocker locker(&m_videoSurfaceFilter->m_mutex);
        if (m_videoSurfaceFilter->state() != State_Stopped && !m_flushing && !m_inErrorState) {
            m_videoSurfaceFilter->NotifyEvent(EC_ERRORABORT, hr, 0);
            {
                QMutexLocker renderLocker(&m_videoSurfaceFilter->m_renderMutex);
                if (m_videoSurfaceFilter->m_running && !m_videoSurfaceFilter->m_EOSDelivered)
                    m_videoSurfaceFilter->notifyEOS();
            }
            m_inErrorState = true;
        }
    }

    return hr;
}


VideoSurfaceFilter::VideoSurfaceFilter(QAbstractVideoSurface *surface, DirectShowEventLoop *loop, QObject *parent)
    : QObject(parent)
    , m_loop(loop)
    , m_pin(NULL)
    , m_surface(surface)
    , m_bytesPerLine(0)
    , m_surfaceStarted(false)
    , m_renderMutex(QMutex::Recursive)
    , m_running(false)
    , m_pendingSample(NULL)
    , m_pendingSampleEndTime(0)
    , m_renderEvent(CreateEvent(NULL, FALSE, FALSE, NULL))
    , m_flushEvent(CreateEvent(NULL, TRUE, FALSE, NULL))
    , m_adviseCookie(0)
    , m_EOS(false)
    , m_EOSDelivered(false)
    , m_EOSTimer(0)
{
    supportedFormatsChanged();
    connect(surface, &QAbstractVideoSurface::supportedFormatsChanged,
            this, &VideoSurfaceFilter::supportedFormatsChanged);
}

VideoSurfaceFilter::~VideoSurfaceFilter()
{
    clearPendingSample();

    if (m_pin)
        m_pin->Release();

    CloseHandle(m_flushEvent);
    CloseHandle(m_renderEvent);
}

HRESULT VideoSurfaceFilter::getInterface(const IID &riid, void **ppvObject)
{
    if (riid == IID_IAMFilterMiscFlags)
        return GetInterface(static_cast<IAMFilterMiscFlags*>(this), ppvObject);
    else
        return DirectShowBaseFilter::getInterface(riid, ppvObject);
}

QList<DirectShowPin *> VideoSurfaceFilter::pins()
{
    if (!m_pin)
        m_pin = new VideoSurfaceInputPin(this);

    return QList<DirectShowPin *>() << m_pin;
}

HRESULT VideoSurfaceFilter::GetClassID(CLSID *pClassID)
{
    *pClassID = CLSID_VideoSurfaceFilter;
    return S_OK;
}

ULONG VideoSurfaceFilter::GetMiscFlags()
{
    return AM_FILTER_MISC_FLAGS_IS_RENDERER;
}

void VideoSurfaceFilter::supportedFormatsChanged()
{
    QWriteLocker writeLocker(&m_typesLock);

    qCDebug(qLcRenderFilter, "supportedFormatChanged");

    m_supportedTypes.clear();

    const QList<QVideoFrame::PixelFormat> formats = m_surface->supportedPixelFormats();
    m_supportedTypes.reserve(formats.count());

    for (QVideoFrame::PixelFormat format : formats) {
        GUID subtype = DirectShowMediaType::convertPixelFormat(format);
        if (!IsEqualGUID(subtype, MEDIASUBTYPE_None)) {
            qCDebug(qLcRenderFilter) << "  " << format;
            m_supportedTypes.append(subtype);
        }
    }
}

bool VideoSurfaceFilter::isMediaTypeSupported(const DirectShowMediaType *type)
{
    if (type->majortype != MEDIATYPE_Video || type->bFixedSizeSamples == FALSE)
        return false;

    QReadLocker readLocker(&m_typesLock);

    for (const GUID &supportedType : m_supportedTypes) {
        if (IsEqualGUID(supportedType, type->subtype))
            return true;
    }

    return false;
}

bool VideoSurfaceFilter::setMediaType(const DirectShowMediaType *type)
{
    if (!type) {
        qCDebug(qLcRenderFilter, "clear media type");
        m_surfaceFormat = QVideoSurfaceFormat();
        m_bytesPerLine = 0;
        return true;
    } else {
        m_surfaceFormat = DirectShowMediaType::formatFromType(*type);
        m_bytesPerLine = DirectShowMediaType::bytesPerLine(m_surfaceFormat);
        qCDebug(qLcRenderFilter) << "setMediaType -->" << m_surfaceFormat;
        return m_surfaceFormat.isValid();
    }
}

HRESULT VideoSurfaceFilter::completeConnection(IPin *pin)
{
    Q_UNUSED(pin);

    qCDebug(qLcRenderFilter, "completeConnection");

    if (!startSurface())
        return VFW_E_TYPE_NOT_ACCEPTED;
    else
        return S_OK;
}

HRESULT VideoSurfaceFilter::connectionEnded()
{
    qCDebug(qLcRenderFilter, "connectionEnded");

    stopSurface();

    return S_OK;
}

HRESULT VideoSurfaceFilter::Run(REFERENCE_TIME tStart)
{
    QMutexLocker locker(&m_mutex);

    if (m_state == State_Running)
        return S_OK;

    qCDebug(qLcRenderFilter, "Run (start=%lli)", tStart);

    HRESULT hr = DirectShowBaseFilter::Run(tStart);
    if (FAILED(hr))
        return hr;

    ResetEvent(m_flushEvent);

    IMemAllocator *allocator;
    if (SUCCEEDED(m_pin->GetAllocator(&allocator))) {
        allocator->Commit();
        allocator->Release();
    }

    QMutexLocker renderLocker(&m_renderMutex);

    m_running = true;

    if (!m_pendingSample)
        checkEOS();
    else if (!scheduleSample(m_pendingSample))
        SetEvent(m_renderEvent); // render immediately

    return S_OK;
}

HRESULT VideoSurfaceFilter::Pause()
{
    QMutexLocker locker(&m_mutex);

    if (m_state == State_Paused)
        return S_OK;

    qCDebug(qLcRenderFilter, "Pause");

    HRESULT hr = DirectShowBaseFilter::Pause();
    if (FAILED(hr))
        return hr;

    m_renderMutex.lock();
    m_EOSDelivered = false;
    m_running = false;
    m_renderMutex.unlock();

    resetEOSTimer();
    ResetEvent(m_flushEvent);
    unscheduleSample();

    IMemAllocator *allocator;
    if (SUCCEEDED(m_pin->GetAllocator(&allocator))) {
        allocator->Commit();
        allocator->Release();
    }

    return S_OK;
}

HRESULT VideoSurfaceFilter::Stop()
{
    QMutexLocker locker(&m_mutex);

    if (m_state == State_Stopped)
        return S_OK;

    qCDebug(qLcRenderFilter, "Stop");

    DirectShowBaseFilter::Stop();

    clearPendingSample();

    m_renderMutex.lock();
    m_EOSDelivered = false;
    m_running = false;
    m_renderMutex.unlock();

    SetEvent(m_flushEvent);
    resetEOS();
    unscheduleSample();
    flushSurface();

    IMemAllocator *allocator;
    if (SUCCEEDED(m_pin->GetAllocator(&allocator))) {
        allocator->Decommit();
        allocator->Release();
    }

    return S_OK;
}

HRESULT VideoSurfaceFilter::EndOfStream()
{
    QMutexLocker renderLocker(&m_renderMutex);

    qCDebug(qLcRenderFilter, "EndOfStream");

    m_EOS = true;

    if (!m_pendingSample && m_running)
        checkEOS();

    return S_OK;
}

HRESULT VideoSurfaceFilter::BeginFlush()
{
    qCDebug(qLcRenderFilter, "BeginFlush");

    SetEvent(m_flushEvent);
    unscheduleSample();
    clearPendingSample();

    return S_OK;
}

HRESULT VideoSurfaceFilter::EndFlush()
{
    qCDebug(qLcRenderFilter, "EndFlush");

    ResetEvent(m_flushEvent);
    return S_OK;
}

HRESULT VideoSurfaceFilter::Receive(IMediaSample *pMediaSample)
{
    {
        QMutexLocker locker(&m_mutex);

        qCDebug(qLcRenderFilter, "Receive (sample=%p)", pMediaSample);

        HRESULT hr = m_pin->DirectShowInputPin::Receive(pMediaSample);
        if (hr != S_OK) {
            qCDebug(qLcRenderFilter, "  can't receive sample (error %X)", uint(hr));
            return E_FAIL;
        }

        // If the format dynamically changed, the sample contains information about the new format.
        // We need to reset the format and restart the QAbstractVideoSurface.
        if (m_pin->currentSampleProperties()->pMediaType
                && (!m_pin->setMediaType(reinterpret_cast<const DirectShowMediaType *>(m_pin->currentSampleProperties()->pMediaType))
                    || !restartSurface())) {
                qCWarning(qLcRenderFilter, "  dynamic format change failed, aborting rendering");
                NotifyEvent(EC_ERRORABORT, VFW_E_TYPE_NOT_ACCEPTED, 0);
                return VFW_E_INVALIDMEDIATYPE;
        }

        {
            QMutexLocker locker(&m_renderMutex);

            if (m_pendingSample || m_EOS)
                return E_UNEXPECTED;

            if (m_running && !scheduleSample(pMediaSample)) {
                qCWarning(qLcRenderFilter, "  sample can't be scheduled, discarding it");
                return S_OK;
            }

            m_pendingSample = pMediaSample;
            m_pendingSample->AddRef();
            m_pendingSampleEndTime = m_pin->currentSampleProperties()->tStop;
        }

        if (m_state == State_Paused) // Render immediately
            renderPendingSample();
    }

    qCDebug(qLcRenderFilter, "  waiting for render time");

    // Wait for render time. The clock will wake us up whenever the time comes.
    // It can also be interrupted by a flush, pause or stop.
    HANDLE waitObjects[] = { m_flushEvent, m_renderEvent };
    DWORD result = WAIT_TIMEOUT;
    while (result == WAIT_TIMEOUT)
        result = WaitForMultipleObjects(2, waitObjects, FALSE, INFINITE);

    if (result == WAIT_OBJECT_0) {
        // render interrupted (flush, pause, stop)
        qCDebug(qLcRenderFilter, " rendering of sample %p interrupted", pMediaSample);
        return S_OK;
    }

    m_adviseCookie = 0;

    QMutexLocker locker(&m_mutex);

    // State might have changed just before the lock
    if (m_state == State_Stopped) {
        qCDebug(qLcRenderFilter, "  state changed to Stopped, discarding sample (%p)", pMediaSample);
        return S_OK;
    }

    QMutexLocker renderLock(&m_renderMutex);

    // Flush or pause might have happened just before the lock
    if (m_pendingSample && m_running) {
        renderLock.unlock();
        renderPendingSample();
        renderLock.relock();
    } else {
        qCDebug(qLcRenderFilter, "  discarding sample (%p)", pMediaSample);
    }

    clearPendingSample();
    checkEOS();
    ResetEvent(m_renderEvent);

    return S_OK;
}

bool VideoSurfaceFilter::scheduleSample(IMediaSample *sample)
{
    if (!sample)
        return false;

    qCDebug(qLcRenderFilter, "scheduleSample (sample=%p)", sample);

    REFERENCE_TIME sampleStart, sampleEnd;
    if (FAILED(sample->GetTime(&sampleStart, &sampleEnd)) || !m_clock) {
        qCDebug(qLcRenderFilter, "  render now");
        SetEvent(m_renderEvent); // Render immediately
        return true;
    }

    if (sampleEnd < sampleStart) { // incorrect times
        qCWarning(qLcRenderFilter, "  invalid sample times (start=%lli, end=%lli)", sampleStart, sampleEnd);
        return false;
    }

    HRESULT hr = m_clock->AdviseTime(m_startTime, sampleStart, (HEVENT)m_renderEvent, &m_adviseCookie);
    if (FAILED(hr)) {
        qCWarning(qLcRenderFilter, "  clock failed to advise time (error=%X)", uint(hr));
        return false;
    }

    return true;
}

void VideoSurfaceFilter::unscheduleSample()
{
    if (m_adviseCookie) {
        qCDebug(qLcRenderFilter, "unscheduleSample");
        m_clock->Unadvise(m_adviseCookie);
        m_adviseCookie = 0;
    }

    ResetEvent(m_renderEvent);
}

void VideoSurfaceFilter::clearPendingSample()
{
    QMutexLocker locker(&m_renderMutex);
    if (m_pendingSample) {
        qCDebug(qLcRenderFilter, "clearPendingSample");
        m_pendingSample->Release();
        m_pendingSample = NULL;
    }
}

void QT_WIN_CALLBACK EOSTimerCallback(UINT, UINT, DWORD_PTR dwUser, DWORD_PTR, DWORD_PTR)
{
    VideoSurfaceFilter *that = reinterpret_cast<VideoSurfaceFilter *>(dwUser);
    that->onEOSTimerTimeout();
}

void VideoSurfaceFilter::onEOSTimerTimeout()
{
    QMutexLocker locker(&m_renderMutex);

    if (m_EOSTimer) {
        m_EOSTimer = 0;
        checkEOS();
    }
}

void VideoSurfaceFilter::checkEOS()
{
    QMutexLocker locker(&m_renderMutex);

    if (!m_EOS || m_EOSDelivered || m_EOSTimer)
        return;

    if (!m_clock) {
        notifyEOS();
        return;
    }

    REFERENCE_TIME eosTime = m_startTime + m_pendingSampleEndTime;
    REFERENCE_TIME currentTime;
    m_clock->GetTime(&currentTime);
    LONG delay = LONG((eosTime - currentTime) / 10000);

    if (delay < 1) {
        notifyEOS();
    } else {
        qCDebug(qLcRenderFilter, "will trigger EOS in %li", delay);

        m_EOSTimer = timeSetEvent(delay,
                                  1,
                                  EOSTimerCallback,
                                  reinterpret_cast<DWORD_PTR>(this),
                                  TIME_ONESHOT | TIME_CALLBACK_FUNCTION | TIME_KILL_SYNCHRONOUS);

        if (!m_EOSTimer) {
            qDebug("Error with timer");
            notifyEOS();
        }
    }
}

void VideoSurfaceFilter::notifyEOS()
{
    QMutexLocker locker(&m_renderMutex);

    if (!m_running)
        return;

    qCDebug(qLcRenderFilter, "notifyEOS, delivering EC_COMPLETE event");

    m_EOSTimer = 0;
    m_EOSDelivered = true;
    NotifyEvent(EC_COMPLETE, S_OK, (LONG_PTR)(IBaseFilter *)this);
}

void VideoSurfaceFilter::resetEOS()
{
    resetEOSTimer();

    QMutexLocker locker(&m_renderMutex);

    if (m_EOS)
        qCDebug(qLcRenderFilter, "resetEOS (delivered=%s)", m_EOSDelivered ? "true" : "false");

    m_EOS = false;
    m_EOSDelivered = false;
    m_pendingSampleEndTime = 0;
}

void VideoSurfaceFilter::resetEOSTimer()
{
    if (m_EOSTimer) {
        timeKillEvent(m_EOSTimer);
        m_EOSTimer = 0;
    }
}

bool VideoSurfaceFilter::startSurface()
{
    if (QThread::currentThread() != thread()) {
        m_loop->postEvent(this, new QEvent(QEvent::Type(StartSurface)));
        m_waitSurface.wait(&m_mutex);
        return m_surfaceStarted;
    } else {
        m_surfaceStarted = m_surface->start(m_surfaceFormat);
        qCDebug(qLcRenderFilter, "startSurface %s", m_surfaceStarted ? "succeeded" : "failed");
        return m_surfaceStarted;
    }
}

void VideoSurfaceFilter::stopSurface()
{
    if (!m_surfaceStarted)
        return;

    if (QThread::currentThread() != thread()) {
        m_loop->postEvent(this, new QEvent(QEvent::Type(StopSurface)));
        m_waitSurface.wait(&m_mutex);
    } else {
        qCDebug(qLcRenderFilter, "stopSurface");
        m_surface->stop();
        m_surfaceStarted = false;
    }
}

bool VideoSurfaceFilter::restartSurface()
{
    if (QThread::currentThread() != thread()) {
        m_loop->postEvent(this, new QEvent(QEvent::Type(RestartSurface)));
        m_waitSurface.wait(&m_mutex);
        return m_surfaceStarted;
    } else {
        m_surface->stop();
        m_surfaceStarted = m_surface->start(m_surfaceFormat);
        qCDebug(qLcRenderFilter, "restartSurface %s", m_surfaceStarted ? "succeeded" : "failed");
        return m_surfaceStarted;
    }
}

void VideoSurfaceFilter::flushSurface()
{
    if (QThread::currentThread() != thread()) {
        m_loop->postEvent(this, new QEvent(QEvent::Type(FlushSurface)));
        m_waitSurface.wait(&m_mutex);
    } else {
        qCDebug(qLcRenderFilter, "flushSurface");
        m_surface->present(QVideoFrame());
    }
}

void VideoSurfaceFilter::renderPendingSample()
{
    if (QThread::currentThread() != thread()) {
        m_loop->postEvent(this, new QEvent(QEvent::Type(RenderSample)));
        m_waitSurface.wait(&m_mutex);
    } else {
        QMutexLocker locker(&m_renderMutex);
        if (!m_pendingSample)
            return;

        qCDebug(qLcRenderFilter, "presentSample (sample=%p)", m_pendingSample);

        m_surface->present(QVideoFrame(new DirectShowVideoBuffer(m_pendingSample, m_bytesPerLine),
                                       m_surfaceFormat.frameSize(),
                                       m_surfaceFormat.pixelFormat()));
    }
}

bool VideoSurfaceFilter::event(QEvent *e)
{
    if (e->type() == QEvent::Type(StartSurface)) {
        QMutexLocker locker(&m_mutex);
        startSurface();
        m_waitSurface.wakeAll();
        return true;
    } else if (e->type() == QEvent::Type(StopSurface)) {
        QMutexLocker locker(&m_mutex);
        stopSurface();
        m_waitSurface.wakeAll();
        return true;
    } else if (e->type() == QEvent::Type(RestartSurface)) {
        QMutexLocker locker(&m_mutex);
        restartSurface();
        m_waitSurface.wakeAll();
        return true;
    } else if (e->type() == QEvent::Type(FlushSurface)) {
        QMutexLocker locker(&m_mutex);
        flushSurface();
        m_waitSurface.wakeAll();
        return true;
    } else if (e->type() == QEvent::Type(RenderSample)) {
        QMutexLocker locker(&m_mutex);
        renderPendingSample();
        m_waitSurface.wakeAll();
        return true;
    }

    return QObject::event(e);
}
