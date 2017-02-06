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

#ifndef QT3DRENDER_SCENE3DSGNODE_P_H
#define QT3DRENDER_SCENE3DSGNODE_P_H

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

#include <QtQuick/QSGGeometryNode>

#include "scene3dsgmaterial_p.h"

QT_BEGIN_NAMESPACE

class QSGTexture;

namespace Qt3DRender {

class Scene3DSGNode : public QSGGeometryNode
{
public:
    Scene3DSGNode();
    ~Scene3DSGNode();

    void setTexture(QSGTexture *texture) Q_DECL_NOTHROW
    {
        m_material.setTexture(texture);
        m_opaqueMaterial.setTexture(texture);
        markDirty(DirtyMaterial);
    }
    QSGTexture *texture() const Q_DECL_NOTHROW { return m_material.texture(); }

    void setRect(const QRectF &rect);
    QRectF rect() const Q_DECL_NOTHROW { return m_rect; }

private:
    Scene3DSGMaterial m_material;
    Scene3DSGMaterial m_opaqueMaterial;
    QSGGeometry m_geometry;
    QRectF m_rect;
};

} // namespace Qt3DRender

QT_END_NAMESPACE

#endif // QT3DRENDER_SCENE3DSGNODE_P_H
