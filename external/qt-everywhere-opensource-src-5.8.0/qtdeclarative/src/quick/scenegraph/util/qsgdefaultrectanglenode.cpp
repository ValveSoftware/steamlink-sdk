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

#include "qsgdefaultrectanglenode_p.h"
#include "qsgflatcolormaterial.h"

QT_BEGIN_NAMESPACE

// Unlike our predecessor, QSGSimpleRectNode, use QSGVertexColorMaterial
// instead of Flat in order to allow better batching in the renderer.

QSGDefaultRectangleNode::QSGDefaultRectangleNode()
    : m_geometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), 4)
{
    QSGGeometry::updateColoredRectGeometry(&m_geometry, QRectF());
    setMaterial(&m_material);
    setGeometry(&m_geometry);
    setColor(QColor(255, 255, 255));
#ifdef QSG_RUNTIME_DESCRIPTION
    qsgnode_set_description(this, QLatin1String("rectangle"));
#endif
}

void QSGDefaultRectangleNode::setRect(const QRectF &rect)
{
    QSGGeometry::updateColoredRectGeometry(&m_geometry, rect);
    markDirty(QSGNode::DirtyGeometry);
}

QRectF QSGDefaultRectangleNode::rect() const
{
    const QSGGeometry::ColoredPoint2D *pts = m_geometry.vertexDataAsColoredPoint2D();
    return QRectF(pts[0].x,
                  pts[0].y,
                  pts[3].x - pts[0].x,
                  pts[3].y - pts[0].y);
}

void QSGDefaultRectangleNode::setColor(const QColor &color)
{
    if (color != m_color) {
        m_color = color;
        QSGGeometry::ColoredPoint2D *pts = m_geometry.vertexDataAsColoredPoint2D();
        for (int i = 0; i < 4; ++i) {
            pts[i].r = uchar(qRound(m_color.redF() * m_color.alphaF() * 255));
            pts[i].g = uchar(qRound(m_color.greenF() * m_color.alphaF() * 255));
            pts[i].b = uchar(qRound(m_color.blueF() * m_color.alphaF() * 255));
            pts[i].a = uchar(qRound(m_color.alphaF() * 255));
        }
        markDirty(QSGNode::DirtyGeometry);
    }
}

QColor QSGDefaultRectangleNode::color() const
{
    return m_color;
}

QT_END_NAMESPACE
