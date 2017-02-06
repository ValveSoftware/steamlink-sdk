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

#include "sphere_p.h"

#include <Qt3DRender/private/qray3d_p.h>

#include <QPair>

#include <math.h>

QT_BEGIN_NAMESPACE

namespace {

// Algorithms taken from Real-time collision detection, p178-179

// Intersects ray r = p + td, |d| = 1, with sphere s and, if intersecting,
// returns true and intersection point q; false otherwise
bool intersectRaySphere(const Qt3DRender::QRay3D &ray, const Qt3DRender::Render::Sphere &s, QVector3D *q = nullptr)
{
    const QVector3D p = ray.origin();
    const QVector3D d = ray.direction();
    const QVector3D m = p - s.center();
    const float c = QVector3D::dotProduct(m, m) - s.radius() * s.radius();

    // If there is definitely at least one real root, there must be an intersection
    if (q == nullptr && c <= 0.0f)
        return true;

    const float b = QVector3D::dotProduct(m, d);
    // Exit if r’s origin outside s (c > 0) and r pointing away from s (b > 0)
    if (c > 0.0f && b > 0.0f)
        return false;

    const float discr = b*b - c;
    // A negative discriminant corresponds to ray missing sphere
    if (discr < 0.0f)
        return false;

    // If we don't need the intersection point, return early
    if (q == nullptr)
        return true;

    // Ray now found to intersect sphere, compute smallest t value of intersection
    float t = -b - sqrt(discr);

    // If t is negative, ray started inside sphere so clamp t to zero
    if (t < 0.0f)
        t = 0.0f;

    *q = p + t * d;
    return true;
}

inline QPair<int, int> findExtremePoints(const QVector<QVector3D> &points)
{
    // Find indices of extreme points along x, y, and z axes
    int xMin = 0, xMax = 0, yMin = 0, yMax = 0, zMin = 0, zMax = 0;
    for (int i = 1; i < points.size(); ++i) {
        const QVector3D &p = points.at(i);
        if (p.x() < points[xMin].x())
            xMin = i;
        if (p.x() > points[xMax].x())
            xMax = i;
        if (p.y() < points[yMin].y())
            yMin = i;
        if (p.y() > points[yMax].y())
            yMax = i;
        if (p.z() < points[zMin].z())
            zMin = i;
        if (p.z() > points[zMax].z())
            zMax = i;
    }

    // Calculate squared distance for the pairs of points
    const float xDist2 = (points.at(xMax) - points.at(xMin)).lengthSquared();
    const float yDist2 = (points.at(yMax) - points.at(yMin)).lengthSquared();
    const float zDist2 = (points.at(zMax) - points.at(zMin)).lengthSquared();

    // Select most distant pair
    QPair<int, int> extremeIndices(xMin, xMax);
    if (yDist2 > xDist2 && yDist2 > zDist2)
        extremeIndices = qMakePair(yMin, yMax);
    if (zDist2 > xDist2 && zDist2 > yDist2)
        extremeIndices = qMakePair(zMin, zMax);

    return extremeIndices;
}

inline void sphereFromExtremePoints(Qt3DRender::Render::Sphere &s, const QVector<QVector3D> &points)
{
    // Find two most separated points on any of the basis vectors
    QPair<int, int> extremeIndices = findExtremePoints(points);

    // Construct sphere to contain these two points
    const QVector3D &p = points.at(extremeIndices.first);
    const QVector3D &q = points.at(extremeIndices.second);
    const QVector3D c = 0.5f * (p + q);
    s.setCenter(c);
    s.setRadius((q - c).length());
}

inline void constructRitterSphere(Qt3DRender::Render::Sphere &s, const QVector<QVector3D> &points)
{
    // Calculate the sphere encompassing two axially extreme points
    sphereFromExtremePoints(s, points);

    // Now make sure the sphere bounds all points by growing if needed
    s.expandToContain(points);
}

} // anonymous namespace

namespace Qt3DRender {

namespace Render {

const float Sphere::ms_epsilon = 1.0e-7f;

Sphere Sphere::fromPoints(const QVector<QVector3D> &points)
{
    Sphere s;
    s.initializeFromPoints(points);
    return s;
}

void Sphere::initializeFromPoints(const QVector<QVector3D> &points)
{
    if (!points.isEmpty())
        constructRitterSphere(*this, points);
}

void Sphere::expandToContain(const QVector3D &p)
{
    const QVector3D d = p - m_center;
    const float dist2 = d.lengthSquared();

    if (dist2 > m_radius * m_radius) {
        // Expand radius so sphere also contains p
        const float dist = sqrt(dist2);
        const float newRadius = 0.5f * (m_radius + dist);
        const float k = (newRadius - m_radius) / dist;
        m_radius = newRadius;
        m_center += k * d;
    }
}

void Sphere::expandToContain(const Sphere &sphere)
{
    const QVector3D d(sphere.m_center - m_center);
    const float dist2 = d.lengthSquared();

    const float dr = sphere.m_radius - m_radius;
    if (dr * dr >= dist2) {
        // Larger sphere encloses the smaller. Set our size to the larger
        if (m_radius > sphere.m_radius)
            return;
        else
            *this = sphere;
    } else {
        // The spheres are overlapping or disjoint
        const float dist = sqrt(dist2);
        const float newRadius = 0.5f * (dist + m_radius + sphere.m_radius);
        if (dist > ms_epsilon)
            m_center += d * (newRadius - m_radius) / dist;
        m_radius = newRadius;
    }
}

Sphere Sphere::transformed(const QMatrix4x4 &mat) const
{
    // Transform extremities in x, y, and z directions to find extremities
    // of the resulting ellipsoid
    QVector3D x = mat.map(m_center + QVector3D(m_radius, 0.0f, 0.0f));
    QVector3D y = mat.map(m_center + QVector3D(0.0f, m_radius, 0.0f));
    QVector3D z = mat.map(m_center + QVector3D(0.0f, 0.0f, m_radius));

    // Transform center and find maximum radius of ellipsoid
    QVector3D c = mat.map(m_center);
    float rSquared = qMax(qMax((x - c).lengthSquared(), (y - c).lengthSquared()), (z - c).lengthSquared());
    return Sphere(c, sqrt(rSquared), id());
}

Qt3DCore::QNodeId Sphere::id() const
{
    return m_id;
}

bool Sphere::intersects(const QRay3D &ray, QVector3D *q) const
{
    return intersectRaySphere(ray, *this, q);
}

QBoundingVolume::Type Sphere::type() const
{
    return QBoundingVolume::Sphere;
}

} // Render

} // Qt3DRender

QT_END_NAMESPACE
