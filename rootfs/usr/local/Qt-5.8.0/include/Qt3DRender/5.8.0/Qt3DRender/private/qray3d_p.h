/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
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

#ifndef QT3DRENDER_QRAY3D_H
#define QT3DRENDER_QRAY3D_H

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

#include <Qt3DRender/qt3drender_global.h>
#include <QtGui/qvector3d.h>
#include <QtGui/qmatrix4x4.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

class QT3DRENDERSHARED_EXPORT QRay3D
{
public:
    QRay3D();
    explicit QRay3D(const QVector3D &origin, const QVector3D &direction = QVector3D(0.0f, 0.0f, 1.0f), float distance = 1.0f);
    ~QRay3D();

    QVector3D origin() const;
    void setOrigin(const QVector3D &value);

    QVector3D direction() const;
    void setDirection(const QVector3D &value);

    float distance() const;
    void setDistance(float distance);

    bool contains(const QVector3D &point) const;
    bool contains(const QRay3D &ray) const;

    QVector3D point(float t) const;
    float projectedDistance(const QVector3D &point) const;

    QVector3D project(const QVector3D &vector) const;

    float distance(const QVector3D &point) const;

    QRay3D &transform(const QMatrix4x4 &matrix);
    QRay3D transformed(const QMatrix4x4 &matrix) const;

    bool operator==(const QRay3D &other) const;
    bool operator!=(const QRay3D &other) const;

private:
    QVector3D m_origin;
    QVector3D m_direction;
    float m_distance;
};
QT3D_DECLARE_TYPEINFO(Qt3DRender, QRay3D, Q_MOVABLE_TYPE)


#ifndef QT_NO_DEBUG_STREAM
QT3DRENDERSHARED_EXPORT QDebug operator<<(QDebug dbg, const QRay3D &ray);
#endif

#ifndef QT_NO_DATASTREAM
QT3DRENDERSHARED_EXPORT QDataStream &operator<<(QDataStream &stream, const QRay3D &ray);
QT3DRENDERSHARED_EXPORT QDataStream &operator>>(QDataStream &stream, QRay3D &ray);
#endif

} // namespace Qt3DRender
QT_END_NAMESPACE

inline bool qFuzzyCompare(const Qt3DRender::QRay3D &ray1, const Qt3DRender::QRay3D &ray2)
{
    return qFuzzyCompare(ray1.origin(), ray2.origin()) &&
           qFuzzyCompare(ray1.direction(), ray2.direction());
}

Q_DECLARE_METATYPE(Qt3DRender::QRay3D) // LCOV_EXCL_LINE

#endif // QT3DRENDER_QRAY3D_H
