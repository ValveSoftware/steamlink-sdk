/****************************************************************************
**
** Copyright (C) 2008-2012 NVIDIA Corporation.
** Copyright (C) 2019 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Quick 3D.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QSSG_RENDER_RAY_H
#define QSSG_RENDER_RAY_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtQuick3DUtils/private/qssgoption_p.h>
#include <QtQuick3DUtils/private/qssgbounds3_p.h>
#include <QtQuick3DUtils/private/qssgplane_p.h>

#include <QtGui/QVector2D>
#include <QtGui/QVector3D>
#include <QtGui/QMatrix4x4>

QT_BEGIN_NAMESPACE
enum class QSSGRenderBasisPlanes
{
    XY,
    YZ,
    XZ,
};

struct QSSGRenderRay
{
    QVector3D origin;
    QVector3D direction;
    QSSGRenderRay() = default;
    QSSGRenderRay(const QVector3D &inOrigin, const QVector3D &inDirection)
        : origin(inOrigin), direction(inDirection)
    {
    }
    // If we are parallel, then no intersection of course.
    QSSGOption<QVector3D> intersect(const QSSGPlane &inPlane) const;

    struct IntersectionResult
    {
        bool intersects = false;
        float rayLengthSquared = 0.; // Length of the ray in world coordinates for the hit.
        QVector2D relXY; // UV coords for further mouse picking against a offscreen-rendered object.
        QVector3D scenePosition;
        IntersectionResult() = default;
        IntersectionResult(float rl, QVector2D relxy, QVector3D scenePosition)
            : intersects(true)
            , rayLengthSquared(rl)
            , relXY(relxy)
            , scenePosition(scenePosition)
        {}
    };

    IntersectionResult intersectWithAABB(const QMatrix4x4 &inGlobalTransform, const QSSGBounds3 &inBounds,
                                         bool inForceIntersect = false) const;

    QSSGOption<QVector2D> relative(const QMatrix4x4 &inGlobalTransform,
                                        const QSSGBounds3 &inBounds,
                                        QSSGRenderBasisPlanes inPlane) const;

    QSSGOption<QVector2D> relativeXY(const QMatrix4x4 &inGlobalTransform, const QSSGBounds3 &inBounds) const
    {
        return relative(inGlobalTransform, inBounds, QSSGRenderBasisPlanes::XY);
    }
};
QT_END_NAMESPACE
#endif
