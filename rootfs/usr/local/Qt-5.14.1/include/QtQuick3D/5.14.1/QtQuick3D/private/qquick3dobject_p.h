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

#ifndef QSSGOBJECT_H
#define QSSGOBJECT_H

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

#include <QtQuick3D/private/qtquick3dglobal_p.h>

#include <QtQml/qqml.h>
#include <QtQml/qqmlcomponent.h>

#include <QtCore/QObject>

#include <QtQuick3DRuntimeRender/private/qssgrendergraphobject_p.h>

QT_BEGIN_NAMESPACE

class QQuick3DObjectPrivate;
class QQuick3DSceneManager;

class Q_QUICK3D_EXPORT QQuick3DObject : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_DECLARE_PRIVATE(QQuick3DObject)
    Q_DISABLE_COPY(QQuick3DObject)

    Q_PROPERTY(QQuick3DObject *parent READ parentItem WRITE setParentItem NOTIFY parentChanged DESIGNABLE false FINAL)
    Q_PRIVATE_PROPERTY(QQuick3DObject::d_func(), QQmlListProperty<QObject> data READ data DESIGNABLE false)
    Q_PRIVATE_PROPERTY(QQuick3DObject::d_func(), QQmlListProperty<QObject> resources READ resources DESIGNABLE false)
    Q_PRIVATE_PROPERTY(QQuick3DObject::d_func(),
                       QQmlListProperty<QQuick3DObject> children READ children NOTIFY childrenChanged DESIGNABLE false)

    Q_PROPERTY(QQuick3DObject::Type type READ type CONSTANT)

    Q_PRIVATE_PROPERTY(QQuick3DObject::d_func(), QQmlListProperty<QQuickState> states READ states DESIGNABLE false)
    Q_PRIVATE_PROPERTY(QQuick3DObject::d_func(), QQmlListProperty<QQuickTransition> transitions READ transitions DESIGNABLE false)
    Q_PROPERTY(QString state READ state WRITE setState NOTIFY stateChanged)

    Q_CLASSINFO("DefaultProperty", "data")
    Q_CLASSINFO("qt_QmlJSWrapperFactoryMethod", "_q_createJSWrapper(QV4::ExecutionEngine*)")
public:
    enum Type {
        Unknown = 0,
        SceneEnvironment,
        Node,
        Layer,
        Light,
        Camera,
        Model,
        DefaultMaterial,
        PrincipledMaterial,
        Image,
        Text,
        CustomMaterial,
        RenderPlugin,
        Lightmaps,
        Geometry,
        LastKnownGraphObjectType,
    };
    Q_ENUM(Type)

    enum ItemChange {
        ItemChildAddedChange, // value.item
        ItemChildRemovedChange, // value.item
        ItemSceneChange, // value.window
        ItemVisibleHasChanged, // value.boolValue
        ItemParentHasChanged, // value.item
        ItemOpacityHasChanged, // value.realValue
        ItemActiveFocusHasChanged, // value.boolValue
        ItemRotationHasChanged, // value.realValue
        ItemAntialiasingHasChanged, // value.boolValue
        ItemDevicePixelRatioHasChanged, // value.realValue
        ItemEnabledHasChanged // value.boolValue
    };

    union ItemChangeData {
        ItemChangeData(QQuick3DObject *v) : item(v) {}
        ItemChangeData(QQuick3DSceneManager *v) : sceneManager(v) {}
        ItemChangeData(qreal v) : realValue(v) {}
        ItemChangeData(bool v) : boolValue(v) {}

        QQuick3DObject *item;
        QQuick3DSceneManager *sceneManager;
        qreal realValue;
        bool boolValue;
    };

    explicit QQuick3DObject(QQuick3DObject *parent = nullptr);
    ~QQuick3DObject() override;

    virtual QQuick3DObject::Type type() const = 0;

    QString state() const;
    void setState(const QString &state);

    QList<QQuick3DObject *> childItems() const;

    QQuick3DSceneManager *sceneManager() const;
    QQuick3DObject *parentItem() const;

public Q_SLOTS:
    void update();

    void setParentItem(QQuick3DObject *parentItem);

Q_SIGNALS:
    void sceneManagerChanged();
    void parentChanged();
    void childrenChanged();
    void stateChanged();

protected:
    virtual QSSGRenderGraphObject *updateSpatialNode(QSSGRenderGraphObject *node) = 0;
    virtual void markAllDirty();
    virtual void itemChange(ItemChange, const ItemChangeData &);
    QQuick3DObject(QQuick3DObjectPrivate &dd, QQuick3DObject *parent = nullptr);

    void classBegin() override;
    void componentComplete() override;

    bool isComponentComplete() const;

private:
    Q_PRIVATE_SLOT(d_func(), void _q_resourceObjectDeleted(QObject *))
    Q_PRIVATE_SLOT(d_func(), quint64 _q_createJSWrapper(QV4::ExecutionEngine *))

    friend QQuick3DSceneManager;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuick3DObject)

#endif // QSSGOBJECT_H
