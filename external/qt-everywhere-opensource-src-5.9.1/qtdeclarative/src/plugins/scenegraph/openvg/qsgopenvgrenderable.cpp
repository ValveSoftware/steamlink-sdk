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

#include "qsgopenvgrenderable.h"

QT_BEGIN_NAMESPACE

QSGOpenVGRenderable::QSGOpenVGRenderable()
    : m_opacity(1.0f)
{
    m_opacityPaint = vgCreatePaint();
}

QSGOpenVGRenderable::~QSGOpenVGRenderable()
{
    vgDestroyPaint(m_opacityPaint);
}

void QSGOpenVGRenderable::setOpacity(float opacity)
{
    if (m_opacity == opacity)
        return;

    m_opacity = opacity;
    VGfloat values[] = {
        1.0f, 1.0f, 1.0f, m_opacity
    };
    vgSetParameterfv(m_opacityPaint, VG_PAINT_COLOR, 4, values);
}

float QSGOpenVGRenderable::opacity() const
{
    return m_opacity;
}

VGPaint QSGOpenVGRenderable::opacityPaint() const
{
    return m_opacityPaint;
}

void QSGOpenVGRenderable::setTransform(const QOpenVGMatrix &transform)
{
    m_transform = transform;
}

const QOpenVGMatrix &QSGOpenVGRenderable::transform() const
{
    return m_transform;
}

QT_END_NAMESPACE
