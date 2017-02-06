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

#ifndef EVRHELPERS_H
#define EVRHELPERS_H

#include "evrdefs.h"
#include <qvideoframe.h>

QT_USE_NAMESPACE

template<class T>
static inline void qt_evr_safe_release(T **unk)
{
    if (*unk) {
        (*unk)->Release();
        *unk = NULL;
    }
}

HRESULT qt_evr_getFourCC(IMFMediaType *type, DWORD *fourCC);

bool qt_evr_areMediaTypesEqual(IMFMediaType *type1, IMFMediaType *type2);

HRESULT qt_evr_validateVideoArea(const MFVideoArea& area, UINT32 width, UINT32 height);

bool qt_evr_isSampleTimePassed(IMFClock *clock, IMFSample *sample);

inline float qt_evr_MFOffsetToFloat(const MFOffset& offset)
{
    return offset.value + (float(offset.fract) / 65536);
}

inline MFOffset qt_evr_makeMFOffset(float v)
{
    MFOffset offset;
    offset.value = short(v);
    offset.fract = WORD(65536 * (v-offset.value));
    return offset;
}

inline MFVideoArea qt_evr_makeMFArea(float x, float y, DWORD width, DWORD height)
{
    MFVideoArea area;
    area.OffsetX = qt_evr_makeMFOffset(x);
    area.OffsetY = qt_evr_makeMFOffset(y);
    area.Area.cx = width;
    area.Area.cy = height;
    return area;
}

inline HRESULT qt_evr_getFrameRate(IMFMediaType *pType, MFRatio *pRatio)
{
    return MFGetAttributeRatio(pType, MF_MT_FRAME_RATE, (UINT32*)&pRatio->Numerator, (UINT32*)&pRatio->Denominator);
}

QVideoFrame::PixelFormat qt_evr_pixelFormatFromD3DFormat(D3DFORMAT format);
D3DFORMAT qt_evr_D3DFormatFromPixelFormat(QVideoFrame::PixelFormat format);

#endif // EVRHELPERS_H

