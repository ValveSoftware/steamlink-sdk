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

#include "mfglobal.h"

HRESULT qt_wmf_getFourCC(IMFMediaType *type, DWORD *fourCC)
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

MFRatio qt_wmf_getPixelAspectRatio(IMFMediaType *type)
{
    MFRatio ratio = { 0, 0 };
    HRESULT hr = S_OK;

    hr = MFGetAttributeRatio(type, MF_MT_PIXEL_ASPECT_RATIO, (UINT32*)&ratio.Numerator, (UINT32*)&ratio.Denominator);
    if (FAILED(hr)) {
        ratio.Numerator = 1;
        ratio.Denominator = 1;
    }
    return ratio;
}

bool qt_wmf_areMediaTypesEqual(IMFMediaType *type1, IMFMediaType *type2)
{
    if (!type1 && !type2)
        return true;
    else if (!type1 || !type2)
        return false;

    DWORD dwFlags = 0;
    HRESULT hr = type1->IsEqual(type2, &dwFlags);

    return (hr == S_OK);
}

HRESULT qt_wmf_validateVideoArea(const MFVideoArea& area, UINT32 width, UINT32 height)
{
    float fOffsetX = qt_wmf_MFOffsetToFloat(area.OffsetX);
    float fOffsetY = qt_wmf_MFOffsetToFloat(area.OffsetY);

    if ( ((LONG)fOffsetX + area.Area.cx > (LONG)width) ||
         ((LONG)fOffsetY + area.Area.cy > (LONG)height) )
        return MF_E_INVALIDMEDIATYPE;
    else
        return S_OK;
}

bool qt_wmf_isSampleTimePassed(IMFClock *clock, IMFSample *sample)
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
