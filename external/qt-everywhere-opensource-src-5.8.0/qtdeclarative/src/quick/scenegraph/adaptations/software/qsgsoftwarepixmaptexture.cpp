/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qsgsoftwarepixmaptexture_p.h"

QT_BEGIN_NAMESPACE

QSGSoftwarePixmapTexture::QSGSoftwarePixmapTexture(const QImage &image, uint flags)
{
    // Prevent pixmap format conversion to reduce memory consumption
    // and surprises in calling code. (See QTBUG-47328)
    if (flags & QSGRenderContext::CreateTexture_Alpha) {
        //If texture should have an alpha
        m_pixmap = QPixmap::fromImage(image, Qt::NoFormatConversion);
    } else {
        //Force opaque texture
        m_pixmap = QPixmap::fromImage(image.convertToFormat(QImage::Format_RGB32), Qt::NoFormatConversion);
    }
}

QSGSoftwarePixmapTexture::QSGSoftwarePixmapTexture(const QPixmap &pixmap)
    : m_pixmap(pixmap)
{
}


int QSGSoftwarePixmapTexture::textureId() const
{
    return 0;
}

QSize QSGSoftwarePixmapTexture::textureSize() const
{
    return m_pixmap.size();
}

bool QSGSoftwarePixmapTexture::hasAlphaChannel() const
{
    return m_pixmap.hasAlphaChannel();
}

bool QSGSoftwarePixmapTexture::hasMipmaps() const
{
    return false;
}

void QSGSoftwarePixmapTexture::bind()
{
    Q_UNREACHABLE();
}

QT_END_NAMESPACE
