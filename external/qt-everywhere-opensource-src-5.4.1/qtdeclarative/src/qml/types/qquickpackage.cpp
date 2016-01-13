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

#include "qquickpackage_p.h"

#include <private/qobject_p.h>
#include <private/qqmlguard_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype Package
    \instantiates QQuickPackage
    \inqmlmodule QtQuick
    \ingroup qtquick-views
    \brief Specifies a collection of named items

    The Package class is used in conjunction with
    VisualDataModel to enable delegates with a shared context
    to be provided to multiple views.

    Any item within a Package may be assigned a name via the
    \l{Package::name}{Package.name} attached property.

    The example below creates a Package containing two named items;
    \e list and \e grid.  The third item in the package (the \l Rectangle) is parented to whichever
    delegate it should appear in.  This allows an item to move
    between views.

    \snippet package/Delegate.qml 0

    These named items are used as the delegates by the two views who
    reference the special \l{VisualDataModel::parts} property to select
    a model which provides the chosen delegate.

    \snippet package/view.qml 0

    \sa {Qt Quick Examples - Views}, {Qt Quick Demo - Photo Viewer}, {Qt QML}
*/

/*!
    \qmlattachedproperty string QtQuick::Package::name
    This attached property holds the name of an item within a Package.
*/


class QQuickPackagePrivate : public QObjectPrivate
{
public:
    QQuickPackagePrivate() {}

    struct DataGuard : public QQmlGuard<QObject>
    {
        DataGuard(QObject *obj, QList<DataGuard> *l) : list(l) { (QQmlGuard<QObject>&)*this = obj; }
        QList<DataGuard> *list;
        void objectDestroyed(QObject *) {
            // we assume priv will always be destroyed after objectDestroyed calls
            list->removeOne(*this);
        }
    };

    QList<DataGuard> dataList;
    static void data_append(QQmlListProperty<QObject> *prop, QObject *o) {
        QList<DataGuard> *list = static_cast<QList<DataGuard> *>(prop->data);
        list->append(DataGuard(o, list));
    }
    static void data_clear(QQmlListProperty<QObject> *prop) {
        QList<DataGuard> *list = static_cast<QList<DataGuard> *>(prop->data);
        list->clear();
    }
    static QObject *data_at(QQmlListProperty<QObject> *prop, int index) {
        QList<DataGuard> *list = static_cast<QList<DataGuard> *>(prop->data);
        return list->at(index);
    }
    static int data_count(QQmlListProperty<QObject> *prop) {
        QList<DataGuard> *list = static_cast<QList<DataGuard> *>(prop->data);
        return list->count();
    }
};

QHash<QObject *, QQuickPackageAttached *> QQuickPackageAttached::attached;

QQuickPackageAttached::QQuickPackageAttached(QObject *parent)
: QObject(parent)
{
    attached.insert(parent, this);
}

QQuickPackageAttached::~QQuickPackageAttached()
{
    attached.remove(parent());
}

QString QQuickPackageAttached::name() const
{
    return _name;
}

void QQuickPackageAttached::setName(const QString &n)
{
    _name = n;
}

QQuickPackage::QQuickPackage(QObject *parent)
    : QObject(*(new QQuickPackagePrivate), parent)
{
}

QQuickPackage::~QQuickPackage()
{
}

QQmlListProperty<QObject> QQuickPackage::data()
{
    Q_D(QQuickPackage);
    return QQmlListProperty<QObject>(this, &d->dataList, QQuickPackagePrivate::data_append,
                                                        QQuickPackagePrivate::data_count,
                                                        QQuickPackagePrivate::data_at,
                                                        QQuickPackagePrivate::data_clear);
}

bool QQuickPackage::hasPart(const QString &name)
{
    Q_D(QQuickPackage);
    for (int ii = 0; ii < d->dataList.count(); ++ii) {
        QObject *obj = d->dataList.at(ii);
        QQuickPackageAttached *a = QQuickPackageAttached::attached.value(obj);
        if (a && a->name() == name)
            return true;
    }
    return false;
}

QObject *QQuickPackage::part(const QString &name)
{
    Q_D(QQuickPackage);
    if (name.isEmpty() && !d->dataList.isEmpty())
        return d->dataList.at(0);

    for (int ii = 0; ii < d->dataList.count(); ++ii) {
        QObject *obj = d->dataList.at(ii);
        QQuickPackageAttached *a = QQuickPackageAttached::attached.value(obj);
        if (a && a->name() == name)
            return obj;
    }

    if (name == QLatin1String("default") && !d->dataList.isEmpty())
        return d->dataList.at(0);

    return 0;
}

QQuickPackageAttached *QQuickPackage::qmlAttachedProperties(QObject *o)
{
    return new QQuickPackageAttached(o);
}



QT_END_NAMESPACE
