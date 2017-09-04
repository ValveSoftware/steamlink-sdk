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


#include "qquickclipnode_p.h"

#include <QtGui/qvector2d.h>
#include <QtCore/qmath.h>

QQuickDefaultClipNode::QQuickDefaultClipNode(const QRectF &rect)
    : m_rect(rect)
    , m_radius(0)
    , m_dirty_geometry(true)
    , m_geometry(QSGGeometry::defaultAttributes_Point2D(), 0)
{
    Q_UNUSED(m_reserved);
    setGeometry(&m_geometry);
    setIsRectangular(true);
}

void QQuickDefaultClipNode::setRect(const QRectF &rect)
{
    m_rect = rect;
    m_dirty_geometry = true;
}

void QQuickDefaultClipNode::setRadius(qreal radius)
{
    m_radius = radius;
    m_dirty_geometry = true;
    setIsRectangular(radius == 0);
}

void QQuickDefaultClipNode::update()
{
    if (m_dirty_geometry) {
        updateGeometry();
        m_dirty_geometry = false;
    }
}

void QQuickDefaultClipNode::updateGeometry()
{
    QSGGeometry *g = geometry();

    if (qFuzzyIsNull(m_radius)) {
        g->allocate(4);
        QSGGeometry::updateRectGeometry(g, m_rect);

    } else {
        int vertexCount = 0;

        // Radius should never exceeds half of the width or half of the height
        qreal radius = qMin(qMin(m_rect.width() / 2, m_rect.height() / 2), m_radius);
        QRectF rect = m_rect;
        rect.adjust(radius, radius, -radius, -radius);

        int segments = qMin(30, qCeil(radius)); // Number of segments per corner.

        g->allocate((segments + 1) * 4);

        QVector2D *vertices = (QVector2D *)g->vertexData();

        for (int part = 0; part < 2; ++part) {
            for (int i = 0; i <= segments; ++i) {
                //### Should change to calculate sin/cos only once.
                qreal angle = qreal(0.5 * M_PI) * (part + i / qreal(segments));
                qreal s = qFastSin(angle);
                qreal c = qFastCos(angle);
                qreal y = (part ? rect.bottom() : rect.top()) - radius * c; // current inner y-coordinate.
                qreal lx = rect.left() - radius * s; // current inner left x-coordinate.
                qreal rx = rect.right() + radius * s; // current inner right x-coordinate.

                vertices[vertexCount++] = QVector2D(rx, y);
                vertices[vertexCount++] = QVector2D(lx, y);
            }
        }

    }
    setClipRect(m_rect);
    markDirty(DirtyGeometry);
}

