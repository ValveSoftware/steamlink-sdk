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

#ifndef DIRECTSHOWBASEFILTER_H
#define DIRECTSHOWBASEFILTER_H

#include "directshowpin.h"

QT_USE_NAMESPACE

class DirectShowBaseFilter : public DirectShowObject
                           , public IBaseFilter
{
    DIRECTSHOW_OBJECT

public:
    DirectShowBaseFilter();
    virtual ~DirectShowBaseFilter();

    FILTER_STATE state() const { return m_state; }
    HRESULT NotifyEvent(long eventCode, LONG_PTR eventParam1, LONG_PTR eventParam2);

    virtual QList<DirectShowPin *> pins() = 0;

    // DirectShowObject
    HRESULT getInterface(const IID &riid, void **ppvObject);

    // IPersist
    STDMETHODIMP GetClassID(CLSID *pClassID);

    // IMediaFilter
    STDMETHODIMP Run(REFERENCE_TIME tStart);
    STDMETHODIMP Pause();
    STDMETHODIMP Stop();

    STDMETHODIMP GetState(DWORD dwMilliSecsTimeout, FILTER_STATE *pState);

    STDMETHODIMP SetSyncSource(IReferenceClock *pClock);
    STDMETHODIMP GetSyncSource(IReferenceClock **ppClock);

    // IBaseFilter
    STDMETHODIMP EnumPins(IEnumPins **ppEnum);
    STDMETHODIMP FindPin(LPCWSTR Id, IPin **ppPin);

    STDMETHODIMP JoinFilterGraph(IFilterGraph *pGraph, LPCWSTR pName);

    STDMETHODIMP QueryFilterInfo(FILTER_INFO *pInfo);
    STDMETHODIMP QueryVendorInfo(LPWSTR *pVendorInfo);

protected:
    QMutex m_mutex;
    FILTER_STATE m_state;
    IFilterGraph *m_graph;
    IReferenceClock *m_clock;
    IMediaEventSink *m_sink;
    QString m_filterName;
    REFERENCE_TIME m_startTime;

private:
    Q_DISABLE_COPY(DirectShowBaseFilter)
};

#endif // DIRECTSHOWBASEFILTER_H
