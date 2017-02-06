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

#include "qsgd3d12glyphnode_p.h"
#include "qsgd3d12builtinmaterials_p.h"

QT_BEGIN_NAMESPACE

void QSGD3D12GlyphNode::setMaterialColor(const QColor &color)
{
    static_cast<QSGD3D12TextMaterial *>(m_material)->setColor(color);
}

void QSGD3D12GlyphNode::update()
{
    QRawFont font = m_glyphs.rawFont();
    QMargins margins(0, 0, 0, 0);

    if (m_style == QQuickText::Normal) {
        // QSGBasicGlyphNode dtor will delete
        m_material = new QSGD3D12TextMaterial(QSGD3D12TextMaterial::Normal, m_rc, font);
    } else if (m_style == QQuickText::Outline) {
        QSGD3D12TextMaterial *material = new QSGD3D12TextMaterial(QSGD3D12TextMaterial::Outlined,
                                                                  m_rc, font, QFontEngine::Format_A8);
        material->setStyleColor(m_styleColor);
        m_material = material;
        margins = QMargins(1, 1, 1, 1);
    } else {
        QSGD3D12TextMaterial *material = new QSGD3D12TextMaterial(QSGD3D12TextMaterial::Styled,
                                                                  m_rc, font, QFontEngine::Format_A8);
        if (m_style == QQuickText::Sunken) {
            material->setStyleShift(QVector2D(0, -1));
            margins.setTop(1);
        } else if (m_style == QQuickText::Raised) {
            material->setStyleShift(QVector2D(0, 1));
            margins.setBottom(1);
        }
        material->setStyleColor(m_styleColor);
        m_material = material;
    }

    QSGD3D12TextMaterial *textMaterial = static_cast<QSGD3D12TextMaterial *>(m_material);
    textMaterial->setColor(m_color);

    QRectF boundingRect;
    textMaterial->populate(m_position, m_glyphs.glyphIndexes(), m_glyphs.positions(), geometry(),
                           &boundingRect, &m_baseLine, margins);
    setBoundingRect(boundingRect);

    setMaterial(m_material);
    markDirty(DirtyGeometry);
}

QT_END_NAMESPACE
