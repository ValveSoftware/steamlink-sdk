/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
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
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "directshowpin.h"

#include "directshowmediatype.h"
#include "directshowbasefilter.h"
#include "directshowmediatypeenum.h"

#include <qdebug.h>

DirectShowPin::DirectShowPin(DirectShowBaseFilter *filter, const QString &name, PIN_DIRECTION direction)
    : m_mutex(QMutex::Recursive)
    , m_filter(filter)
    , m_name(name)
    , m_direction(direction)
    , m_peerPin(NULL)
{
}

DirectShowPin::~DirectShowPin()
{

}

HRESULT DirectShowPin::getInterface(const IID &riid, void **ppvObject)
{
    if (riid == IID_IPin)
        return GetInterface(static_cast<IPin*>(this), ppvObject);
    else
        return DirectShowObject::getInterface(riid, ppvObject);
}



HRESULT DirectShowPin::Connect(IPin *pReceivePin, const AM_MEDIA_TYPE *pmt)
{
    if (!pReceivePin)
        return E_POINTER;

    HRESULT hr = E_FAIL;
    QMutexLocker locker(&m_mutex);

    if (m_peerPin)
        return VFW_E_ALREADY_CONNECTED;
    if (m_filter->state() != State_Stopped)
        return VFW_E_NOT_STOPPED;

    PIN_DIRECTION pd;
    pReceivePin->QueryDirection(&pd);
    if (pd == m_direction)
        return VFW_E_INVALID_DIRECTION;

    const DirectShowMediaType *type = reinterpret_cast<const DirectShowMediaType*>(pmt);

    if (type != NULL && !type->isPartiallySpecified()) {
        // If the type is fully specified, use it
        hr = tryConnect(pReceivePin, type);
    } else {
        IEnumMediaTypes *enumMediaTypes = NULL;

        // First, try the receiving pin's preferred types
        if (SUCCEEDED(pReceivePin->EnumMediaTypes(&enumMediaTypes))) {
            hr = tryMediaTypes(pReceivePin, type, enumMediaTypes);
            enumMediaTypes->Release();
        }
        // Then, try this pin's preferred types
        if (FAILED(hr) && SUCCEEDED(EnumMediaTypes(&enumMediaTypes))) {
            hr = tryMediaTypes(pReceivePin, type, enumMediaTypes);
            enumMediaTypes->Release();
        }
    }

    if (FAILED(hr)) {
        return ((hr != E_FAIL) && (hr != E_INVALIDARG) && (hr != VFW_E_TYPE_NOT_ACCEPTED))
                ? hr : VFW_E_NO_ACCEPTABLE_TYPES;
    }

    return S_OK;
}

HRESULT DirectShowPin::tryMediaTypes(IPin *pin, const DirectShowMediaType *partialType, IEnumMediaTypes *enumMediaTypes)
{
    HRESULT hr = enumMediaTypes->Reset();
    if (FAILED(hr))
        return hr;

    DirectShowMediaType *mediaType = NULL;
    ULONG mediaCount = 0;
    HRESULT hrFailure = VFW_E_NO_ACCEPTABLE_TYPES;

    for (; enumMediaTypes->Next(1, reinterpret_cast<AM_MEDIA_TYPE**>(&mediaType), &mediaCount) == S_OK;) {

        if (mediaType && (partialType == NULL || mediaType->isCompatibleWith(partialType))) {
            hr = tryConnect(pin, mediaType);

            if (FAILED(hr) && (hr != E_FAIL)
                    && (hr != E_INVALIDARG)
                    && (hr != VFW_E_TYPE_NOT_ACCEPTED)) {
                hrFailure = hr;
            }
        }

        if (mediaType)
            DirectShowMediaType::deleteType(mediaType);

        if (SUCCEEDED(hr))
            return S_OK;
    }

    return hrFailure;
}

HRESULT DirectShowPin::tryConnect(IPin *pin, const DirectShowMediaType *type)
{
    if (!isMediaTypeSupported(type))
        return VFW_E_TYPE_NOT_ACCEPTED;

    m_peerPin = pin;
    m_peerPin->AddRef();

    HRESULT hr;
    if (!setMediaType(type)) {
        hr = VFW_E_TYPE_NOT_ACCEPTED;
    } else {
        hr = pin->ReceiveConnection(this, type);
        if (SUCCEEDED(hr)) {
            hr = completeConnection(pin);
            if (FAILED(hr))
                pin->Disconnect();
        }
    }

    if (FAILED(hr)) {
        connectionEnded();
        m_peerPin->Release();
        m_peerPin = NULL;
        setMediaType(NULL);
        return hr;
    }

    return S_OK;
}

HRESULT DirectShowPin::ReceiveConnection(IPin *pConnector, const AM_MEDIA_TYPE *pmt)
{
    if (!pConnector || !pmt)
        return E_POINTER;

    QMutexLocker locker(&m_mutex);

    if (m_peerPin)
        return VFW_E_ALREADY_CONNECTED;
    if (m_filter->state() != State_Stopped)
        return VFW_E_NOT_STOPPED;

    PIN_DIRECTION pd;
    pConnector->QueryDirection(&pd);
    if (pd == m_direction)
        return VFW_E_INVALID_DIRECTION;

    const DirectShowMediaType *type = reinterpret_cast<const DirectShowMediaType*>(pmt);
    if (!isMediaTypeSupported(type))
        return VFW_E_TYPE_NOT_ACCEPTED;

    m_peerPin = pConnector;
    m_peerPin->AddRef();

    HRESULT hr;
    if (!setMediaType(type))
        hr = VFW_E_TYPE_NOT_ACCEPTED;
    else
        hr = completeConnection(pConnector);

    if (FAILED(hr)) {
        connectionEnded();
        m_peerPin->Release();
        m_peerPin = NULL;
        setMediaType(NULL);
        return hr;
    }

    return S_OK;
}

HRESULT DirectShowPin::Disconnect()
{
    QMutexLocker locker(&m_mutex);

    if (m_filter->state() != State_Stopped)
        return VFW_E_NOT_STOPPED;

    if (m_peerPin) {
        HRESULT hr = connectionEnded();
        if (FAILED(hr))
            return hr;

        m_peerPin->Release();
        m_peerPin = NULL;

        setMediaType(NULL);

        return S_OK;
    }

    return S_FALSE;
}

HRESULT DirectShowPin::ConnectedTo(IPin **ppPin)
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

HRESULT DirectShowPin::ConnectionMediaType(AM_MEDIA_TYPE *pmt)
{
    if (!pmt) {
        return E_POINTER;
    } else {
        QMutexLocker locker(&m_mutex);

        if (!m_peerPin) {
            DirectShowMediaType::init(pmt);
            return VFW_E_NOT_CONNECTED;
        } else {
            DirectShowMediaType::copy(pmt, m_mediaType);
            return S_OK;
        }
    }
}

HRESULT DirectShowPin::QueryPinInfo(PIN_INFO *pInfo)
{
    if (!pInfo) {
        return E_POINTER;
    } else {
        pInfo->pFilter = m_filter;
        if (m_filter) {
            m_filter->AddRef();
        }
        pInfo->dir = m_direction;

        QString name = m_name;
        if (name.length() >= MAX_PIN_NAME)
            name.truncate(MAX_PIN_NAME - 1);
        int length = name.toWCharArray(pInfo->achName);
        pInfo->achName[length] = '\0';

        return S_OK;
    }
}

HRESULT DirectShowPin::QueryId(LPWSTR *Id)
{
    if (!Id) {
        return E_POINTER;
    } else {
        const int bytes = (m_name.length() + 1) * 2;
        *Id = static_cast<LPWSTR>(::CoTaskMemAlloc(bytes));
        ::memcpy(*Id, m_name.utf16(), bytes);
        return S_OK;
    }
}

HRESULT DirectShowPin::QueryAccept(const AM_MEDIA_TYPE *pmt)
{
    if (!pmt)
        return E_POINTER;

    if (!isMediaTypeSupported(reinterpret_cast<const DirectShowMediaType*>(pmt)))
        return S_FALSE;

    return S_OK;
}

HRESULT DirectShowPin::EnumMediaTypes(IEnumMediaTypes **ppEnum)
{
    if (!ppEnum) {
        return E_POINTER;
    } else {
        *ppEnum = new DirectShowMediaTypeEnum(this);
        return S_OK;
    }
}

HRESULT DirectShowPin::QueryInternalConnections(IPin **apPin, ULONG *nPin)
{
    Q_UNUSED(apPin);
    Q_UNUSED(nPin);
    return E_NOTIMPL;
}

HRESULT DirectShowPin::EndOfStream()
{
    return S_OK;
}

HRESULT DirectShowPin::BeginFlush()
{
    return E_UNEXPECTED;
}

HRESULT DirectShowPin::EndFlush()
{
    return E_UNEXPECTED;
}

HRESULT DirectShowPin::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
    Q_UNUSED(tStart);
    Q_UNUSED(tStop);
    Q_UNUSED(dRate);
    return S_OK;
}

HRESULT DirectShowPin::QueryDirection(PIN_DIRECTION *pPinDir)
{
    if (!pPinDir) {
        return E_POINTER;
    } else {
        *pPinDir = m_direction;
        return S_OK;
    }
}

QList<DirectShowMediaType> DirectShowPin::supportedMediaTypes()
{
    return QList<DirectShowMediaType>();
}

bool DirectShowPin::setMediaType(const DirectShowMediaType *type)
{
    if (!type)
        m_mediaType.clear();
    else
        m_mediaType = *type;

    return true;
}

HRESULT DirectShowPin::completeConnection(IPin *pin)
{
    Q_UNUSED(pin)
    return S_OK;
}

HRESULT DirectShowPin::connectionEnded()
{
    return S_OK;
}

HRESULT DirectShowPin::setActive(bool active)
{
    Q_UNUSED(active)
    return S_OK;
}


/* DirectShowOutputPin */

DirectShowOutputPin::DirectShowOutputPin(DirectShowBaseFilter *filter, const QString &name)
    : DirectShowPin(filter, name, PINDIR_OUTPUT)
    , m_allocator(NULL)
    , m_inputPin(NULL)
{

}

DirectShowOutputPin::~DirectShowOutputPin()
{
}

HRESULT DirectShowOutputPin::completeConnection(IPin *pin)
{
    if (!pin)
        return E_POINTER;

    Q_ASSERT(m_inputPin == NULL);
    Q_ASSERT(m_allocator == NULL);

    HRESULT hr = pin->QueryInterface(IID_PPV_ARGS(&m_inputPin));
    if (FAILED(hr))
        return hr;

    ALLOCATOR_PROPERTIES prop;
    ZeroMemory(&prop, sizeof(prop));
    m_inputPin->GetAllocatorRequirements(&prop);
    if (prop.cBuffers <= 0)
        prop.cBuffers = 1;
    if (prop.cbBuffer <= 0)
        prop.cbBuffer = 1;
    if (prop.cbAlign <= 0)
        prop.cbAlign = 1;

    // Use the connected input pin's allocator if it has one
    hr = m_inputPin->GetAllocator(&m_allocator);
    if (SUCCEEDED(hr)) {
        ALLOCATOR_PROPERTIES actualProperties;
        hr = m_allocator->SetProperties(&prop, &actualProperties);

        if (SUCCEEDED(hr)) {
            hr = m_inputPin->NotifyAllocator(m_allocator, FALSE);
            if (SUCCEEDED(hr))
                return S_OK;
        }

        m_allocator->Release();
        m_allocator = NULL;
    }

    // Otherwise, allocate its own allocator
    m_allocator = com_new<IMemAllocator>(CLSID_MemoryAllocator);
    if (!m_allocator) {
        hr = E_OUTOFMEMORY;
    } else {
        ALLOCATOR_PROPERTIES actualProperties;
        hr = m_allocator->SetProperties(&prop, &actualProperties);

        if (SUCCEEDED(hr)) {
            hr = m_inputPin->NotifyAllocator(m_allocator, FALSE);
            if (SUCCEEDED(hr))
                return S_OK;
        }

        m_allocator->Release();
        m_allocator = NULL;
    }

    return hr;
}

HRESULT DirectShowOutputPin::connectionEnded()
{
    if (m_allocator) {
        HRESULT hr = m_allocator->Decommit();
        if (FAILED(hr))
            return hr;

        m_allocator->Release();
        m_allocator = NULL;
    }

    if (m_inputPin) {
        m_inputPin->Release();
        m_inputPin = NULL;
    }

    return S_OK;
}

HRESULT DirectShowOutputPin::setActive(bool active)
{
    if (!m_allocator)
        return VFW_E_NO_ALLOCATOR;

    return active ? m_allocator->Commit()
                  : m_allocator->Decommit();
}

HRESULT DirectShowOutputPin::EndOfStream()
{
    return E_UNEXPECTED;
}


/* DirectShowInputPin */

DirectShowInputPin::DirectShowInputPin(DirectShowBaseFilter *filter, const QString &name)
    : DirectShowPin(filter, name, PINDIR_INPUT)
    , m_allocator(NULL)
    , m_flushing(false)
    , m_inErrorState(false)
{
    ZeroMemory(&m_sampleProperties, sizeof(m_sampleProperties));
}

DirectShowInputPin::~DirectShowInputPin()
{

}

HRESULT DirectShowInputPin::getInterface(const IID &riid, void **ppvObject)
{
    if (riid == IID_IMemInputPin)
        return GetInterface(static_cast<IMemInputPin*>(this), ppvObject);
    else
        return DirectShowPin::getInterface(riid, ppvObject);
}

HRESULT DirectShowInputPin::connectionEnded()
{
    if (m_allocator) {
        HRESULT hr = m_allocator->Decommit();
        if (FAILED(hr))
            return hr;

        m_allocator->Release();
        m_allocator = NULL;
    }

    return S_OK;
}

HRESULT DirectShowInputPin::setActive(bool active)
{
    if (!active) {
        m_inErrorState = false;

        if (!m_allocator)
            return VFW_E_NO_ALLOCATOR;

        m_flushing = false;
        return m_allocator->Decommit();
    }

    return S_OK;
}

HRESULT DirectShowInputPin::EndOfStream()
{
    if (m_filter->state() == State_Stopped)
        return VFW_E_WRONG_STATE;
    if (m_flushing)
        return S_FALSE;
    if (m_inErrorState)
        return VFW_E_RUNTIME_ERROR;

    return S_OK;
}

HRESULT DirectShowInputPin::BeginFlush()
{
    QMutexLocker locker(&m_mutex);
    m_flushing = true;
    return S_OK;
}

HRESULT DirectShowInputPin::EndFlush()
{
    QMutexLocker locker(&m_mutex);
    m_flushing = false;
    m_inErrorState = false;
    return S_OK;
}

HRESULT DirectShowInputPin::GetAllocator(IMemAllocator **ppAllocator)
{
    if (!ppAllocator)
        return E_POINTER;

    QMutexLocker locker(&m_mutex);

    if (!m_allocator) {
        m_allocator = com_new<IMemAllocator>(CLSID_MemoryAllocator);;
        if (!m_allocator)
            return E_OUTOFMEMORY;
    }

    *ppAllocator = m_allocator;
    m_allocator->AddRef();

    return S_OK;
}

HRESULT DirectShowInputPin::NotifyAllocator(IMemAllocator *pAllocator, BOOL bReadOnly)
{
    Q_UNUSED(bReadOnly)

    if (!pAllocator)
        return E_POINTER;

    QMutexLocker locker(&m_mutex);

    if (m_allocator)
        m_allocator->Release();

    m_allocator = pAllocator;
    m_allocator->AddRef();

    return S_OK;
}

HRESULT DirectShowInputPin::GetAllocatorRequirements(ALLOCATOR_PROPERTIES *pProps)
{
    Q_UNUSED(pProps)
    return E_NOTIMPL;
}

HRESULT DirectShowInputPin::Receive(IMediaSample *pSample)
{
    if (!pSample)
        return E_POINTER;
    if (m_filter->state() == State_Stopped)
        return VFW_E_WRONG_STATE;
    if (m_flushing)
        return S_FALSE;
    if (m_inErrorState)
        return VFW_E_RUNTIME_ERROR;

    HRESULT hr = S_OK;

    IMediaSample2 *sample2;
    if (SUCCEEDED(pSample->QueryInterface(IID_PPV_ARGS(&sample2)))) {
        hr = sample2->GetProperties(sizeof(m_sampleProperties), (PBYTE)&m_sampleProperties);
        sample2->Release();
        if (FAILED(hr))
            return hr;
    } else {
        m_sampleProperties.cbData = sizeof(m_sampleProperties);
        m_sampleProperties.dwTypeSpecificFlags = 0;
        m_sampleProperties.dwStreamId = AM_STREAM_MEDIA;
        m_sampleProperties.dwSampleFlags = 0;
        if (pSample->IsDiscontinuity() == S_OK)
            m_sampleProperties.dwSampleFlags |= AM_SAMPLE_DATADISCONTINUITY;
        if (pSample->IsPreroll() == S_OK)
            m_sampleProperties.dwSampleFlags |= AM_SAMPLE_PREROLL;
        if (pSample->IsSyncPoint() == S_OK)
            m_sampleProperties.dwSampleFlags |= AM_SAMPLE_SPLICEPOINT;
        if (SUCCEEDED(pSample->GetTime(&m_sampleProperties.tStart,
                                       &m_sampleProperties.tStop))) {
            m_sampleProperties.dwSampleFlags |= AM_SAMPLE_TIMEVALID | AM_SAMPLE_STOPVALID;
        }
        if (pSample->GetMediaType(&m_sampleProperties.pMediaType) == S_OK)
            m_sampleProperties.dwSampleFlags |= AM_SAMPLE_TYPECHANGED;
        pSample->GetPointer(&m_sampleProperties.pbBuffer);
        m_sampleProperties.lActual = pSample->GetActualDataLength();
        m_sampleProperties.cbBuffer = pSample->GetSize();
    }


    if (!(m_sampleProperties.dwSampleFlags & AM_SAMPLE_TYPECHANGED))
        return S_OK;

    if (isMediaTypeSupported(reinterpret_cast<DirectShowMediaType*>(m_sampleProperties.pMediaType)))
        return S_OK;

    m_inErrorState = true;
    EndOfStream();
    m_filter->NotifyEvent(EC_ERRORABORT, VFW_E_TYPE_NOT_ACCEPTED, 0);
    return VFW_E_INVALIDMEDIATYPE;
}

HRESULT DirectShowInputPin::ReceiveMultiple(IMediaSample **pSamples, long nSamples, long *nSamplesProcessed)
{
    if (!pSamples || !nSamplesProcessed)
        return E_POINTER;

    HRESULT hr = S_OK;
    *nSamplesProcessed = 0;
    while (nSamples-- > 0) {
         hr = Receive(pSamples[*nSamplesProcessed]);
         if (hr != S_OK)
             break;
         (*nSamplesProcessed)++;
    }
    return hr;
}

HRESULT DirectShowInputPin::ReceiveCanBlock()
{
    int outputPins = 0;

    const QList<DirectShowPin *> pinList = m_filter->pins();
    for (DirectShowPin *pin : pinList) {
        PIN_DIRECTION pd;
        HRESULT hr = pin->QueryDirection(&pd);
        if (FAILED(hr))
            return hr;

        if (pd == PINDIR_OUTPUT) {
            IPin *connected;
            hr = pin->ConnectedTo(&connected);
            if (SUCCEEDED(hr)) {
                ++outputPins;
                IMemInputPin *inputPin;
                hr = connected->QueryInterface(IID_PPV_ARGS(&inputPin));
                connected->Release();
                if (SUCCEEDED(hr)) {
                    hr = inputPin->ReceiveCanBlock();
                    inputPin->Release();
                    if (hr != S_FALSE)
                        return S_OK;
                } else {
                    return S_OK;
                }
            }
        }
    }

    return outputPins == 0 ? S_OK : S_FALSE;
}
