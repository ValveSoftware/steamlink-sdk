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

#ifndef DIRECTSHOWIOSOURCE_H
#define DIRECTSHOWIOSOURCE_H

#include "directshowglobal.h"
#include "directshowioreader.h"
#include "directshowmediatype.h"

#include <QtCore/qfile.h>

class DirectShowIOSource
    : public IBaseFilter
    , public IAMFilterMiscFlags
    , public IPin
{
public:
    DirectShowIOSource(DirectShowEventLoop *loop);
    virtual ~DirectShowIOSource();

    void setDevice(QIODevice *device);
    void setAllocator(IMemAllocator *allocator);

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();

    // IPersist
    HRESULT STDMETHODCALLTYPE GetClassID(CLSID *pClassID);

    // IMediaFilter
    HRESULT STDMETHODCALLTYPE Run(REFERENCE_TIME tStart);
    HRESULT STDMETHODCALLTYPE Pause();
    HRESULT STDMETHODCALLTYPE Stop();

    HRESULT STDMETHODCALLTYPE GetState(DWORD dwMilliSecsTimeout, FILTER_STATE *pState);

    HRESULT STDMETHODCALLTYPE SetSyncSource(IReferenceClock *pClock);
    HRESULT STDMETHODCALLTYPE GetSyncSource(IReferenceClock **ppClock);

    // IBaseFilter
    HRESULT STDMETHODCALLTYPE EnumPins(IEnumPins **ppEnum);
    HRESULT STDMETHODCALLTYPE FindPin(LPCWSTR Id, IPin **ppPin);

    HRESULT STDMETHODCALLTYPE JoinFilterGraph(IFilterGraph *pGraph, LPCWSTR pName);

    HRESULT STDMETHODCALLTYPE QueryFilterInfo(FILTER_INFO *pInfo);
    HRESULT STDMETHODCALLTYPE QueryVendorInfo(LPWSTR *pVendorInfo);

    // IAMFilterMiscFlags
    ULONG STDMETHODCALLTYPE GetMiscFlags();

    // IPin
    HRESULT STDMETHODCALLTYPE Connect(IPin *pReceivePin, const AM_MEDIA_TYPE *pmt);
    HRESULT STDMETHODCALLTYPE ReceiveConnection(IPin *pConnector, const AM_MEDIA_TYPE *pmt);
    HRESULT STDMETHODCALLTYPE Disconnect();
    HRESULT STDMETHODCALLTYPE ConnectedTo(IPin **ppPin);

    HRESULT STDMETHODCALLTYPE ConnectionMediaType(AM_MEDIA_TYPE *pmt);

    HRESULT STDMETHODCALLTYPE QueryPinInfo(PIN_INFO *pInfo);
    HRESULT STDMETHODCALLTYPE QueryId(LPWSTR *Id);

    HRESULT STDMETHODCALLTYPE QueryAccept(const AM_MEDIA_TYPE *pmt);

    HRESULT STDMETHODCALLTYPE EnumMediaTypes(IEnumMediaTypes **ppEnum);

    HRESULT STDMETHODCALLTYPE QueryInternalConnections(IPin **apPin, ULONG *nPin);

    HRESULT STDMETHODCALLTYPE EndOfStream();

    HRESULT STDMETHODCALLTYPE BeginFlush();
    HRESULT STDMETHODCALLTYPE EndFlush();

    HRESULT STDMETHODCALLTYPE NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);

    HRESULT STDMETHODCALLTYPE QueryDirection(PIN_DIRECTION *pPinDir);

private:
    volatile LONG m_ref;
    FILTER_STATE m_state;
    DirectShowIOReader *m_reader;
    DirectShowEventLoop *m_loop;
    IFilterGraph *m_graph;
    IReferenceClock *m_clock;
    IMemAllocator *m_allocator;
    IPin *m_peerPin;
    DirectShowMediaType m_connectionMediaType;
    QList<DirectShowMediaType> m_supportedMediaTypes;
    QString m_filterName;
    const QString m_pinId;
    bool m_queriedForAsyncReader;
    QMutex m_mutex;
};

#endif
