/****************************************************************************
**
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

#ifndef QSSGMODEL_H
#define QSSGMODEL_H

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

#include <QtQuick3D/private/qquick3dnode_p.h>
#include <QtQuick3D/private/qquick3dmaterial_p.h>
#include <QtQuick3D/private/qquick3dgeometry_p.h>

#include <QtQml/QQmlListProperty>

#include <QtCore/QVector>
#include <QtCore/QUrl>

QT_BEGIN_NAMESPACE

class Q_QUICK3D_EXPORT QQuick3DModel : public QQuick3DNode
{
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QSSGTessellationModeValues tessellationMode READ tessellationMode WRITE setTessellationMode NOTIFY tessellationModeChanged)
    Q_PROPERTY(float edgeTessellation READ edgeTessellation WRITE setEdgeTessellation NOTIFY edgeTessellationChanged)
    Q_PROPERTY(float innerTessellation READ innerTessellation WRITE setInnerTessellation NOTIFY innerTessellationChanged)
    Q_PROPERTY(bool isWireframeMode READ isWireframeMode WRITE setIsWireframeMode NOTIFY isWireframeModeChanged)
    Q_PROPERTY(bool castsShadows READ castsShadows WRITE setCastsShadows NOTIFY castsShadowsChanged)
    Q_PROPERTY(bool receivesShadows READ receivesShadows WRITE setReceivesShadows NOTIFY receivesShadowsChanged)
    Q_PROPERTY(QQmlListProperty<QQuick3DMaterial> materials READ materials)
    Q_PROPERTY(bool pickable READ pickable WRITE setPickable NOTIFY pickableChanged)
    Q_PROPERTY(QQuick3DGeometry *geometry READ geometry WRITE setGeometry NOTIFY geometryChanged)

public:
    enum QSSGTessellationModeValues {
        NoTessellation = 0,
        Linear = 1,
        Phong = 2,
        NPatch = 3,
    };
    Q_ENUM(QSSGTessellationModeValues)

    QQuick3DModel();
    ~QQuick3DModel() override;

    QQuick3DObject::Type type() const override;

    QUrl source() const;
    QSSGTessellationModeValues tessellationMode() const;
    float edgeTessellation() const;
    float innerTessellation() const;
    bool isWireframeMode() const;
    bool castsShadows() const;
    bool receivesShadows() const;
    bool pickable() const;
    QQuick3DGeometry *geometry() const;

    QQmlListProperty<QQuick3DMaterial> materials();

public Q_SLOTS:
    void setSource(const QUrl &source);
    void setTessellationMode(QSSGTessellationModeValues tessellationMode);
    void setEdgeTessellation(float edgeTessellation);
    void setInnerTessellation(float innerTessellation);
    void setIsWireframeMode(bool isWireframeMode);
    void setCastsShadows(bool castsShadows);
    void setReceivesShadows(bool receivesShadows);
    void setPickable(bool pickable);
    void setGeometry(QQuick3DGeometry *geometry);

Q_SIGNALS:
    void sourceChanged();
    void tessellationModeChanged();
    void edgeTessellationChanged();
    void innerTessellationChanged();
    void isWireframeModeChanged();
    void castsShadowsChanged();
    void receivesShadowsChanged();
    void pickableChanged();
    void geometryChanged();

protected:
    QSSGRenderGraphObject *updateSpatialNode(QSSGRenderGraphObject *node) override;
    void markAllDirty() override;
    void itemChange(ItemChange, const ItemChangeData &) override;

private:
    enum QSSGModelDirtyType {
        SourceDirty =            0x00000001,
        TessellationModeDirty =  0x00000002,
        TessellationEdgeDirty =  0x00000004,
        TessellationInnerDirty = 0x00000008,
        WireframeDirty =         0x00000010,
        MaterialsDirty =         0x00000020,
        ShadowsDirty =           0x00000040,
        PickingDirty =           0x00000080,
        GeometryDirty =          0x00000100,
    };

    QString translateSource();
    QUrl m_source;
    QSSGTessellationModeValues m_tessellationMode = QSSGTessellationModeValues::NoTessellation;
    float m_edgeTessellation = 1.0f;
    float m_innerTessellation = 1.0f;
    bool m_isWireframeMode = false;

    quint32 m_dirtyAttributes = 0xffffffff; // all dirty by default
    void markDirty(QSSGModelDirtyType type);

    static void qmlAppendMaterial(QQmlListProperty<QQuick3DMaterial> *list, QQuick3DMaterial *material);
    static QQuick3DMaterial *qmlMaterialAt(QQmlListProperty<QQuick3DMaterial> *list, int index);
    static int qmlMaterialsCount(QQmlListProperty<QQuick3DMaterial> *list);
    static void qmlClearMaterials(QQmlListProperty<QQuick3DMaterial> *list);

    QVector<QQuick3DMaterial *> m_materials;
    QQuick3DGeometry *m_geometry = nullptr;
    QMetaObject::Connection m_geometryConnection;
    bool m_castsShadows = true;
    bool m_receivesShadows = true;
    bool m_pickable = false;
};

QT_END_NAMESPACE

#endif // QSSGMODEL_H
