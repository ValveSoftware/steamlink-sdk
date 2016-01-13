/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qqmlobjectmodel_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtQml/qqmlcontext.h>
#include <QtQml/qqmlengine.h>

#include <private/qqmlchangeset_p.h>
#include <private/qqmlglobal_p.h>
#include <private/qobject_p.h>

#include <QtCore/qhash.h>
#include <QtCore/qlist.h>

QT_BEGIN_NAMESPACE

QHash<QObject*, QQmlObjectModelAttached*> QQmlObjectModelAttached::attachedProperties;


class QQmlObjectModelPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QQmlObjectModel)
public:
    class Item {
    public:
        Item(QObject *i) : item(i), ref(0) {}

        void addRef() { ++ref; }
        bool deref() { return --ref == 0; }

        QObject *item;
        int ref;
    };

    QQmlObjectModelPrivate() : QObjectPrivate() {}

    static void children_append(QQmlListProperty<QObject> *prop, QObject *item) {
        static_cast<QQmlObjectModelPrivate *>(prop->data)->children.append(Item(item));
        static_cast<QQmlObjectModelPrivate *>(prop->data)->itemAppended();
        static_cast<QQmlObjectModelPrivate *>(prop->data)->emitChildrenChanged();
    }

    static int children_count(QQmlListProperty<QObject> *prop) {
        return static_cast<QQmlObjectModelPrivate *>(prop->data)->children.count();
    }

    static QObject *children_at(QQmlListProperty<QObject> *prop, int index) {
        return static_cast<QQmlObjectModelPrivate *>(prop->data)->children.at(index).item;
    }

    static void children_clear(QQmlListProperty<QObject> *prop) {
        static_cast<QQmlObjectModelPrivate *>(prop->data)->itemCleared(static_cast<QQmlObjectModelPrivate *>(prop->data)->children);
        static_cast<QQmlObjectModelPrivate *>(prop->data)->children.clear();
        static_cast<QQmlObjectModelPrivate *>(prop->data)->emitChildrenChanged();
    }

    void itemAppended() {
        Q_Q(QQmlObjectModel);
        QQmlObjectModelAttached *attached = QQmlObjectModelAttached::properties(children.last().item);
        attached->setIndex(children.count()-1);
        QQmlChangeSet changeSet;
        changeSet.insert(children.count() - 1, 1);
        emit q->modelUpdated(changeSet, false);
        emit q->countChanged();
    }

    void itemCleared(const QList<Item> &children) {
        Q_Q(QQmlObjectModel);
        foreach (const Item &child, children)
            emit q->destroyingItem(child.item);
        emit q->countChanged();
    }

    void emitChildrenChanged() {
        Q_Q(QQmlObjectModel);
        emit q->childrenChanged();
    }

    int indexOf(QObject *item) const {
        for (int i = 0; i < children.count(); ++i)
            if (children.at(i).item == item)
                return i;
        return -1;
    }


    QList<Item> children;
};


/*!
    \qmltype ObjectModel
    \instantiates QQmlObjectModel
    \inqmlmodule QtQml.Models
    \ingroup qtquick-models
    \brief Defines a set of items to be used as a model

    A ObjectModel contains the visual items to be used in a view.
    When a ObjectModel is used in a view, the view does not require
    a delegate since the ObjectModel already contains the visual
    delegate (items).

    An item can determine its index within the
    model via the \l{ObjectModel::index}{index} attached property.

    The example below places three colored rectangles in a ListView.
    \code
    import QtQuick 2.0
    import QtQml.Models 2.1

    Rectangle {
        ObjectModel {
            id: itemModel
            Rectangle { height: 30; width: 80; color: "red" }
            Rectangle { height: 30; width: 80; color: "green" }
            Rectangle { height: 30; width: 80; color: "blue" }
        }

        ListView {
            anchors.fill: parent
            model: itemModel
        }
    }
    \endcode

    \image visualitemmodel.png

    \sa {Qt Quick Examples - Views}
*/
/*!
    \qmltype VisualItemModel
    \instantiates QQmlObjectModel
    \inqmlmodule QtQuick
    \brief Defines a set of objects to be used as a model

    The VisualItemModel type encapsulates contains the objects to be used
    as a model.

    This element is now primarily available as ObjectModel in the QtQml.Models module.
    VisualItemModel continues to be provided, with the same implementation, in \c QtQuick for
    compatibility reasons.

    For full details about the type, see the \l ObjectModel documentation.

    \sa {QtQml.Models::ObjectModel}
*/

QQmlObjectModel::QQmlObjectModel(QObject *parent)
    : QQmlInstanceModel(*(new QQmlObjectModelPrivate), parent)
{
}

/*!
    \qmlattachedproperty int QtQml.Models::ObjectModel::index
    This attached property holds the index of this delegate's item within the model.

    It is attached to each instance of the delegate.
*/

QQmlListProperty<QObject> QQmlObjectModel::children()
{
    Q_D(QQmlObjectModel);
    return QQmlListProperty<QObject>(this,
                                        d,
                                        d->children_append,
                                        d->children_count,
                                        d->children_at,
                                        d->children_clear);
}

/*!
    \qmlproperty int QtQml.Models::ObjectModel::count

    The number of items in the model.  This property is readonly.
*/
int QQmlObjectModel::count() const
{
    Q_D(const QQmlObjectModel);
    return d->children.count();
}

bool QQmlObjectModel::isValid() const
{
    return true;
}

QObject *QQmlObjectModel::object(int index, bool)
{
    Q_D(QQmlObjectModel);
    QQmlObjectModelPrivate::Item &item = d->children[index];
    item.addRef();
    if (item.ref == 1) {
        emit initItem(index, item.item);
        emit createdItem(index, item.item);
    }
    return item.item;
}

QQmlInstanceModel::ReleaseFlags QQmlObjectModel::release(QObject *item)
{
    Q_D(QQmlObjectModel);
    int idx = d->indexOf(item);
    if (idx >= 0) {
        if (!d->children[idx].deref())
            return QQmlInstanceModel::Referenced;
    }
    return 0;
}

QString QQmlObjectModel::stringValue(int index, const QString &name)
{
    Q_D(QQmlObjectModel);
    if (index < 0 || index >= d->children.count())
        return QString();
    return QQmlEngine::contextForObject(d->children.at(index).item)->contextProperty(name).toString();
}

int QQmlObjectModel::indexOf(QObject *item, QObject *) const
{
    Q_D(const QQmlObjectModel);
    return d->indexOf(item);
}

QQmlObjectModelAttached *QQmlObjectModel::qmlAttachedProperties(QObject *obj)
{
    return QQmlObjectModelAttached::properties(obj);
}

QT_END_NAMESPACE
