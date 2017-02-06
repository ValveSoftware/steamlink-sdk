/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
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

#ifndef QT3DRENDER_RENDER_PICKBOUNDINGVOLUMEUTILS_H
#define QT3DRENDER_RENDER_PICKBOUNDINGVOLUMEUTILS_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <Qt3DRender/private/qray3d_p.h>
#include <Qt3DRender/private/trianglesvisitor_p.h>
#include <Qt3DRender/private/qraycastingservice_p.h>


QT_BEGIN_NAMESPACE

namespace Qt3DRender {

class QAbstractCollisionQueryService;

namespace Render {

class Entity;
class Renderer;
class FrameGraphNode;

namespace PickingUtils {

struct Q_AUTOTEST_EXPORT ViewportCameraAreaTriplet
{
    Qt3DCore::QNodeId cameraId;
    QRectF viewport;
    QSize area;
};
QT3D_DECLARE_TYPEINFO_3(Qt3DRender, Render, PickingUtils, ViewportCameraAreaTriplet, Q_PRIMITIVE_TYPE)

class Q_AUTOTEST_EXPORT ViewportCameraAreaGatherer
{
public:
    QVector<ViewportCameraAreaTriplet> gather(FrameGraphNode *root);

private:
    QVector<FrameGraphNode *> m_leaves;

    void visit(FrameGraphNode *node);
    ViewportCameraAreaTriplet gatherUpViewportCameraAreas(Render::FrameGraphNode *node) const;
    bool isUnique(const QVector<ViewportCameraAreaTriplet> &vcaTriplets, const ViewportCameraAreaTriplet &vca) const;
};

class Q_AUTOTEST_EXPORT EntityGatherer
{
public:
    explicit EntityGatherer(Entity *root);

    QVector<Entity *> entities() const;

private:
    Entity *m_root;
    mutable QVector<Entity *> m_entities;
    mutable bool m_needsRefresh;
};

class Q_AUTOTEST_EXPORT CollisionVisitor : public TrianglesVisitor
{
public:
    typedef QVector<QCollisionQueryResult::Hit> HitList;
    HitList hits;

    CollisionVisitor(NodeManagers* manager, const Entity *root, const QRay3D& ray) : TrianglesVisitor(manager), m_root(root), m_ray(ray), m_triangleIndex(0) { }
private:
    const Entity *m_root;
    QRay3D m_ray;
    Qt3DRender::QRayCastingService rayCasting;
    uint m_triangleIndex;

    void visit(uint andx, const QVector3D &a,
               uint bndx, const QVector3D &b,
               uint cndx, const QVector3D &c);
};

struct Q_AUTOTEST_EXPORT AbstractCollisionGathererFunctor
{
    AbstractCollisionGathererFunctor();
    virtual ~AbstractCollisionGathererFunctor();

    NodeManagers *m_manager;
    QRay3D m_ray;

    // This define is required to work with QtConcurrent
    typedef CollisionVisitor::HitList result_type;
    result_type operator ()(const Entity *entity) const;
    virtual result_type pick(QAbstractCollisionQueryService *rayCasting, const Entity *entity) const = 0;
};

struct Q_AUTOTEST_EXPORT EntityCollisionGathererFunctor : public AbstractCollisionGathererFunctor
{
    result_type pick(QAbstractCollisionQueryService *rayCasting, const Entity *entity) const Q_DECL_OVERRIDE;
};

struct Q_AUTOTEST_EXPORT TriangleCollisionGathererFunctor : public AbstractCollisionGathererFunctor
{
    result_type pick(QAbstractCollisionQueryService *rayCasting, const Entity *entity) const Q_DECL_OVERRIDE;

    bool rayHitsEntity(QAbstractCollisionQueryService *rayCasting, const Entity *entity) const;
};

Q_AUTOTEST_EXPORT QVector<Entity *> gatherEntities(Entity *entity, QVector<Entity *> entities);

Q_AUTOTEST_EXPORT CollisionVisitor::HitList reduceToFirstHit(CollisionVisitor::HitList &result, const CollisionVisitor::HitList &intermediate);

// Unordered
Q_AUTOTEST_EXPORT CollisionVisitor::HitList reduceToAllHits(CollisionVisitor::HitList &results, const CollisionVisitor::HitList &intermediate);

} // PickingUtils

} // Render

} // Qt3DRender

QT_END_NAMESPACE

#endif // QT3DRENDER_RENDER_PICKBOUNDINGVOLUMEUTILS_H
