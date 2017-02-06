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

#include "evrhelpers.h"

#ifndef D3DFMT_YV12
#define D3DFMT_YV12 (D3DFORMAT)MAKEFOURCC ('Y', 'V', '1', '2')
#endif
#ifndef D3DFMT_NV12
#define D3DFMT_NV12 (D3DFORMAT)MAKEFOURCC ('N', 'V', '1', '2')
#endif

HRESULT qt_evr_getFourCC(IMFMediaType *type, DWORD *fourCC)
{
    if (!fourCC)
        return E_POINTER;

    HRESULT hr = S_OK;
    GUID guidSubType = GUID_NULL;

    if (SUCCEEDED(hr))
        hr = type->GetGUID(MF_MT_SUBTYPE, &guidSubType);

    if (SUCCEEDED(hr))
        *fourCC = guidSubType.Data1;

    return hr;
}

bool qt_evr_areMediaTypesEqual(IMFMediaType *type1, IMFMediaType *type2)
{
    if (!type1 && !type2)
        return true;
    else if (!type1 || !type2)
        return false;

    DWORD dwFlags = 0;
    HRESULT hr = type1->IsEqual(type2, &dwFlags);

    return (hr == S_OK);
}

HRESULT qt_evr_validateVideoArea(const MFVideoArea& area, UINT32 width, UINT32 height)
{
    float fOffsetX = qt_evr_MFOffsetToFloat(area.OffsetX);
    float fOffsetY = qt_evr_MFOffsetToFloat(area.OffsetY);

    if ( ((LONG)fOffsetX + area.Area.cx > (LONG)width) ||
         ((LONG)fOffsetY + area.Area.cy > (LONG)height) )
        return MF_E_INVALIDMEDIATYPE;
    else
        return S_OK;
}

bool qt_evr_isSampleTimePassed(IMFClock *clock, IMFSample *sample)
{
    if (!sample || !clock)
        return false;

    HRESULT hr = S_OK;
    MFTIME hnsTimeNow = 0;
    MFTIME hnsSystemTime = 0;
    MFTIME hnsSampleStart = 0;
    MFTIME hnsSampleDuration = 0;

    hr = clock->GetCorrelatedTime(0, &hnsTimeNow, &hnsSystemTime);

    if (SUCCEEDED(hr))
        hr = sample->GetSampleTime(&hnsSampleStart);

    if (SUCCEEDED(hr))
        hr = sample->GetSampleDuration(&hnsSampleDuration);

    if (SUCCEEDED(hr)) {
        if (hnsSampleStart + hnsSampleDuration < hnsTimeNow)
            return true;
    }

    return false;
}

QVideoFrame::PixelFormat qt_evr_pixelFormatFromD3DFormat(D3DFORMAT format)
{
    switch (format) {
    case D3DFMT_R8G8B8:
        return QVideoFrame::Format_RGB24;
    case D3DFMT_A8R8G8B8:
        return QVideoFrame::Format_ARGB32;
    case D3DFMT_X8R8G8B8:
        return QVideoFrame::Format_RGB32;
    case D3DFMT_R5G6B5:
        return QVideoFrame::Format_RGB565;
    case D3DFMT_X1R5G5B5:
        return QVideoFrame::Format_RGB555;
    case D3DFMT_A8:
        return QVideoFrame::Format_Y8;
    case D3DFMT_A8B8G8R8:
        return QVideoFrame::Format_BGRA32;
    case D3DFMT_X8B8G8R8:
        return QVideoFrame::Format_BGR32;
    case D3DFMT_UYVY:
        return QVideoFrame::Format_UYVY;
    case D3DFMT_YUY2:
        return QVideoFrame::Format_YUYV;
    case D3DFMT_NV12:
        return QVideoFrame::Format_NV12;
    case D3DFMT_YV12:
        return QVideoFrame::Format_YV12;
    case D3DFMT_UNKNOWN:
    default:
        return QVideoFrame::Format_Invalid;
    }
}

D3DFORMAT qt_evr_D3DFormatFromPixelFormat(QVideoFrame::PixelFormat format)
{
    switch (format) {
    case QVideoFrame::Format_RGB24:
        return D3DFMT_R8G8B8;
    case QVideoFrame::Format_ARGB32:
        return D3DFMT_A8R8G8B8;
    case QVideoFrame::Format_RGB32:
        return D3DFMT_X8R8G8B8;
    case QVideoFrame::Format_RGB565:
        return D3DFMT_R5G6B5;
    case QVideoFrame::Format_RGB555:
        return D3DFMT_X1R5G5B5;
    case QVideoFrame::Format_Y8:
        return D3DFMT_A8;
    case QVideoFrame::Format_BGRA32:
        return D3DFMT_A8B8G8R8;
    case QVideoFrame::Format_BGR32:
        return D3DFMT_X8B8G8R8;
    case QVideoFrame::Format_UYVY:
        return D3DFMT_UYVY;
    case QVideoFrame::Format_YUYV:
        return D3DFMT_YUY2;
    case QVideoFrame::Format_NV12:
        return D3DFMT_NV12;
    case QVideoFrame::Format_YV12:
        return D3DFMT_YV12;
    case QVideoFrame::Format_Invalid:
    default:
        return D3DFMT_UNKNOWN;
    }
}
