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

#include "qaudiohelpers_p.h"

#include <QDebug>

QT_BEGIN_NAMESPACE

namespace QAudioHelperInternal
{

template<class T> void adjustSamples(qreal factor, const void *src, void *dst, int samples)
{
    const T *pSrc = (const T *)src;
    T *pDst = (T*)dst;
    for ( int i = 0; i < samples; i++ )
        pDst[i] = pSrc[i] * factor;
}

// Unsigned samples are biased around 0x80/0x8000 :/
// This makes a pure template solution a bit unwieldy but possible
template<class T> struct signedVersion {};
template<> struct signedVersion<quint8>
{
    typedef qint8 TS;
    enum {offset = 0x80};
};

template<> struct signedVersion<quint16>
{
    typedef qint16 TS;
    enum {offset = 0x8000};
};

template<> struct signedVersion<quint32>
{
    typedef qint32 TS;
    enum {offset = 0x80000000};
};

template<class T> void adjustUnsignedSamples(qreal factor, const void *src, void *dst, int samples)
{
    const T *pSrc = (const T *)src;
    T *pDst = (T*)dst;
    for ( int i = 0; i < samples; i++ ) {
        pDst[i] = signedVersion<T>::offset + ((typename signedVersion<T>::TS)(pSrc[i] - signedVersion<T>::offset) * factor);
    }
}

void qMultiplySamples(qreal factor, const QAudioFormat &format, const void* src, void* dest, int len)
{
    int samplesCount = len / (format.sampleSize()/8);

    switch ( format.sampleSize() ) {
    case 8:
        if (format.sampleType() == QAudioFormat::SignedInt)
            QAudioHelperInternal::adjustSamples<qint8>(factor,src,dest,samplesCount);
        else if (format.sampleType() == QAudioFormat::UnSignedInt)
            QAudioHelperInternal::adjustUnsignedSamples<quint8>(factor,src,dest,samplesCount);
        break;
    case 16:
        if (format.sampleType() == QAudioFormat::SignedInt)
            QAudioHelperInternal::adjustSamples<qint16>(factor,src,dest,samplesCount);
        else if (format.sampleType() == QAudioFormat::UnSignedInt)
            QAudioHelperInternal::adjustUnsignedSamples<quint16>(factor,src,dest,samplesCount);
        break;
    default:
        if (format.sampleType() == QAudioFormat::SignedInt)
            QAudioHelperInternal::adjustSamples<qint32>(factor,src,dest,samplesCount);
        else if (format.sampleType() == QAudioFormat::UnSignedInt)
            QAudioHelperInternal::adjustUnsignedSamples<quint32>(factor,src,dest,samplesCount);
        else if (format.sampleType() == QAudioFormat::Float)
            QAudioHelperInternal::adjustSamples<float>(factor,src,dest,samplesCount);
    }
}
}

QT_END_NAMESPACE
