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

#include "qvideoframeconversionhelper_p.h"

#ifdef QT_COMPILER_SUPPORTS_SSSE3

QT_BEGIN_NAMESPACE

void QT_FASTCALL qt_convert_BGRA32_to_ARGB32_ssse3(const QVideoFrame &frame, uchar *output)
{
    FETCH_INFO_PACKED(frame)
    MERGE_LOOPS(width, height, stride, 4)
    quint32 *argb = reinterpret_cast<quint32*>(output);

    __m128i shuffleMask = _mm_set_epi8(12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3);

    for (int y = 0; y < height; ++y) {
        const quint32 *bgra = reinterpret_cast<const quint32*>(src);

        int x = 0;
        ALIGN(16, argb, x, width) {
            *argb = qConvertBGRA32ToARGB32(*bgra);
            ++bgra;
            ++argb;
        }

        for (; x < width - 7; x += 8) {
            __m128i pixelData = _mm_loadu_si128(reinterpret_cast<const __m128i*>(bgra));
            __m128i pixelData2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(bgra + 4));
            bgra += 8;
            pixelData = _mm_shuffle_epi8(pixelData, shuffleMask);
            pixelData2 = _mm_shuffle_epi8(pixelData2, shuffleMask);
            _mm_store_si128(reinterpret_cast<__m128i*>(argb), pixelData);
            _mm_store_si128(reinterpret_cast<__m128i*>(argb + 4), pixelData2);
            argb += 8;
        }

        // leftovers
        for (; x < width; ++x) {
            *argb = qConvertBGRA32ToARGB32(*bgra);
            ++bgra;
            ++argb;
        }

        src += stride;
    }
}

QT_END_NAMESPACE

#endif
