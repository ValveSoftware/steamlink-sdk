/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qsgsimplerectnode.h"
#include "qsgflatcolormaterial.h"

QT_BEGIN_NAMESPACE

/*!
  \class QSGSimpleRectNode

  \brief The QSGSimpleRectNode class is a convenience class for drawing
  solid filled rectangles using scenegraph.

 */



/*!
    Constructs a QSGSimpleRectNode instance which is spanning \a rect with
    the color \a color.
 */
QSGSimpleRectNode::QSGSimpleRectNode(const QRectF &rect, const QColor &color)
    : m_geometry(QSGGeometry::defaultAttributes_Point2D(), 4)
{
    Q_UNUSED(reserved);
    QSGGeometry::updateRectGeometry(&m_geometry, rect);
    m_material.setColor(color);
    setMaterial(&m_material);
    setGeometry(&m_geometry);
}



/*!
    Constructs a QSGSimpleRectNode instance with an empty rectangle and
    white color.
 */
QSGSimpleRectNode::QSGSimpleRectNode()
    : m_geometry(QSGGeometry::defaultAttributes_Point2D(), 4)
{
    QSGGeometry::updateRectGeometry(&m_geometry, QRectF());
    setMaterial(&m_material);
    setGeometry(&m_geometry);
}



/*!
    Sets the rectangle of this rect node to \a rect.
 */
void QSGSimpleRectNode::setRect(const QRectF &rect)
{
    QSGGeometry::updateRectGeometry(&m_geometry, rect);
    markDirty(QSGNode::DirtyGeometry);
}


/*!
    \fn void QSGSimpleRectNode::setRect(qreal x, qreal y, qreal w, qreal h)
    \overload

    Sets the rectangle of this rect node to begin at (\a x, \a y) and have
    width \a w and height \a h.
 */

/*!
    Returns the rectangle that this rect node covers.
 */
QRectF QSGSimpleRectNode::rect() const
{
    const QSGGeometry::Point2D *pts = m_geometry.vertexDataAsPoint2D();
    return QRectF(pts[0].x,
                  pts[0].y,
                  pts[3].x - pts[0].x,
                  pts[3].y - pts[0].y);
}


/*!
    Sets the color of this rectangle to \a color. The default
    color will be white.
 */
void QSGSimpleRectNode::setColor(const QColor &color)
{
    if (color != m_material.color()) {
        m_material.setColor(color);
        markDirty(QSGNode::DirtyMaterial);
    }
}



/*!
    Returns the color of this rectangle.
 */
QColor QSGSimpleRectNode::color() const
{
    return m_material.color();
}

QT_END_NAMESPACE
