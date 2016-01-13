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

#ifndef DIRECTSHOWGLOBAL_H
#define DIRECTSHOWGLOBAL_H

#include <QtCore/qglobal.h>

#include <dshow.h>

template <typename T> T *com_cast(IUnknown *unknown, const IID &iid)
{
    T *iface = 0;
    return unknown && unknown->QueryInterface(iid, reinterpret_cast<void **>(&iface)) == S_OK
        ? iface
        : 0;
}

template <typename T> T *com_new(const IID &clsid, const IID &iid)
{
    T *object = 0;
    return CoCreateInstance(
            clsid,
            NULL,
            CLSCTX_INPROC_SERVER,
            iid,
            reinterpret_cast<void **>(&object)) == S_OK
        ? object
        : 0;
}

#ifndef __IFilterGraph2_INTERFACE_DEFINED__
#define __IFilterGraph2_INTERFACE_DEFINED__
#define INTERFACE IFilterGraph2
DECLARE_INTERFACE_(IFilterGraph2 ,IGraphBuilder)
{
    STDMETHOD(AddSourceFilterForMoniker)(THIS_ IMoniker *, IBindCtx *, LPCWSTR,IBaseFilter **) PURE;
    STDMETHOD(ReconnectEx)(THIS_ IPin *, const AM_MEDIA_TYPE *) PURE;
    STDMETHOD(RenderEx)(IPin *, DWORD, DWORD *) PURE;
};
#undef INTERFACE
#endif

#ifndef __IAMFilterMiscFlags_INTERFACE_DEFINED__
#define __IAMFilterMiscFlags_INTERFACE_DEFINED__
#define INTERFACE IAMFilterMiscFlags
DECLARE_INTERFACE_(IAMFilterMiscFlags ,IUnknown)
{
    STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;
    STDMETHOD_(ULONG,GetMiscFlags)(THIS) PURE;
};
#undef INTERFACE
#endif

#ifndef __IFileSourceFilter_INTERFACE_DEFINED__
#define __IFileSourceFilter_INTERFACE_DEFINED__
#define INTERFACE IFileSourceFilter
DECLARE_INTERFACE_(IFileSourceFilter ,IUnknown)
{
    STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;
    STDMETHOD(Load)(THIS_ LPCOLESTR, const AM_MEDIA_TYPE *) PURE;
    STDMETHOD(GetCurFile)(THIS_ LPOLESTR *ppszFileName, AM_MEDIA_TYPE *) PURE;
};
#undef INTERFACE
#endif

#ifndef __IAMOpenProgress_INTERFACE_DEFINED__
#define __IAMOpenProgress_INTERFACE_DEFINED__
#define INTERFACE IAMOpenProgress
DECLARE_INTERFACE_(IAMOpenProgress ,IUnknown)
{
    STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;
    STDMETHOD(QueryProgress)(THIS_ LONGLONG *, LONGLONG *) PURE;
    STDMETHOD(AbortOperation)(THIS) PURE;
};
#undef INTERFACE
#endif

#ifndef __IFilterChain_INTERFACE_DEFINED__
#define __IFilterChain_INTERFACE_DEFINED__
#define INTERFACE IFilterChain
DECLARE_INTERFACE_(IFilterChain ,IUnknown)
{
    STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;
    STDMETHOD(StartChain)(IBaseFilter *, IBaseFilter *) PURE;
    STDMETHOD(PauseChain)(IBaseFilter *, IBaseFilter *) PURE;
    STDMETHOD(StopChain)(IBaseFilter *, IBaseFilter *) PURE;
    STDMETHOD(RemoveChain)(IBaseFilter *, IBaseFilter *) PURE;
};
#undef INTERFACE
#endif

#endif
