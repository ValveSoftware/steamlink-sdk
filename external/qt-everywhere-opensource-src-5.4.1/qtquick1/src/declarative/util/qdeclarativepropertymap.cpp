/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
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

#include "qdeclarativepropertymap.h"

#include <private/qmetaobjectbuilder_p.h>
#include "private/qdeclarativeopenmetaobject_p.h"

#include <QDebug>

QT_BEGIN_NAMESPACE

//QDeclarativePropertyMapMetaObject lets us listen for changes coming from QML
//so we can emit the changed signal.
class QDeclarativePropertyMapMetaObject : public QDeclarativeOpenMetaObject
{
public:
    QDeclarativePropertyMapMetaObject(QDeclarativePropertyMap *obj, QDeclarativePropertyMapPrivate *objPriv);

protected:
    virtual void propertyWritten(int index);
    virtual void propertyCreated(int, QMetaPropertyBuilder &);

private:
    QDeclarativePropertyMap *map;
    QDeclarativePropertyMapPrivate *priv;
};

class QDeclarativePropertyMapPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QDeclarativePropertyMap)
public:
    QDeclarativePropertyMapMetaObject *mo;
    QStringList keys;
    void emitChanged(const QString &key, const QVariant &value);
};

void QDeclarativePropertyMapPrivate::emitChanged(const QString &key, const QVariant &value)
{
    Q_Q(QDeclarativePropertyMap);
    emit q->valueChanged(key, value);
}

QDeclarativePropertyMapMetaObject::QDeclarativePropertyMapMetaObject(QDeclarativePropertyMap *obj, QDeclarativePropertyMapPrivate *objPriv) : QDeclarativeOpenMetaObject(obj)
{
    map = obj;
    priv = objPriv;
}

void QDeclarativePropertyMapMetaObject::propertyWritten(int index)
{
    priv->emitChanged(QString::fromUtf8(name(index)), operator[](index));
}

void QDeclarativePropertyMapMetaObject::propertyCreated(int, QMetaPropertyBuilder &b)
{
    priv->keys.append(QString::fromUtf8(b.name()));
}

/*!
    \class QDeclarativePropertyMap
    \since 4.7
    \brief The QDeclarativePropertyMap class allows you to set key-value pairs that can be used in QML bindings.

    QDeclarativePropertyMap provides a convenient way to expose domain data to the UI layer.
    The following example shows how you might declare data in C++ and then
    access it in QML.

    In the C++ file:
    \code
    // create our data
    QDeclarativePropertyMap ownerData;
    ownerData.insert("name", QVariant(QString("John Smith")));
    ownerData.insert("phone", QVariant(QString("555-5555")));

    // expose it to the UI layer
    QDeclarativeView view;
    QDeclarativeContext *ctxt = view.rootContext();
    ctxt->setContextProperty("owner", &ownerData);

    view.setSource(QUrl::fromLocalFile("main.qml"));
    view.show();
    \endcode

    Then, in \c main.qml:
    \code
    Text { text: owner.name + " " + owner.phone }
    \endcode

    The binding is dynamic - whenever a key's value is updated, anything bound to that
    key will be updated as well.

    To detect value changes made in the UI layer you can connect to the valueChanged() signal.
    However, note that valueChanged() is \b NOT emitted when changes are made by calling insert()
    or clear() - it is only emitted when a value is updated from QML.

    \note It is not possible to remove keys from the map; once a key has been added, you can only
    modify or clear its associated value.
*/

/*!
    Constructs a bindable map with parent object \a parent.
*/
QDeclarativePropertyMap::QDeclarativePropertyMap(QObject *parent)
: QObject(*(new QDeclarativePropertyMapPrivate), parent)
{
    Q_D(QDeclarativePropertyMap);
    d->mo = new QDeclarativePropertyMapMetaObject(this, d);
}

/*!
    Destroys the bindable map.
*/
QDeclarativePropertyMap::~QDeclarativePropertyMap()
{
}

/*!
    Clears the value (if any) associated with \a key.
*/
void QDeclarativePropertyMap::clear(const QString &key)
{
    Q_D(QDeclarativePropertyMap);
    d->mo->setValue(key.toUtf8(), QVariant());
}

/*!
    Returns the value associated with \a key.

    If no value has been set for this key (or if the value has been cleared),
    an invalid QVariant is returned.
*/
QVariant QDeclarativePropertyMap::value(const QString &key) const
{
    Q_D(const QDeclarativePropertyMap);
    return d->mo->value(key.toUtf8());
}

/*!
    Sets the value associated with \a key to \a value.

    If the key doesn't exist, it is automatically created.
*/
void QDeclarativePropertyMap::insert(const QString &key, const QVariant &value)
{
    Q_D(QDeclarativePropertyMap);
    //The following strings shouldn't be used as property names
    if (key != QLatin1String("keys")
     && key != QLatin1String("valueChanged")
     && key != QLatin1String("QObject")
     && key != QLatin1String("destroyed")
     && key != QLatin1String("deleteLater")) {
        d->mo->setValue(key.toUtf8(), value);
    } else {
        qWarning() << "Creating property with name"
                   << key
                   << "is not permitted, conflicts with internal symbols.";
    }
}

/*!
    Returns the list of keys.

    Keys that have been cleared will still appear in this list, even though their
    associated values are invalid QVariants.
*/
QStringList QDeclarativePropertyMap::keys() const
{
    Q_D(const QDeclarativePropertyMap);
    return d->keys;
}

/*!
    \overload

    Same as size().
*/
int QDeclarativePropertyMap::count() const
{
    Q_D(const QDeclarativePropertyMap);
    return d->keys.count();
}

/*!
    Returns the number of keys in the map.

    \sa isEmpty(), count()
*/
int QDeclarativePropertyMap::size() const
{
    Q_D(const QDeclarativePropertyMap);
    return d->keys.size();
}

/*!
    Returns true if the map contains no keys; otherwise returns
    false.

    \sa size()
*/
bool QDeclarativePropertyMap::isEmpty() const
{
    Q_D(const QDeclarativePropertyMap);
    return d->keys.isEmpty();
}

/*!
    Returns true if the map contains \a key.

    \sa size()
*/
bool QDeclarativePropertyMap::contains(const QString &key) const
{
    Q_D(const QDeclarativePropertyMap);
    return d->keys.contains(key);
}

/*!
    Returns the value associated with the key \a key as a modifiable
    reference.

    If the map contains no item with key \a key, the function inserts
    an invalid QVariant into the map with key \a key, and
    returns a reference to it.

    \sa insert(), value()
*/
QVariant &QDeclarativePropertyMap::operator[](const QString &key)
{
    //### optimize
    Q_D(QDeclarativePropertyMap);
    QByteArray utf8key = key.toUtf8();
    if (!d->keys.contains(key))
        insert(key, QVariant());//force creation -- needed below

    return (*(d->mo))[utf8key];
}

/*!
    \overload

    Same as value().
*/
QVariant QDeclarativePropertyMap::operator[](const QString &key) const
{
    return value(key);
}

/*!
    \fn void QDeclarativePropertyMap::valueChanged(const QString &key, const QVariant &value)
    This signal is emitted whenever one of the values in the map is changed. \a key
    is the key corresponding to the \a value that was changed.

    \note valueChanged() is \b NOT emitted when changes are made by calling insert()
    or clear() - it is only emitted when a value is updated from QML.
*/

QT_END_NAMESPACE
