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

#include "qimagevideobuffer_p.h"

#include "qabstractvideobuffer_p.h"

#include <qimage.h>
#include <qvariant.h>

QT_BEGIN_NAMESPACE

/*!
 * \class QImageVideoBuffer
 * \internal
 *
 * A video buffer class for a QImage.
 */
class QImageVideoBufferPrivate : public QAbstractVideoBufferPrivate
{
public:
    QImageVideoBufferPrivate()
        : mapMode(QAbstractVideoBuffer::NotMapped)
    {
    }

    QAbstractVideoBuffer::MapMode mapMode;
    QImage image;
};

QImageVideoBuffer::QImageVideoBuffer(const QImage &image)
    : QAbstractVideoBuffer(*new QImageVideoBufferPrivate, NoHandle)
{
    Q_D(QImageVideoBuffer);

    d->image = image;
}

QImageVideoBuffer::~QImageVideoBuffer()
{
}

QAbstractVideoBuffer::MapMode QImageVideoBuffer::mapMode() const
{
    return d_func()->mapMode;
}

uchar *QImageVideoBuffer::map(MapMode mode, int *numBytes, int *bytesPerLine)
{
    Q_D(QImageVideoBuffer);

    if (d->mapMode == NotMapped && d->image.bits() && mode != NotMapped) {
        d->mapMode = mode;

        if (numBytes)
            *numBytes = d->image.byteCount();

        if (bytesPerLine)
            *bytesPerLine = d->image.bytesPerLine();

        return d->image.bits();
    } else {
        return 0;
    }
}

void QImageVideoBuffer::unmap()
{
    Q_D(QImageVideoBuffer);

    d->mapMode = NotMapped;
}

QT_END_NAMESPACE
