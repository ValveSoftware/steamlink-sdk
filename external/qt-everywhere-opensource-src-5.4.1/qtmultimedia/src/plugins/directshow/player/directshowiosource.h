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

#ifndef DIRECTSHOWIOSOURCE_H
#define DIRECTSHOWIOSOURCE_H

#include "directshowglobal.h"
#include "directshowioreader.h"
#include "directshowmediatype.h"
#include "directshowmediatypelist.h"

#include <QtCore/qfile.h>

class DirectShowIOSource
    : public DirectShowMediaTypeList
    , public IBaseFilter
    , public IAMFilterMiscFlags
    , public IPin
{
public:
    DirectShowIOSource(DirectShowEventLoop *loop);
    ~DirectShowIOSource();

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
    HRESULT tryConnect(IPin *pin, const AM_MEDIA_TYPE *type);

    volatile LONG m_ref;
    FILTER_STATE m_state;
    DirectShowIOReader *m_reader;
    DirectShowEventLoop *m_loop;
    IFilterGraph *m_graph;
    IReferenceClock *m_clock;
    IMemAllocator *m_allocator;
    IPin *m_peerPin;
    DirectShowMediaType m_mediaType;
    QString m_filterName;
    const QString m_pinId;
    QMutex m_mutex;
};

class DirectShowRcSource : public DirectShowIOSource
{
public:
    DirectShowRcSource(DirectShowEventLoop *loop);

    bool open(const QUrl &url);

private:
    QFile m_file;
};

#endif
