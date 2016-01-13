/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "videosurfacefilter.h"

#include "directshoweventloop.h"
#include "directshowglobal.h"
#include "directshowpinenum.h"
#include "mediasamplevideobuffer.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qcoreevent.h>
#include <QtCore/qthread.h>
#include <qabstractvideosurface.h>

#include <initguid.h>

// { e23cad72-153d-406c-bf3f-4c4b523d96f2 }
DEFINE_GUID(CLSID_VideoSurfaceFilter,
0xe23cad72, 0x153d, 0x406c, 0xbf, 0x3f, 0x4c, 0x4b, 0x52, 0x3d, 0x96, 0xf2);

VideoSurfaceFilter::VideoSurfaceFilter(
        QAbstractVideoSurface *surface, DirectShowEventLoop *loop, QObject *parent)
    : QObject(parent)
    , m_ref(1)
    , m_state(State_Stopped)
    , m_surface(surface)
    , m_loop(loop)
    , m_graph(0)
    , m_peerPin(0)
    , m_bytesPerLine(0)
    , m_startResult(S_OK)
    , m_pinId(QString::fromLatin1("reference"))
    , m_sampleScheduler(static_cast<IPin *>(this))
{
    connect(surface, SIGNAL(supportedFormatsChanged()), this, SLOT(supportedFormatsChanged()));
    connect(&m_sampleScheduler, SIGNAL(sampleReady()), this, SLOT(sampleReady()));
}

VideoSurfaceFilter::~VideoSurfaceFilter()
{
    Q_ASSERT(m_ref == 1);
}

HRESULT VideoSurfaceFilter::QueryInterface(REFIID riid, void **ppvObject)
{
    // 2dd74950-a890-11d1-abe8-00a0c905f375
    static const GUID iid_IAmFilterMiscFlags = {
        0x2dd74950, 0xa890, 0x11d1, {0xab, 0xe8, 0x00, 0xa0, 0xc9, 0x05, 0xf3, 0x75} };

    if (!ppvObject) {
        return E_POINTER;
    } else if (riid == IID_IUnknown
            || riid == IID_IPersist
            || riid == IID_IMediaFilter
            || riid == IID_IBaseFilter) {
        *ppvObject = static_cast<IBaseFilter *>(this);
    } else if (riid == iid_IAmFilterMiscFlags) {
        *ppvObject = static_cast<IAMFilterMiscFlags *>(this);
    } else if (riid == IID_IPin) {
        *ppvObject = static_cast<IPin *>(this);
    } else if (riid == IID_IMemInputPin) {
        *ppvObject = static_cast<IMemInputPin *>(&m_sampleScheduler);
    } else {
        *ppvObject = 0;

        return E_NOINTERFACE;
    }

    AddRef();

    return S_OK;
}

ULONG VideoSurfaceFilter::AddRef()
{
    return InterlockedIncrement(&m_ref);
}

ULONG VideoSurfaceFilter::Release()
{
    ULONG ref = InterlockedDecrement(&m_ref);

    Q_ASSERT(ref != 0);

    return ref;
}

HRESULT VideoSurfaceFilter::GetClassID(CLSID *pClassID)
{
    *pClassID = CLSID_VideoSurfaceFilter;

    return S_OK;
}

HRESULT VideoSurfaceFilter::Run(REFERENCE_TIME tStart)
{
    m_state = State_Running;

    m_sampleScheduler.run(tStart);

    return S_OK;
}

HRESULT VideoSurfaceFilter::Pause()
{
    m_state = State_Paused;

    m_sampleScheduler.pause();

    return S_OK;
}

HRESULT VideoSurfaceFilter::Stop()
{
    m_state = State_Stopped;

    m_sampleScheduler.stop();

    return S_OK;
}

HRESULT VideoSurfaceFilter::GetState(DWORD dwMilliSecsTimeout, FILTER_STATE *pState)
{
    Q_UNUSED(dwMilliSecsTimeout)
    if (!pState)
        return E_POINTER;

    *pState = m_state;

    return S_OK;
}

HRESULT VideoSurfaceFilter::SetSyncSource(IReferenceClock *pClock)
{

    m_sampleScheduler.setClock(pClock);

    return S_OK;
}

HRESULT VideoSurfaceFilter::GetSyncSource(IReferenceClock **ppClock)
{
    if (!ppClock) {
        return E_POINTER;
    } else {
        *ppClock = m_sampleScheduler.clock();

        if (*ppClock) {
            (*ppClock)->AddRef();

            return S_OK;
        } else {
            return S_FALSE;
        }
    }
}

HRESULT VideoSurfaceFilter::EnumPins(IEnumPins **ppEnum)
{
    if (ppEnum) {
        *ppEnum = new DirectShowPinEnum(QList<IPin *>() << this);

        return S_OK;
    } else {
        return E_POINTER;
    }
}

HRESULT VideoSurfaceFilter::FindPin(LPCWSTR pId, IPin **ppPin)
{
    if (!ppPin || !pId) {
        return E_POINTER;
    } else if (QString::fromWCharArray(pId) == m_pinId) {
        AddRef();

        *ppPin = this;

        return S_OK;
    } else {
        return VFW_E_NOT_FOUND;
    }
}

HRESULT VideoSurfaceFilter::JoinFilterGraph(IFilterGraph *pGraph, LPCWSTR pName)
{
    m_graph = pGraph;
    m_name = QString::fromWCharArray(pName);

    return S_OK;
}

HRESULT VideoSurfaceFilter::QueryFilterInfo(FILTER_INFO *pInfo)
{
    if (pInfo) {
        QString name = m_name;

        if (name.length() >= MAX_FILTER_NAME)
            name.truncate(MAX_FILTER_NAME - 1);

        int length = name.toWCharArray(pInfo->achName);
        pInfo->achName[length] = '\0';

        if (m_graph)
            m_graph->AddRef();

        pInfo->pGraph = m_graph;

        return S_OK;
    } else {
        return E_POINTER;
    }
}

HRESULT VideoSurfaceFilter::QueryVendorInfo(LPWSTR *pVendorInfo)
{
    Q_UNUSED(pVendorInfo);

    return E_NOTIMPL;
}

ULONG VideoSurfaceFilter::GetMiscFlags()
{
    return AM_FILTER_MISC_FLAGS_IS_RENDERER;
}


HRESULT VideoSurfaceFilter::Connect(IPin *pReceivePin, const AM_MEDIA_TYPE *pmt)
{
    Q_UNUSED(pReceivePin)
    Q_UNUSED(pmt)
    // This is an input pin, you shouldn't be calling Connect on it.
    return E_POINTER;
}

HRESULT VideoSurfaceFilter::ReceiveConnection(IPin *pConnector, const AM_MEDIA_TYPE *pmt)
{
    if (!pConnector) {
        return E_POINTER;
    } else if (!pmt) {
        return E_POINTER;
    } else {
        HRESULT hr;
        QMutexLocker locker(&m_mutex);

        if (m_peerPin) {
            hr = VFW_E_ALREADY_CONNECTED;
        } else if (pmt->majortype != MEDIATYPE_Video) {
            hr = VFW_E_TYPE_NOT_ACCEPTED;
        } else {
            m_surfaceFormat = DirectShowMediaType::formatFromType(*pmt);
            m_bytesPerLine = DirectShowMediaType::bytesPerLine(m_surfaceFormat);

            if (thread() == QThread::currentThread()) {
                hr = start();
            } else {
                m_loop->postEvent(this, new QEvent(QEvent::Type(StartSurface)));

                m_wait.wait(&m_mutex);

                hr = m_startResult;
            }
        }
        if (hr == S_OK) {
           m_peerPin = pConnector;
           m_peerPin->AddRef();

           DirectShowMediaType::copy(&m_mediaType, *pmt);
        }
        return hr;
    }
}

HRESULT VideoSurfaceFilter::start()
{
    if (!m_surface->isFormatSupported(m_surfaceFormat)) {
        return VFW_E_TYPE_NOT_ACCEPTED;
    }
    if (!m_surface->start(m_surfaceFormat)) {
        return VFW_E_TYPE_NOT_ACCEPTED;
    } else {
        return S_OK;
    }
}

HRESULT VideoSurfaceFilter::Disconnect()
{
    QMutexLocker locker(&m_mutex);

    if (!m_peerPin)
        return S_FALSE;

    if (thread() == QThread::currentThread()) {
        stop();
    } else {
        m_loop->postEvent(this, new QEvent(QEvent::Type(StopSurface)));

        m_wait.wait(&m_mutex);
    }

    m_mediaType.clear();

    m_sampleScheduler.NotifyAllocator(0, FALSE);

    m_peerPin->Release();
    m_peerPin = 0;

    return S_OK;
}

void VideoSurfaceFilter::stop()
{
    m_surface->stop();
}

HRESULT VideoSurfaceFilter::ConnectedTo(IPin **ppPin)
{
    if (!ppPin) {
        return E_POINTER;
    } else {
        QMutexLocker locker(&m_mutex);

        if (!m_peerPin) {
            return VFW_E_NOT_CONNECTED;
        } else {
            m_peerPin->AddRef();

            *ppPin = m_peerPin;

            return S_OK;
        }
    }
}

HRESULT VideoSurfaceFilter::ConnectionMediaType(AM_MEDIA_TYPE *pmt)
{
    if (!pmt) {
        return E_POINTER;
    } else {
        QMutexLocker locker(&m_mutex);

        if (!m_peerPin) {
            return VFW_E_NOT_CONNECTED;
        } else {
            DirectShowMediaType::copy(pmt, m_mediaType);

            return S_OK;
        }
    }
}

HRESULT VideoSurfaceFilter::QueryPinInfo(PIN_INFO *pInfo)
{
    if (!pInfo) {
        return E_POINTER;
    } else {
        AddRef();

        pInfo->pFilter = this;
        pInfo->dir = PINDIR_INPUT;

        const int bytes = qMin(MAX_FILTER_NAME, (m_pinId.length() + 1) * 2);

        ::memcpy(pInfo->achName, m_pinId.utf16(), bytes);

        return S_OK;
    }
}

HRESULT VideoSurfaceFilter::QueryId(LPWSTR *Id)
{
    if (!Id) {
        return E_POINTER;
    } else {
        const int bytes = (m_pinId.length() + 1) * 2;

        *Id = static_cast<LPWSTR>(::CoTaskMemAlloc(bytes));

        ::memcpy(*Id, m_pinId.utf16(), bytes);

        return S_OK;
    }
}

HRESULT VideoSurfaceFilter::QueryAccept(const AM_MEDIA_TYPE *pmt)
{
    return !m_surface->isFormatSupported(DirectShowMediaType::formatFromType(*pmt))
            ? S_OK
            : S_FALSE;
}

HRESULT VideoSurfaceFilter::EnumMediaTypes(IEnumMediaTypes **ppEnum)
{
    if (!ppEnum) {
        return E_POINTER;
    } else {
        QMutexLocker locker(&m_mutex);

        *ppEnum = createMediaTypeEnum();

        return S_OK;
    }
}

HRESULT VideoSurfaceFilter::QueryInternalConnections(IPin **apPin, ULONG *nPin)
{
    Q_UNUSED(apPin);
    Q_UNUSED(nPin);

    return E_NOTIMPL;
}

HRESULT VideoSurfaceFilter::EndOfStream()
{
    QMutexLocker locker(&m_mutex);

    if (!m_sampleScheduler.scheduleEndOfStream()) {
        if (IMediaEventSink *sink = com_cast<IMediaEventSink>(m_graph, IID_IMediaEventSink)) {
            sink->Notify(
                    EC_COMPLETE,
                    S_OK,
                    reinterpret_cast<LONG_PTR>(static_cast<IBaseFilter *>(this)));
            sink->Release();
        }
    }

    return S_OK;
}

HRESULT VideoSurfaceFilter::BeginFlush()
{
    QMutexLocker locker(&m_mutex);

    m_sampleScheduler.setFlushing(true);

    if (thread() == QThread::currentThread()) {
        flush();
    } else {
        m_loop->postEvent(this, new QEvent(QEvent::Type(FlushSurface)));

        m_wait.wait(&m_mutex);
    }

    return S_OK;
}

HRESULT VideoSurfaceFilter::EndFlush()
{
    QMutexLocker locker(&m_mutex);

    m_sampleScheduler.setFlushing(false);

    return S_OK;
}

void VideoSurfaceFilter::flush()
{
    m_surface->present(QVideoFrame());

    m_wait.wakeAll();
}

HRESULT VideoSurfaceFilter::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
    Q_UNUSED(tStart);
    Q_UNUSED(tStop);
    Q_UNUSED(dRate);

    return S_OK;
}

HRESULT VideoSurfaceFilter::QueryDirection(PIN_DIRECTION *pPinDir)
{
    if (!pPinDir) {
        return E_POINTER;
    } else {
        *pPinDir = PINDIR_INPUT;

        return S_OK;
    }
}

int VideoSurfaceFilter::currentMediaTypeToken()
{
    QMutexLocker locker(&m_mutex);

    return DirectShowMediaTypeList::currentMediaTypeToken();
}

HRESULT VideoSurfaceFilter::nextMediaType(
        int token, int *index, ULONG count, AM_MEDIA_TYPE **types, ULONG *fetchedCount)
{
    QMutexLocker locker(&m_mutex);

    return DirectShowMediaTypeList::nextMediaType(token, index, count, types, fetchedCount);

}

HRESULT VideoSurfaceFilter::skipMediaType(int token, int *index, ULONG count)
{
    QMutexLocker locker(&m_mutex);

    return DirectShowMediaTypeList::skipMediaType(token, index, count);
}

HRESULT VideoSurfaceFilter::cloneMediaType(int token, int index, IEnumMediaTypes **enumeration)
{
    QMutexLocker locker(&m_mutex);

    return DirectShowMediaTypeList::cloneMediaType(token, index, enumeration);
}

void VideoSurfaceFilter::customEvent(QEvent *event)
{
    const int type = event->type();
    if (type == StartSurface) {
        QMutexLocker locker(&m_mutex);

        m_startResult = start();

        m_wait.wakeAll();
    } else if (type == StopSurface) {
        QMutexLocker locker(&m_mutex);

        stop();

        m_wait.wakeAll();
    } else if (type == FlushSurface) {
        QMutexLocker locker(&m_mutex);

        flush();

        m_wait.wakeAll();
    } else {
       QObject::customEvent(event);
    }
}

void VideoSurfaceFilter::supportedFormatsChanged()
{
    QMutexLocker locker(&m_mutex);

    // MEDIASUBTYPE_None;
    static const GUID none = {
        0xe436eb8e, 0x524f, 0x11ce, {0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70} };

    QList<QVideoFrame::PixelFormat> formats = m_surface->supportedPixelFormats();

    QVector<AM_MEDIA_TYPE> mediaTypes;
    mediaTypes.reserve(formats.count());

    AM_MEDIA_TYPE type;
    type.majortype = MEDIATYPE_Video;
    type.bFixedSizeSamples = TRUE;
    type.bTemporalCompression = FALSE;
    type.lSampleSize = 0;
    type.formattype = GUID_NULL;
    type.pUnk = 0;
    type.cbFormat = 0;
    type.pbFormat = 0;

    foreach (QVideoFrame::PixelFormat format, formats) {
        type.subtype = DirectShowMediaType::convertPixelFormat(format);

        if (type.subtype != none)
            mediaTypes.append(type);
    }

    setMediaTypes(mediaTypes);
}

void VideoSurfaceFilter::sampleReady()
{
    bool eos = false;

    IMediaSample *sample = m_sampleScheduler.takeSample(&eos);

    if (sample) {
        m_surface->present(QVideoFrame(
                new MediaSampleVideoBuffer(sample, m_bytesPerLine),
                m_surfaceFormat.frameSize(),
                m_surfaceFormat.pixelFormat()));

        sample->Release();

        if (eos) {
            if (IMediaEventSink *sink = com_cast<IMediaEventSink>(m_graph, IID_IMediaEventSink)) {
                sink->Notify(
                        EC_COMPLETE,
                        S_OK,
                        reinterpret_cast<LONG_PTR>(static_cast<IBaseFilter *>(this)));
                sink->Release();
            }
        }
    }
}

