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

#ifndef QSSGNODE_H
#define QSSGNODE_H

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

#include <QtQuick3D/private/qquick3dobject_p.h>

#include <QtGui/QVector3D>
#include <QtGui/QMatrix4x4>

#include <QtQuick3DUtils/private/qssgrendereulerangles_p.h>

QT_BEGIN_NAMESPACE
struct QSSGRenderNode;
class QQuick3DNodePrivate;
class Q_QUICK3D_EXPORT QQuick3DNode : public QQuick3DObject
{
    Q_OBJECT
    Q_PROPERTY(float x READ x WRITE setX NOTIFY xChanged)
    Q_PROPERTY(float y READ y WRITE setY NOTIFY yChanged)
    Q_PROPERTY(float z READ z WRITE setZ NOTIFY zChanged)
    Q_PROPERTY(QVector3D rotation READ rotation WRITE setRotation NOTIFY rotationChanged)
    Q_PROPERTY(QVector3D position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(QVector3D scale READ scale WRITE setScale NOTIFY scaleChanged)
    Q_PROPERTY(QVector3D pivot READ pivot WRITE setPivot NOTIFY pivotChanged)
    Q_PROPERTY(float opacity READ localOpacity WRITE setLocalOpacity NOTIFY localOpacityChanged)
    Q_PROPERTY(RotationOrder rotationOrder READ rotationOrder WRITE setRotationOrder NOTIFY rotationOrderChanged)
    Q_PROPERTY(Orientation orientation READ orientation WRITE setOrientation NOTIFY orientationChanged)
    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(QVector3D forward READ forward)
    Q_PROPERTY(QVector3D up READ up)
    Q_PROPERTY(QVector3D right READ right)
    Q_PROPERTY(QVector3D scenePosition READ scenePosition NOTIFY scenePositionChanged)
    Q_PROPERTY(QVector3D sceneRotation READ sceneRotation NOTIFY sceneRotationChanged)
    Q_PROPERTY(QVector3D sceneScale READ sceneScale NOTIFY sceneScaleChanged)
    Q_PROPERTY(QMatrix4x4 sceneTransform READ sceneTransform NOTIFY sceneTransformChanged)

public:
    enum RotationOrder {
        XYZ = EulerOrder::XYZs,
        YZX = EulerOrder::YZXs,
        ZXY = EulerOrder::ZXYs,
        XZY = EulerOrder::XZYs,
        YXZ = EulerOrder::YXZs,
        ZYX = EulerOrder::ZYXs,
        XYZr = EulerOrder::XYZr,
        YZXr = EulerOrder::YZXr,
        ZXYr = EulerOrder::ZXYr,
        XZYr = EulerOrder::XZYr,
        YXZr = EulerOrder::YXZr,
        ZYXr = EulerOrder::ZYXr
    };
    Q_ENUM(RotationOrder)

    enum TransformSpace {
        LocalSpace,
        ParentSpace,
        SceneSpace
    };
    Q_ENUM(TransformSpace)

    enum Orientation { LeftHanded = 0, RightHanded };
    Q_ENUM(Orientation)
    QQuick3DNode(QQuick3DNode *parent = nullptr);
    ~QQuick3DNode() override;

    float x() const;
    float y() const;
    float z() const;
    QVector3D rotation() const;
    QVector3D position() const;
    QVector3D scale() const;
    QVector3D pivot() const;
    float localOpacity() const;
    RotationOrder rotationOrder() const;
    Orientation orientation() const;
    bool visible() const;

    QQuick3DNode *parentNode() const;

    QVector3D forward() const;
    QVector3D up() const;
    QVector3D right() const;

    QVector3D scenePosition() const;
    QVector3D sceneRotation() const;
    QVector3D sceneScale() const;
    QMatrix4x4 sceneTransform() const;
    QMatrix4x4 sceneTransformLeftHanded() const;
    QMatrix4x4 sceneTransformRightHanded() const;

    QQuick3DObject::Type type() const override;

    Q_INVOKABLE void rotate(qreal degrees, const QVector3D &axis, TransformSpace space);

    Q_INVOKABLE QVector3D mapPositionToScene(const QVector3D &localPosition) const;
    Q_INVOKABLE QVector3D mapPositionFromScene(const QVector3D &scenePosition) const;
    Q_INVOKABLE QVector3D mapPositionToNode(QQuick3DNode *node, const QVector3D &localPosition) const;
    Q_INVOKABLE QVector3D mapPositionFromNode(QQuick3DNode *node, const QVector3D &localPosition) const;
    Q_INVOKABLE QVector3D mapDirectionToScene(const QVector3D &localDirection) const;
    Q_INVOKABLE QVector3D mapDirectionFromScene(const QVector3D &sceneDirection) const;
    Q_INVOKABLE QVector3D mapDirectionToNode(QQuick3DNode *node, const QVector3D &localDirection) const;
    Q_INVOKABLE QVector3D mapDirectionFromNode(QQuick3DNode *node, const QVector3D &localDirection) const;

    void markAllDirty() override;

protected:
    void connectNotify(const QMetaMethod &signal) override;
    void disconnectNotify(const QMetaMethod &signal) override;
    void componentComplete() override;

public Q_SLOTS:
    void setX(float x);
    void setY(float y);
    void setZ(float z);
    void setRotation(const QVector3D &rotation);
    void setPosition(const QVector3D &position);
    void setScale(const QVector3D &scale);
    void setPivot(const QVector3D &pivot);
    void setLocalOpacity(float opacity);
    void setRotationOrder(RotationOrder rotationorder);
    void setOrientation(Orientation orientation);
    void setVisible(bool visible);

Q_SIGNALS:
    void xChanged();
    void yChanged();
    void zChanged();
    void rotationChanged();
    void positionChanged();
    void scaleChanged();
    void pivotChanged();
    void localOpacityChanged();
    void rotationOrderChanged();
    void orientationChanged();
    void visibleChanged();
    void sceneTransformChanged();
    void scenePositionChanged();
    void sceneRotationChanged();
    void sceneScaleChanged();

protected:
    QQuick3DNode(QQuick3DNodePrivate &dd, QQuick3DNode *parent = nullptr);
    QSSGRenderGraphObject *updateSpatialNode(QSSGRenderGraphObject *node) override;

private:
    friend QQuick3DSceneManager;
    Q_DISABLE_COPY(QQuick3DNode)
    Q_DECLARE_PRIVATE(QQuick3DNode)
};

QT_END_NAMESPACE

#endif // QSSGNODE_H
