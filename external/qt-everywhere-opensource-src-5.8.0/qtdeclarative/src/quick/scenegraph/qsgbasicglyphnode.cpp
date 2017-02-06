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

#include "qsgbasicglyphnode_p.h"
#include <qsgmaterial.h> // just so that we can safely do delete m_material in the dtor

QT_BEGIN_NAMESPACE

QSGBasicGlyphNode::QSGBasicGlyphNode()
    : m_style(QQuickText::Normal)
    , m_material(0)
    , m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 0)
{
    m_geometry.setDrawingMode(QSGGeometry::DrawTriangles);
    setGeometry(&m_geometry);
}

QSGBasicGlyphNode::~QSGBasicGlyphNode()
{
    delete m_material;
}

void QSGBasicGlyphNode::setColor(const QColor &color)
{
    m_color = color;
    if (m_material != 0) {
        setMaterialColor(color);
        markDirty(DirtyMaterial);
    }
}

void QSGBasicGlyphNode::setGlyphs(const QPointF &position, const QGlyphRun &glyphs)
{
    if (m_material != 0)
        delete m_material;

    m_position = position;
    m_glyphs = glyphs;

#ifdef QSG_RUNTIME_DESCRIPTION
    qsgnode_set_description(this, QLatin1String("glyphs"));
#endif
}

void QSGBasicGlyphNode::setStyle(QQuickText::TextStyle style)
{
    if (m_style == style)
        return;
    m_style = style;
}

void QSGBasicGlyphNode::setStyleColor(const QColor &color)
{
    if (m_styleColor == color)
        return;
    m_styleColor = color;
}

QT_END_NAMESPACE
