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

#include "directshowiosource.h"

#include "directshowglobal.h"
#include "directshowmediatype.h"
#include "directshowmediatypeenum.h"
#include "directshowpinenum.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qurl.h>

static const GUID directshow_subtypes[] =
{
    MEDIASUBTYPE_NULL,
    MEDIASUBTYPE_Avi,
    MEDIASUBTYPE_Asf,
    MEDIASUBTYPE_MPEG1Video,
    MEDIASUBTYPE_QTMovie,
    MEDIASUBTYPE_WAVE,
    MEDIASUBTYPE_AIFF,
    MEDIASUBTYPE_AU,
    MEDIASUBTYPE_DssVideo,
    MEDIASUBTYPE_MPEG1Audio,
    MEDIASUBTYPE_MPEG1System,
    MEDIASUBTYPE_MPEG1VideoCD
};

DirectShowIOSource::DirectShowIOSource(DirectShowEventLoop *loop)
    : m_ref(1)
    , m_state(State_Stopped)
    , m_reader(0)
    , m_loop(loop)
    , m_graph(0)
    , m_clock(0)
    , m_allocator(0)
    , m_peerPin(0)
    , m_pinId(QLatin1String("Data"))
    , m_queriedForAsyncReader(false)
{
    // This filter has only one possible output type, that is, a stream of data
    // with no particular subtype. The graph builder will try every demux/decode filters
    // to find one able to decode the stream.
    //
    // The filter works in pull mode, the downstream filter is responsible for requesting
    // samples from this one.
    //
    AM_MEDIA_TYPE type =
    {
        MEDIATYPE_Stream,  // majortype
        MEDIASUBTYPE_NULL, // subtype
        TRUE,              // bFixedSizeSamples
        FALSE,             // bTemporalCompression
        1,                 // lSampleSize
        GUID_NULL,         // formattype
        0,                 // pUnk
        0,                 // cbFormat
        0,                 // pbFormat
    };

    static const int count = sizeof(directshow_subtypes) / sizeof(GUID);

    for (int i = 0; i < count; ++i) {
        type.subtype = directshow_subtypes[i];
        m_supportedMediaTypes.append(type);
    }
}

DirectShowIOSource::~DirectShowIOSource()
{
    Q_ASSERT(m_ref == 0);

    delete m_reader;
}

void DirectShowIOSource::setDevice(QIODevice *device)
{
    Q_ASSERT(!m_reader);

    m_reader = new DirectShowIOReader(device, this, m_loop);
}

void DirectShowIOSource::setAllocator(IMemAllocator *allocator)
{
    if (m_allocator == allocator)
        return;

    if (m_allocator)
        m_allocator->Release();

    m_allocator = allocator;

    if (m_allocator)
        m_allocator->AddRef();
}

// IUnknown
HRESULT DirectShowIOSource::QueryInterface(REFIID riid, void **ppvObject)
{
    // 2dd74950-a890-11d1-abe8-00a0c905f375
    static const GUID iid_IAmFilterMiscFlags = {
        0x2dd74950, 0xa890, 0x11d1, {0xab, 0xe8, 0x00, 0xa0, 0xc9, 0x05, 0xf3, 0x75}};

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
    } else if (riid == IID_IAsyncReader) {
        m_queriedForAsyncReader = true;
        *ppvObject = static_cast<IAsyncReader *>(m_reader);
    } else {
        *ppvObject = 0;

        return E_NOINTERFACE;
    }

    AddRef();

    return S_OK;
}

ULONG DirectShowIOSource::AddRef()
{
    return InterlockedIncrement(&m_ref);
}

ULONG DirectShowIOSource::Release()
{
    ULONG ref = InterlockedDecrement(&m_ref);

    if (ref == 0) {
        delete this;
    }

    return ref;
}

// IPersist
HRESULT DirectShowIOSource::GetClassID(CLSID *pClassID)
{
    *pClassID = CLSID_NULL;

    return S_OK;
}

// IMediaFilter
HRESULT DirectShowIOSource::Run(REFERENCE_TIME tStart)
{
    Q_UNUSED(tStart)
    QMutexLocker locker(&m_mutex);

    m_state = State_Running;

    return S_OK;
}

HRESULT DirectShowIOSource::Pause()
{
    QMutexLocker locker(&m_mutex);

    m_state = State_Paused;

    return S_OK;
}

HRESULT DirectShowIOSource::Stop()
{
    QMutexLocker locker(&m_mutex);

    m_state = State_Stopped;

    return S_OK;
}

HRESULT DirectShowIOSource::GetState(DWORD dwMilliSecsTimeout, FILTER_STATE *pState)
{
    Q_UNUSED(dwMilliSecsTimeout);

    if (!pState) {
        return E_POINTER;
    } else {
        QMutexLocker locker(&m_mutex);

        *pState = m_state;

        return S_OK;
    }
}

HRESULT DirectShowIOSource::SetSyncSource(IReferenceClock *pClock)
{
    QMutexLocker locker(&m_mutex);

    if (m_clock)
        m_clock->Release();

    m_clock = pClock;

    if (m_clock)
        m_clock->AddRef();

    return S_OK;
}

HRESULT DirectShowIOSource::GetSyncSource(IReferenceClock **ppClock)
{
    if (!ppClock) {
        return E_POINTER;
    } else {
        if (!m_clock) {
            *ppClock = 0;

            return S_FALSE;
        } else {
            m_clock->AddRef();

            *ppClock = m_clock;

            return S_OK;
        }
    }
}

// IBaseFilter
HRESULT DirectShowIOSource::EnumPins(IEnumPins **ppEnum)
{
    if (!ppEnum) {
        return E_POINTER;
    } else {
        *ppEnum = new DirectShowPinEnum(QList<IPin *>() << this);

        return S_OK;
    }
}

HRESULT DirectShowIOSource::FindPin(LPCWSTR Id, IPin **ppPin)
{
    if (!ppPin || !Id) {
        return E_POINTER;
    } else {
        QMutexLocker locker(&m_mutex);
        if (QString::fromWCharArray(Id) == m_pinId) {
            AddRef();

            *ppPin = this;

            return S_OK;
        } else {
            *ppPin = 0;

            return VFW_E_NOT_FOUND;
        }
    }
}

HRESULT DirectShowIOSource::JoinFilterGraph(IFilterGraph *pGraph, LPCWSTR pName)
{
    QMutexLocker locker(&m_mutex);

    m_graph = pGraph;
    m_filterName = QString::fromWCharArray(pName);

    return S_OK;
}

HRESULT DirectShowIOSource::QueryFilterInfo(FILTER_INFO *pInfo)
{
    if (!pInfo) {
        return E_POINTER;
    } else {
        QString name = m_filterName;

        if (name.length() >= MAX_FILTER_NAME)
            name.truncate(MAX_FILTER_NAME - 1);

        int length = name.toWCharArray(pInfo->achName);
        pInfo->achName[length] = '\0';

        if (m_graph)
            m_graph->AddRef();

        pInfo->pGraph = m_graph;

        return S_OK;
    }
}

HRESULT DirectShowIOSource::QueryVendorInfo(LPWSTR *pVendorInfo)
{
    Q_UNUSED(pVendorInfo);

    return E_NOTIMPL;
}

// IAMFilterMiscFlags
ULONG DirectShowIOSource::GetMiscFlags()
{
    return AM_FILTER_MISC_FLAGS_IS_SOURCE;
}

// IPin
HRESULT DirectShowIOSource::Connect(IPin *pReceivePin, const AM_MEDIA_TYPE *pmt)
{
    if (!pReceivePin)
        return E_POINTER;

    QMutexLocker locker(&m_mutex);

    if (m_state != State_Stopped)
        return VFW_E_NOT_STOPPED;

    if (m_peerPin)
        return VFW_E_ALREADY_CONNECTED;

    // If we get a type from the graph manager, check that we support that
    if (pmt && pmt->majortype != MEDIATYPE_Stream)
        return VFW_E_TYPE_NOT_ACCEPTED;

    // This filter only works in pull mode, the downstream filter must query for the
    // AsyncReader interface during ReceiveConnection().
    // If it doesn't, we can't connect to it.
    m_queriedForAsyncReader = false;
    HRESULT hr = 0;
    // Negotiation of media type
    // - Complete'ish type (Stream with subtype specified).
    if (pmt && pmt->subtype != MEDIASUBTYPE_NULL /* aka. GUID_NULL */) {
         hr = pReceivePin->ReceiveConnection(this, pmt);
         // Update the media type for the current connection.
         if (SUCCEEDED(hr))
             m_connectionMediaType = *pmt;
    } else if (pmt && pmt->subtype == MEDIATYPE_NULL) { // - Partial type (Stream, but no subtype specified).
        m_connectionMediaType = *pmt;
        // Check if the receiving pin accepts any of the streaming subtypes.
        for (const DirectShowMediaType &t : qAsConst(m_supportedMediaTypes)) {
            m_connectionMediaType.subtype = t.subtype;
            hr = pReceivePin->ReceiveConnection(this, &m_connectionMediaType);
            if (SUCCEEDED(hr))
                break;
        }
    } else { // - No media type specified.
        // Check if the receiving pin accepts any of the streaming types.
        for (const DirectShowMediaType &t : qAsConst(m_supportedMediaTypes)) {
            hr = pReceivePin->ReceiveConnection(this, &t);
            if (SUCCEEDED(hr)) {
                m_connectionMediaType = t;
                break;
            }
        }
    }

    if (SUCCEEDED(hr) && m_queriedForAsyncReader) {
        m_peerPin = pReceivePin;
        m_peerPin->AddRef();
    } else {
        pReceivePin->Disconnect();
        if (m_allocator) {
            m_allocator->Release();
            m_allocator = 0;
        }
        if (!m_queriedForAsyncReader)
            hr = VFW_E_NO_TRANSPORT;

        m_connectionMediaType.clear();
    }

    return hr;
}

HRESULT DirectShowIOSource::ReceiveConnection(IPin *pConnector, const AM_MEDIA_TYPE *pmt)
{
    Q_UNUSED(pConnector);
    Q_UNUSED(pmt);
    // Output pin.
    return E_NOTIMPL;
}

HRESULT DirectShowIOSource::Disconnect()
{
    QMutexLocker locker(&m_mutex);

    if (!m_peerPin) {
        return S_FALSE;
    } else if (m_state != State_Stopped) {
        return VFW_E_NOT_STOPPED;
    } else {
        HRESULT hr = m_peerPin->Disconnect();

        if (!SUCCEEDED(hr))
            return hr;

        if (m_allocator) {
            m_allocator->Release();
            m_allocator = 0;
        }

        m_peerPin->Release();
        m_peerPin = 0;

        return S_OK;
    }
}

HRESULT DirectShowIOSource::ConnectedTo(IPin **ppPin)
{
    if (!ppPin) {
        return E_POINTER;
    } else {
        QMutexLocker locker(&m_mutex);

        if (!m_peerPin) {
            *ppPin = 0;

            return VFW_E_NOT_CONNECTED;
        } else {
            m_peerPin->AddRef();

            *ppPin = m_peerPin;

            return S_OK;
        }
    }
}

HRESULT DirectShowIOSource::ConnectionMediaType(AM_MEDIA_TYPE *pmt)
{
    if (!pmt) {
        return E_POINTER;
    } else {
        QMutexLocker locker(&m_mutex);

        if (!m_peerPin) {
            pmt = 0;

            return VFW_E_NOT_CONNECTED;
        } else {
            DirectShowMediaType::copy(pmt, m_connectionMediaType);

            return S_OK;
        }
    }
}

HRESULT DirectShowIOSource::QueryPinInfo(PIN_INFO *pInfo)
{
    if (!pInfo) {
        return E_POINTER;
    } else {
        AddRef();

        pInfo->pFilter = this;
        pInfo->dir = PINDIR_OUTPUT;

        const int bytes = qMin(MAX_FILTER_NAME, (m_pinId.length() + 1) * 2);

        ::memcpy(pInfo->achName, m_pinId.utf16(), bytes);

        return S_OK;
    }
}

HRESULT DirectShowIOSource::QueryId(LPWSTR *Id)
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

HRESULT DirectShowIOSource::QueryAccept(const AM_MEDIA_TYPE *pmt)
{
    if (!pmt) {
        return E_POINTER;
    } else if (pmt->majortype == MEDIATYPE_Stream) {
        return S_OK;
    } else {
        return S_FALSE;
    }
}

HRESULT DirectShowIOSource::EnumMediaTypes(IEnumMediaTypes **ppEnum)
{
    if (!ppEnum) {
        return E_POINTER;
    } else {
        *ppEnum = new DirectShowMediaTypeEnum(m_supportedMediaTypes);

        return S_OK;
    }
}

HRESULT DirectShowIOSource::QueryInternalConnections(IPin **apPin, ULONG *nPin)
{
    Q_UNUSED(apPin);
    Q_UNUSED(nPin);

    return E_NOTIMPL;
}

HRESULT DirectShowIOSource::EndOfStream()
{
    return S_OK;
}

HRESULT DirectShowIOSource::BeginFlush()
{
    return m_reader->BeginFlush();
}

HRESULT DirectShowIOSource::EndFlush()
{
    return m_reader->EndFlush();
}

HRESULT DirectShowIOSource::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
    Q_UNUSED(tStart);
    Q_UNUSED(tStop);
    Q_UNUSED(dRate);

    return S_OK;
}

HRESULT DirectShowIOSource::QueryDirection(PIN_DIRECTION *pPinDir)
{
    if (!pPinDir) {
        return E_POINTER;
    } else {
        *pPinDir = PINDIR_OUTPUT;

        return S_OK;
    }
}
