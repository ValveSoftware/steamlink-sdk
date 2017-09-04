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

#ifndef QT3DRENDER_RENDER_SPHERE_H
#define QT3DRENDER_RENDER_SPHERE_H

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

#include <Qt3DRender/private/qt3drender_global_p.h>
#include <Qt3DCore/qnodeid.h>
#include <Qt3DRender/private/boundingsphere_p.h>

#include <QMatrix4x4>
#include <QVector3D>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

namespace Render {

class QT3DRENDERSHARED_PRIVATE_EXPORT Sphere : public RayCasting::BoundingSphere
{
public:
    inline Sphere(Qt3DCore::QNodeId i = Qt3DCore::QNodeId())
        : m_center()
        , m_radius(0.0f)
        , m_id(i)
    {}

    inline Sphere(const QVector3D &c, float r, Qt3DCore::QNodeId i = Qt3DCore::QNodeId())
        : m_center(c)
        , m_radius(r)
        , m_id(i)
    {}

    void setCenter(const QVector3D &c);
    QVector3D center() const Q_DECL_OVERRIDE;

    inline bool isNull() { return m_center == QVector3D() && m_radius == 0.0f; }

    void setRadius(float r);
    float radius() const Q_DECL_OVERRIDE;

    void clear();
    void initializeFromPoints(const QVector<QVector3D> &points);
    void expandToContain(const QVector3D &point);
    inline void expandToContain(const QVector<QVector3D> &points)
    {
        for (const QVector3D &p : points)
            expandToContain(p);
    }

    void expandToContain(const Sphere &sphere);

    Sphere transformed(const QMatrix4x4 &mat) const;
    inline Sphere &transform(const QMatrix4x4 &mat)
    {
        *this = transformed(mat);
        return *this;
    }

    Qt3DCore::QNodeId id() const Q_DECL_FINAL;
    bool intersects(const RayCasting::QRay3D &ray, QVector3D *q, QVector3D *uvw = nullptr) const Q_DECL_FINAL;
    Type type() const Q_DECL_FINAL;

    static Sphere fromPoints(const QVector<QVector3D> &points);

private:
    QVector3D m_center;
    float m_radius;
    Qt3DCore::QNodeId m_id;

    static const float ms_epsilon;
};

inline void Sphere::setCenter(const QVector3D &c)
{
    m_center = c;
}

inline QVector3D Sphere::center() const
{
    return m_center;
}

inline void Sphere::setRadius(float r)
{
    m_radius = r;
}

inline float Sphere::radius() const
{
    return m_radius;
}

inline void Sphere::clear()
{
    m_center = QVector3D();
    m_radius = 0.0f;
}

inline bool intersects(const Sphere &a, const Sphere &b)
{
    // Calculate squared distance between sphere centers
    const QVector3D d = a.center() - b.center();
    const float distSq = QVector3D::dotProduct(d, d);

    // Spheres intersect if squared distance is less than squared
    // sum of radii
    const float sumRadii = a.radius() + b.radius();
    return distSq <= sumRadii * sumRadii;
}

} // Render

} // Qt3DRender

QT_END_NAMESPACE

Q_DECLARE_METATYPE(Qt3DRender::Render::Sphere) // LCOV_EXCL_LINE

#endif // QT3DRENDER_RENDER_SPHERE_H
