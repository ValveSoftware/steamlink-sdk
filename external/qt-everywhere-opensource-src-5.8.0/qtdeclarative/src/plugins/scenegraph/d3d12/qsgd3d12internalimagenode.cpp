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

#include "qsgd3d12internalimagenode_p.h"

QT_BEGIN_NAMESPACE

QSGD3D12InternalImageNode::QSGD3D12InternalImageNode()
{
    setMaterial(&m_material);
}

void QSGD3D12InternalImageNode::setFiltering(QSGTexture::Filtering filtering)
{
    if (m_material.filtering() == filtering)
        return;

    m_material.setFiltering(filtering);
    m_smoothMaterial.setFiltering(filtering);
    markDirty(DirtyMaterial);
}

void QSGD3D12InternalImageNode::setMipmapFiltering(QSGTexture::Filtering filtering)
{
    if (m_material.mipmapFiltering() == filtering)
        return;

    m_material.setMipmapFiltering(filtering);
    m_smoothMaterial.setMipmapFiltering(filtering);
    markDirty(DirtyMaterial);
}

void QSGD3D12InternalImageNode::setVerticalWrapMode(QSGTexture::WrapMode wrapMode)
{
    if (m_material.verticalWrapMode() == wrapMode)
        return;

    m_material.setVerticalWrapMode(wrapMode);
    m_smoothMaterial.setVerticalWrapMode(wrapMode);
    markDirty(DirtyMaterial);
}

void QSGD3D12InternalImageNode::setHorizontalWrapMode(QSGTexture::WrapMode wrapMode)
{
    if (m_material.horizontalWrapMode() == wrapMode)
        return;

    m_material.setHorizontalWrapMode(wrapMode);
    m_smoothMaterial.setHorizontalWrapMode(wrapMode);
    markDirty(DirtyMaterial);
}

void QSGD3D12InternalImageNode::updateMaterialAntialiasing()
{
    if (m_antialiasing)
        setMaterial(&m_smoothMaterial);
    else
        setMaterial(&m_material);
}

void QSGD3D12InternalImageNode::setMaterialTexture(QSGTexture *texture)
{
    m_material.setTexture(texture);
    m_smoothMaterial.setTexture(texture);
}

QSGTexture *QSGD3D12InternalImageNode::materialTexture() const
{
    return m_material.texture();
}

bool QSGD3D12InternalImageNode::updateMaterialBlending()
{
    const bool alpha = m_material.flags() & QSGMaterial::Blending;
    if (materialTexture() && alpha != materialTexture()->hasAlphaChannel()) {
        m_material.setFlag(QSGMaterial::Blending, !alpha);
        return true;
    }
    return false;
}

bool QSGD3D12InternalImageNode::supportsWrap(const QSize &) const
{
    return true;
}

QT_END_NAMESPACE
