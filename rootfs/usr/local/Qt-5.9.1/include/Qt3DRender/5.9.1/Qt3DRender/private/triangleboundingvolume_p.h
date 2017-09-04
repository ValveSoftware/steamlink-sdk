/****************************************************************************
**
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

#ifndef QT3DRENDER_RENDER_TRIANGLEBOUNDINGVOLUME_P_H
#define QT3DRENDER_RENDER_TRIANGLEBOUNDINGVOLUME_P_H

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

#include <Qt3DRender/private/qboundingvolume_p.h>
#include <Qt3DCore/qnodeid.h>
#include <QVector3D>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

namespace Render {

Q_AUTOTEST_EXPORT bool intersectsSegmentTriangle(const RayCasting::QRay3D &ray,
                                                 const QVector3D &a,
                                                 const QVector3D &b,
                                                 const QVector3D &c,
                                                 QVector3D &uvw,
                                                 float &t);

class Q_AUTOTEST_EXPORT TriangleBoundingVolume : public RayCasting::QBoundingVolume
{
public:
    TriangleBoundingVolume();
    explicit TriangleBoundingVolume(Qt3DCore::QNodeId id,
                                    const QVector3D &a,
                                    const QVector3D &b,
                                    const QVector3D &c);

    Qt3DCore::QNodeId id() const Q_DECL_FINAL;
    bool intersects(const RayCasting::QRay3D &ray, QVector3D *q, QVector3D *uvw) const Q_DECL_FINAL;
    Type type() const  Q_DECL_FINAL;

    QVector3D a() const;
    QVector3D b() const;
    QVector3D c() const;

    void setA(const QVector3D &a);
    void setB(const QVector3D &b);
    void setC(const QVector3D &c);

    TriangleBoundingVolume transformed(const QMatrix4x4 &mat) const;

    inline TriangleBoundingVolume &transform(const QMatrix4x4 &mat)
    {
        *this = transformed(mat);
        return *this;
    }

private:
    Qt3DCore::QNodeId m_id;
    QVector3D m_a, m_b, m_c;
};

} // namespace Render

} // namespace Qt3DRender

QT_END_NAMESPACE

Q_DECLARE_METATYPE(Qt3DRender::Render::TriangleBoundingVolume*) // LCOV_EXCL_LINE

#endif // QT3DRENDER_RENDER_TRIANGLEBOUNDINGVOLUME_P_H
