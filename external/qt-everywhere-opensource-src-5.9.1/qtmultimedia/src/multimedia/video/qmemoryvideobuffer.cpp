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

#include "qmemoryvideobuffer_p.h"

#include "qabstractvideobuffer_p.h"
#include <qbytearray.h>

QT_BEGIN_NAMESPACE

class QMemoryVideoBufferPrivate : public QAbstractVideoBufferPrivate
{
public:
    QMemoryVideoBufferPrivate()
        : bytesPerLine(0)
        , mapMode(QAbstractVideoBuffer::NotMapped)
    {
    }

    int bytesPerLine;
    QAbstractVideoBuffer::MapMode mapMode;
    QByteArray data;
};

/*!
    \class QMemoryVideoBuffer
    \brief The QMemoryVideoBuffer class provides a system memory allocated video data buffer.
    \internal

    QMemoryVideoBuffer is the default video buffer for allocating system memory.  It may be used to
    allocate memory for a QVideoFrame without implementing your own QAbstractVideoBuffer.
*/

/*!
    Constructs a video buffer with an image stride of \a bytesPerLine from a byte \a array.
*/
QMemoryVideoBuffer::QMemoryVideoBuffer(const QByteArray &array, int bytesPerLine)
    : QAbstractVideoBuffer(*new QMemoryVideoBufferPrivate, NoHandle)
{
    Q_D(QMemoryVideoBuffer);

    d->data = array;
    d->bytesPerLine = bytesPerLine;
}

/*!
    Destroys a system memory allocated video buffer.
*/
QMemoryVideoBuffer::~QMemoryVideoBuffer()
{
}

/*!
    \reimp
*/
QAbstractVideoBuffer::MapMode QMemoryVideoBuffer::mapMode() const
{
    return d_func()->mapMode;
}

/*!
    \reimp
*/
uchar *QMemoryVideoBuffer::map(MapMode mode, int *numBytes, int *bytesPerLine)
{
    Q_D(QMemoryVideoBuffer);

    if (d->mapMode == NotMapped && d->data.data() && mode != NotMapped) {
        d->mapMode = mode;

        if (numBytes)
            *numBytes = d->data.size();

        if (bytesPerLine)
            *bytesPerLine = d->bytesPerLine;

        return reinterpret_cast<uchar *>(d->data.data());
    } else {
        return 0;
    }
}

/*!
    \reimp
*/
void QMemoryVideoBuffer::unmap()
{
    d_func()->mapMode = NotMapped;
}

QT_END_NAMESPACE
