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

#include "qsgopenvgtexture.h"
#include "qsgopenvghelpers.h"

QT_BEGIN_NAMESPACE

QSGOpenVGTexture::QSGOpenVGTexture(const QImage &image, uint flags)
{
    Q_UNUSED(flags)

    VGImageFormat format = QSGOpenVGHelpers::qImageFormatToVGImageFormat(image.format());
    m_image = vgCreateImage(format, image.width(), image.height(), VG_IMAGE_QUALITY_BETTER);

    // Do Texture Upload
    vgImageSubData(m_image, image.constBits(), image.bytesPerLine(), format, 0, 0, image.width(), image.height());
}

QSGOpenVGTexture::~QSGOpenVGTexture()
{
    vgDestroyImage(m_image);
}

int QSGOpenVGTexture::textureId() const
{
    return static_cast<int>(m_image);
}

QSize QSGOpenVGTexture::textureSize() const
{
    VGint imageWidth = vgGetParameteri(m_image, VG_IMAGE_WIDTH);
    VGint imageHeight = vgGetParameteri(m_image, VG_IMAGE_HEIGHT);
    return QSize(imageWidth, imageHeight);
}

bool QSGOpenVGTexture::hasAlphaChannel() const
{
    VGImageFormat format = static_cast<VGImageFormat>(vgGetParameteri(m_image, VG_IMAGE_FORMAT));

    switch (format) {
    case VG_sRGBA_8888:
    case VG_sRGBA_8888_PRE:
    case VG_sRGBA_5551:
    case VG_sRGBA_4444:
    case VG_lRGBA_8888:
    case VG_lRGBA_8888_PRE:
    case VG_A_8:
    case VG_A_1:
    case VG_A_4:
    case VG_sARGB_8888:
    case VG_sARGB_8888_PRE:
    case VG_sARGB_1555:
    case VG_sARGB_4444:
    case VG_lARGB_8888:
    case VG_lARGB_8888_PRE:
    case VG_sBGRA_8888:
    case VG_sBGRA_8888_PRE:
    case VG_sBGRA_5551:
    case VG_sBGRA_4444:
    case VG_lBGRA_8888:
    case VG_lBGRA_8888_PRE:
    case VG_sABGR_8888:
    case VG_sABGR_8888_PRE:
    case VG_sABGR_1555:
    case VG_sABGR_4444:
    case VG_lABGR_8888:
    case VG_lABGR_8888_PRE:
        return true;
        break;
    default:
        break;
    }
    return false;
}

bool QSGOpenVGTexture::hasMipmaps() const
{
    return false;
}

void QSGOpenVGTexture::bind()
{
    // No need to bind
}

QT_END_NAMESPACE
