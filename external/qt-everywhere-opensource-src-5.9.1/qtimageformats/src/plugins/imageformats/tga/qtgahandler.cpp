/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the TGA plugin in the Qt ImageFormats module.
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

#include "qtgahandler.h"
#include "qtgafile.h"

#include <QtCore/QVariant>
#include <QtCore/QDebug>
#include <QtGui/QImage>

QT_BEGIN_NAMESPACE

QTgaHandler::QTgaHandler()
    : QImageIOHandler()
    , tga(0)
{
}

QTgaHandler::~QTgaHandler()
{
    delete tga;
}

bool QTgaHandler::canRead() const
{
    if (!tga)
        tga = new QTgaFile(device());
    if (tga->isValid())
    {
        setFormat("tga");
        return true;
    }
    return false;
}

bool QTgaHandler::canRead(QIODevice *device)
{
    if (!device) {
        qWarning("QTgaHandler::canRead() called with no device");
        return false;
    }

    // TGA reader implementation needs a seekable QIODevice, so
    // sequential devices are not supported
    if (device->isSequential())
        return false;
    qint64 pos = device->pos();
    bool isValid;
    {
        QTgaFile tga(device);
        isValid = tga.isValid();
    }
    device->seek(pos);
    return isValid;
}

bool QTgaHandler::read(QImage *image)
{
    if (!canRead())
        return false;
    *image = tga->readImage();
    return !image->isNull();
}

QByteArray QTgaHandler::name() const
{
    return "tga";
}

QVariant QTgaHandler::option(ImageOption option) const
{
    if (option == Size && canRead()) {
        return tga->size();
    } else if (option == CompressionRatio) {
        return tga->compression();
    } else if (option == ImageFormat) {
        return QImage::Format_ARGB32;
    }
    return QVariant();
}

void QTgaHandler::setOption(ImageOption option, const QVariant &value)
{
    Q_UNUSED(option);
    Q_UNUSED(value);
}

bool QTgaHandler::supportsOption(ImageOption option) const
{
    return option == CompressionRatio
            || option == Size
            || option == ImageFormat;
}

QT_END_NAMESPACE
