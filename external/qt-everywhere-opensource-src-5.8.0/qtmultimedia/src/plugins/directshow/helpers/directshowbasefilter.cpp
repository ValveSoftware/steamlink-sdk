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

#include "directshowbasefilter.h"

#include "directshowpinenum.h"

DirectShowBaseFilter::DirectShowBaseFilter()
    : m_mutex(QMutex::Recursive)
    , m_state(State_Stopped)
    , m_graph(NULL)
    , m_clock(NULL)
    , m_sink(NULL)
{

}

DirectShowBaseFilter::~DirectShowBaseFilter()
{
    if (m_clock) {
        m_clock->Release();
        m_clock = NULL;
    }
}

HRESULT DirectShowBaseFilter::getInterface(REFIID riid, void **ppvObject)
{
    if (riid == IID_IPersist
            || riid == IID_IMediaFilter
            || riid == IID_IBaseFilter) {
        return GetInterface(static_cast<IBaseFilter *>(this), ppvObject);
    } else {
        return DirectShowObject::getInterface(riid, ppvObject);
    }
}

HRESULT DirectShowBaseFilter::GetClassID(CLSID *pClassID)
{
    *pClassID = CLSID_NULL;
    return S_OK;
}

HRESULT DirectShowBaseFilter::NotifyEvent(long eventCode, LONG_PTR eventParam1, LONG_PTR eventParam2)
{
    IMediaEventSink *sink = m_sink;
    if (sink) {
        if (eventCode == EC_COMPLETE)
            eventParam2 = (LONG_PTR)(IBaseFilter*)this;

        return sink->Notify(eventCode, eventParam1, eventParam2);
    } else {
        return E_NOTIMPL;
    }
}

HRESULT DirectShowBaseFilter::Run(REFERENCE_TIME tStart)
{
    Q_UNUSED(tStart)
    QMutexLocker locker(&m_mutex);

    m_startTime = tStart;

    if (m_state == State_Stopped){
        HRESULT hr = Pause();
        if (FAILED(hr))
            return hr;
    }

    m_state = State_Running;

    return S_OK;
}

HRESULT DirectShowBaseFilter::Pause()
{
    QMutexLocker locker(&m_mutex);

    if (m_state == State_Stopped) {
        const QList<DirectShowPin *> pinList = pins();
        for (DirectShowPin *pin : pinList) {
            if (pin->isConnected()) {
                HRESULT hr = pin->setActive(true);
                if (FAILED(hr))
                    return hr;
            }
        }
    }

    m_state = State_Paused;

    return S_OK;
}

HRESULT DirectShowBaseFilter::Stop()
{
    QMutexLocker locker(&m_mutex);

    HRESULT hr = S_OK;

    if (m_state != State_Stopped) {
        const QList<DirectShowPin *> pinList = pins();
        for (DirectShowPin *pin : pinList) {
            if (pin->isConnected()) {
                HRESULT hrTmp = pin->setActive(false);
                if (FAILED(hrTmp) && SUCCEEDED(hr))
                    hr = hrTmp;
            }
        }
    }

    m_state = State_Stopped;

    return hr;
}

HRESULT DirectShowBaseFilter::GetState(DWORD dwMilliSecsTimeout, FILTER_STATE *pState)
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

HRESULT DirectShowBaseFilter::SetSyncSource(IReferenceClock *pClock)
{
    QMutexLocker locker(&m_mutex);

    if (m_clock)
        m_clock->Release();

    m_clock = pClock;

    if (m_clock)
        m_clock->AddRef();

    return S_OK;
}

HRESULT DirectShowBaseFilter::GetSyncSource(IReferenceClock **ppClock)
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

HRESULT DirectShowBaseFilter::EnumPins(IEnumPins **ppEnum)
{
    if (!ppEnum) {
        return E_POINTER;
    } else {
        *ppEnum = new DirectShowPinEnum(this);
        return S_OK;
    }
}

HRESULT DirectShowBaseFilter::FindPin(LPCWSTR Id, IPin **ppPin)
{
    if (!ppPin || !Id) {
        return E_POINTER;
    } else {
        QMutexLocker locker(&m_mutex);
        const QList<DirectShowPin *> pinList = pins();
        for (DirectShowPin *pin : pinList) {
            if (QString::fromWCharArray(Id) == pin->name()) {
                pin->AddRef();
                *ppPin = pin;
                return S_OK;
            }
        }

        *ppPin = 0;
        return VFW_E_NOT_FOUND;
    }
}

HRESULT DirectShowBaseFilter::JoinFilterGraph(IFilterGraph *pGraph, LPCWSTR pName)
{
    QMutexLocker locker(&m_mutex);

    m_filterName = QString::fromWCharArray(pName);
    m_graph = pGraph;
    m_sink = NULL;

    if (m_graph) {
        if (SUCCEEDED(m_graph->QueryInterface(IID_PPV_ARGS(&m_sink))))
            m_sink->Release(); // we don't keep a reference on it
    }

    return S_OK;
}

HRESULT DirectShowBaseFilter::QueryFilterInfo(FILTER_INFO *pInfo)
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

HRESULT DirectShowBaseFilter::QueryVendorInfo(LPWSTR *pVendorInfo)
{
    Q_UNUSED(pVendorInfo);
    return E_NOTIMPL;
}
