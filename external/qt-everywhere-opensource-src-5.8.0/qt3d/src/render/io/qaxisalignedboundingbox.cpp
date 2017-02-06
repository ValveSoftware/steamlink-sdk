/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "qaxisalignedboundingbox_p.h"

#include <QDebug>
#include <QMatrix4x4>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

void QAxisAlignedBoundingBox::update(const QVector<QVector3D> &points)
{
    if (points.isEmpty()) {
        m_center = QVector3D();
        m_radii = QVector3D();
        return;
    }

    QVector3D minPoint = points.at( 0 );
    QVector3D maxPoint = points.at( 0 );

    for (int i = 1; i < points.size(); ++i)
    {
        const QVector3D &point = points.at(i);
        if (point.x() > maxPoint.x())
            maxPoint.setX(point.x());
        if (point.y() > maxPoint.y())
            maxPoint.setY(point.y());
        if (point.z() > maxPoint.z())
            maxPoint.setZ(point.z());
        if (point.x() < minPoint.x())
            minPoint.setX(point.x());
        if (point.y() < minPoint.y())
            minPoint.setY(point.y());
        if (point.z() < minPoint.z())
            minPoint.setZ(point.z());
    }

    m_center = 0.5 * (minPoint + maxPoint);
    m_radii = 0.5 * (maxPoint - minPoint);
#if 0
    qDebug() << "AABB:";
    qDebug() << "    min =" << minPoint;
    qDebug() << "    max =" << maxPoint;
    qDebug() << " center =" << m_center;
    qDebug() << "  radii =" << m_radii;
#endif
}

QDebug operator<<(QDebug dbg, const QAxisAlignedBoundingBox &c)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace() << "AABB ( min:" << c.minPoint() << ", max:" << c.maxPoint() << ')';
    return dbg;
}

} //namespace Qt3DRender

QT_END_NAMESPACE
