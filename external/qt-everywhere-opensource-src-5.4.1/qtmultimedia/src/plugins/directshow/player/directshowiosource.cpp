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

#include "directshowiosource.h"

#include "directshowglobal.h"
#include "directshowmediatype.h"
#include "directshowpinenum.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qurl.h>

static const GUID directshow_subtypes[] =
{
    MEDIASUBTYPE_Avi,
    MEDIASUBTYPE_WAVE,
    MEDIASUBTYPE_NULL
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
{
    QVector<AM_MEDIA_TYPE> mediaTypes;

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
        mediaTypes.append(type);
    }

    setMediaTypes(mediaTypes);
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
    QMutexLocker locker(&m_mutex);
    if (!pReceivePin) {
        return E_POINTER;
    } else if (m_state != State_Stopped) {
        return VFW_E_NOT_STOPPED;
    } else if (m_peerPin) {
        return VFW_E_ALREADY_CONNECTED;
    } else {
        HRESULT hr = VFW_E_TYPE_NOT_ACCEPTED;

        m_peerPin = pReceivePin;
        m_peerPin->AddRef();

        if (!pmt) {
            IEnumMediaTypes *mediaTypes = 0;
            if (pReceivePin->EnumMediaTypes(&mediaTypes) == S_OK) {
                for (AM_MEDIA_TYPE *type = 0;
                        mediaTypes->Next(1, &type, 0) == S_OK;
                        DirectShowMediaType::deleteType(type)) {
                    switch (tryConnect(pReceivePin, type)) {
                        case S_OK:
                            DirectShowMediaType::freeData(type);
                            mediaTypes->Release();
                            return S_OK;
                        case VFW_E_NO_TRANSPORT:
                            hr = VFW_E_NO_TRANSPORT;
                            break;
                        default:
                            break;
                    }
                }
                mediaTypes->Release();
            }
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

                switch (tryConnect(pReceivePin, &type)) {
                case S_OK:
                    return S_OK;
                case VFW_E_NO_TRANSPORT:
                    hr = VFW_E_NO_TRANSPORT;
                    break;
                default:
                    break;
                }
            }
        } else if (pmt->majortype == MEDIATYPE_Stream &&  (hr = tryConnect(pReceivePin, pmt))) {
            return S_OK;
        }

        m_peerPin->Release();
        m_peerPin = 0;

        m_mediaType.clear();

        return hr;
    }
}

HRESULT DirectShowIOSource::tryConnect(IPin *pin, const AM_MEDIA_TYPE *type)
{
    m_mediaType = *type;

    HRESULT hr = pin->ReceiveConnection(this, type);

    if (!SUCCEEDED(hr)) {
        if (m_allocator) {
            m_allocator->Release();
            m_allocator = 0;
        }
    } else if (!m_allocator) {
        hr = VFW_E_NO_TRANSPORT;

        if (IMemInputPin *memPin = com_cast<IMemInputPin>(pin, IID_IMemInputPin)) {
            if ((m_allocator = com_new<IMemAllocator>(CLSID_MemoryAllocator, IID_IMemAllocator))) {
                ALLOCATOR_PROPERTIES properties;
                if (memPin->GetAllocatorRequirements(&properties) == S_OK
                        || m_allocator->GetProperties(&properties) == S_OK) {
                    if (properties.cbAlign == 0)
                        properties.cbAlign = 1;

                    ALLOCATOR_PROPERTIES actualProperties;
                    if (SUCCEEDED(hr = m_allocator->SetProperties(&properties, &actualProperties)))
                        hr = memPin->NotifyAllocator(m_allocator, TRUE);
                }
                if (!SUCCEEDED(hr)) {
                    m_allocator->Release();
                    m_allocator = 0;
                }
            }
            memPin->Release();
        }
        if (!SUCCEEDED(hr))
            pin->Disconnect();
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

        m_mediaType.clear();

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
            DirectShowMediaType::copy(pmt, m_mediaType);

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
        *ppEnum = createMediaTypeEnum();

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

DirectShowRcSource::DirectShowRcSource(DirectShowEventLoop *loop)
    : DirectShowIOSource(loop)
{
}

bool DirectShowRcSource::open(const QUrl &url)
{
    m_file.moveToThread(QCoreApplication::instance()->thread());
    m_file.setFileName(QLatin1Char(':') + url.path());

    if (m_file.open(QIODevice::ReadOnly)) {
        setDevice(&m_file);
        return true;
    } else {
        return false;
    }
}
