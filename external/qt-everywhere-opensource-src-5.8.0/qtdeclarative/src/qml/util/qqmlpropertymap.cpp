/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qqmlpropertymap.h"

#include <private/qmetaobjectbuilder_p.h>
#include <private/qqmlopenmetaobject_p.h>

#include <QDebug>

QT_BEGIN_NAMESPACE

//QQmlPropertyMapMetaObject lets us listen for changes coming from QML
//so we can emit the changed signal.
class QQmlPropertyMapMetaObject : public QQmlOpenMetaObject
{
public:
    QQmlPropertyMapMetaObject(QQmlPropertyMap *obj, QQmlPropertyMapPrivate *objPriv, const QMetaObject *staticMetaObject);

protected:
    virtual QVariant propertyWriteValue(int, const QVariant &);
    virtual void propertyWritten(int index);
    virtual void propertyCreated(int, QMetaPropertyBuilder &);
    virtual int createProperty(const char *, const char *);

    const QString &propertyName(int index);

private:
    QQmlPropertyMap *map;
    QQmlPropertyMapPrivate *priv;
};

class QQmlPropertyMapPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QQmlPropertyMap)
public:
    QQmlPropertyMapMetaObject *mo;
    QStringList keys;

    QVariant updateValue(const QString &key, const QVariant &input);
    void emitChanged(const QString &key, const QVariant &value);
    bool validKeyName(const QString& name);

    const QString &propertyName(int index) const;
};

bool QQmlPropertyMapPrivate::validKeyName(const QString& name)
{
    //The following strings shouldn't be used as property names
    return  name != QLatin1String("keys")
         && name != QLatin1String("valueChanged")
         && name != QLatin1String("QObject")
         && name != QLatin1String("destroyed")
         && name != QLatin1String("deleteLater");
}

QVariant QQmlPropertyMapPrivate::updateValue(const QString &key, const QVariant &input)
{
    Q_Q(QQmlPropertyMap);
    return q->updateValue(key, input);
}

void QQmlPropertyMapPrivate::emitChanged(const QString &key, const QVariant &value)
{
    Q_Q(QQmlPropertyMap);
    emit q->valueChanged(key, value);
}

const QString &QQmlPropertyMapPrivate::propertyName(int index) const
{
    Q_ASSERT(index < keys.size());
    return keys[index];
}

QQmlPropertyMapMetaObject::QQmlPropertyMapMetaObject(QQmlPropertyMap *obj, QQmlPropertyMapPrivate *objPriv, const QMetaObject *staticMetaObject)
    : QQmlOpenMetaObject(obj, staticMetaObject)
{
    map = obj;
    priv = objPriv;
}

QVariant QQmlPropertyMapMetaObject::propertyWriteValue(int index, const QVariant &input)
{
    return priv->updateValue(priv->propertyName(index), input);
}

void QQmlPropertyMapMetaObject::propertyWritten(int index)
{
    priv->emitChanged(priv->propertyName(index), operator[](index));
}

void QQmlPropertyMapMetaObject::propertyCreated(int, QMetaPropertyBuilder &b)
{
    priv->keys.append(QString::fromUtf8(b.name()));
}

int QQmlPropertyMapMetaObject::createProperty(const char *name, const char *value)
{
    if (!priv->validKeyName(QString::fromUtf8(name)))
        return -1;
    return QQmlOpenMetaObject::createProperty(name, value);
}

/*!
    \class QQmlPropertyMap
    \brief The QQmlPropertyMap class allows you to set key-value pairs that can be used in QML bindings.
    \inmodule QtQml

    QQmlPropertyMap provides a convenient way to expose domain data to the UI layer.
    The following example shows how you might declare data in C++ and then
    access it in QML.

    In the C++ file:
    \code
    // create our data
    QQmlPropertyMap ownerData;
    ownerData.insert("name", QVariant(QString("John Smith")));
    ownerData.insert("phone", QVariant(QString("555-5555")));

    // expose it to the UI layer
    QQuickView view;
    QQmlContext *ctxt = view.rootContext();
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

    \note When deriving a class from QQmlPropertyMap, use the
    \l {QQmlPropertyMap::QQmlPropertyMap(DerivedType *derived, QObject *parent)} {protected two-argument constructor}
    which ensures that the class is correctly registered with the Qt \l {Meta-Object System}.
*/

/*!
    Constructs a bindable map with parent object \a parent.
*/
QQmlPropertyMap::QQmlPropertyMap(QObject *parent)
: QObject(*allocatePrivate(), parent)
{
    init(metaObject());
}

/*!
    Destroys the bindable map.
*/
QQmlPropertyMap::~QQmlPropertyMap()
{
}

/*!
    Clears the value (if any) associated with \a key.
*/
void QQmlPropertyMap::clear(const QString &key)
{
    Q_D(QQmlPropertyMap);
    d->mo->setValue(key.toUtf8(), QVariant());
}

/*!
    Returns the value associated with \a key.

    If no value has been set for this key (or if the value has been cleared),
    an invalid QVariant is returned.
*/
QVariant QQmlPropertyMap::value(const QString &key) const
{
    Q_D(const QQmlPropertyMap);
    return d->mo->value(key.toUtf8());
}

/*!
    Sets the value associated with \a key to \a value.

    If the key doesn't exist, it is automatically created.
*/
void QQmlPropertyMap::insert(const QString &key, const QVariant &value)
{
    Q_D(QQmlPropertyMap);

    if (d->validKeyName(key)) {
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
QStringList QQmlPropertyMap::keys() const
{
    Q_D(const QQmlPropertyMap);
    return d->keys;
}

/*!
    \overload

    Same as size().
*/
int QQmlPropertyMap::count() const
{
    Q_D(const QQmlPropertyMap);
    return d->keys.count();
}

/*!
    Returns the number of keys in the map.

    \sa isEmpty(), count()
*/
int QQmlPropertyMap::size() const
{
    Q_D(const QQmlPropertyMap);
    return d->keys.size();
}

/*!
    Returns true if the map contains no keys; otherwise returns
    false.

    \sa size()
*/
bool QQmlPropertyMap::isEmpty() const
{
    Q_D(const QQmlPropertyMap);
    return d->keys.isEmpty();
}

/*!
    Returns true if the map contains \a key.

    \sa size()
*/
bool QQmlPropertyMap::contains(const QString &key) const
{
    Q_D(const QQmlPropertyMap);
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
QVariant &QQmlPropertyMap::operator[](const QString &key)
{
    //### optimize
    Q_D(QQmlPropertyMap);
    QByteArray utf8key = key.toUtf8();
    if (!d->keys.contains(key))
        insert(key, QVariant());//force creation -- needed below

    return (*(d->mo))[utf8key];
}

/*!
    \overload

    Same as value().
*/
QVariant QQmlPropertyMap::operator[](const QString &key) const
{
    return value(key);
}

/*!
    Returns the new value to be stored for the key \a key.  This function is provided
    to intercept updates to a property from QML, where the value provided from QML is \a input.

    Override this function to manipulate the property value as it is updated.  Note that
    this function is only invoked when the value is updated from QML.
*/
QVariant QQmlPropertyMap::updateValue(const QString &key, const QVariant &input)
{
    Q_UNUSED(key)
    return input;
}

/*! \internal */
void QQmlPropertyMap::init(const QMetaObject *staticMetaObject)
{
    Q_D(QQmlPropertyMap);
    d->mo = new QQmlPropertyMapMetaObject(this, d, staticMetaObject);
}

/*! \internal */
QObjectPrivate *QQmlPropertyMap::allocatePrivate()
{
    return new QQmlPropertyMapPrivate;
}

/*!
    \fn void QQmlPropertyMap::valueChanged(const QString &key, const QVariant &value)
    This signal is emitted whenever one of the values in the map is changed. \a key
    is the key corresponding to the \a value that was changed.

    \note valueChanged() is \b NOT emitted when changes are made by calling insert()
    or clear() - it is only emitted when a value is updated from QML.
*/

/*!
    \fn QQmlPropertyMap::QQmlPropertyMap(DerivedType *derived, QObject *parent)

    Constructs a bindable map with parent object \a parent.  Use this constructor
    in classes derived from QQmlPropertyMap.

    The type of \a derived is used to register the property map with the \l {Meta-Object System},
    which is necessary to ensure that properties of the derived class are accessible.
    This type must be derived from QQmlPropertyMap.
*/

QT_END_NAMESPACE
