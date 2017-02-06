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

#include "qqmlbind_p.h"

#include <private/qqmlnullablevalue_p.h>
#include <private/qqmlproperty_p.h>
#include <private/qqmlbinding_p.h>
#include <private/qqmlmetatype_p.h>

#include <qqmlengine.h>
#include <qqmlcontext.h>
#include <qqmlproperty.h>
#include <qqmlinfo.h>

#include <QtCore/qfile.h>
#include <QtCore/qdebug.h>
#include <QtCore/qtimer.h>

#include <private/qobject_p.h>

QT_BEGIN_NAMESPACE

class QQmlBindPrivate : public QObjectPrivate
{
public:
    QQmlBindPrivate() : obj(0), componentComplete(true), delayed(false), pendingEval(false) {}
    ~QQmlBindPrivate() { }

    QQmlNullableValue<bool> when;
    QPointer<QObject> obj;
    QString propName;
    QQmlNullableValue<QVariant> value;
    QQmlProperty prop;
    QQmlAbstractBinding::Ptr prevBind;
    bool componentComplete:1;
    bool delayed:1;
    bool pendingEval:1;

    void validate(QObject *binding) const;
};

void QQmlBindPrivate::validate(QObject *binding) const
{
    if (!obj || (when.isValid() && !when))
        return;

    if (!prop.isValid()) {
        qmlInfo(binding) << "Property '" << propName << "' does not exist on " << QQmlMetaType::prettyTypeName(obj) << ".";
        return;
    }

    if (!prop.isWritable()) {
        qmlInfo(binding) << "Property '" << propName << "' on " << QQmlMetaType::prettyTypeName(obj) << " is read-only.";
        return;
    }
}

/*!
    \qmltype Binding
    \instantiates QQmlBind
    \inqmlmodule QtQml
    \ingroup qtquick-interceptors
    \brief Enables the arbitrary creation of property bindings.

    In QML, property bindings result in a dependency between the properties of
    different objects.

    \section1 Binding to an inaccessible property

    Sometimes it is necessary to bind an object's property to
    that of another object that isn't directly instantiated by QML, such as a
    property of a class exported to QML by C++. You can use the Binding type
    to establish this dependency; binding any value to any object's property.

    For example, in a C++ application that maps an "app.enteredText" property
    into QML, you can use Binding to update the enteredText property.

    \code
    TextEdit { id: myTextField; text: "Please type here..." }
    Binding { target: app; property: "enteredText"; value: myTextField.text }
    \endcode

    When \c{text} changes, the C++ property \c{enteredText} will update
    automatically.

    \section1 Conditional bindings

    In some cases you may want to modify the value of a property when a certain
    condition is met but leave it unmodified otherwise. Often, it's not possible
    to do this with direct bindings, as you have to supply values for all
    possible branches.

    For example, the code snippet below results in a warning whenever you
    release the mouse. This is because the value of the binding is undefined
    when the mouse isn't pressed.

    \qml
    // produces warning: "Unable to assign [undefined] to double value"
    value: if (mouse.pressed) mouse.mouseX
    \endqml

    The Binding type can prevent this warning.

    \qml
    Binding on value {
        when: mouse.pressed
        value: mouse.mouseX
    }
    \endqml

    The Binding type restores any previously set direct bindings on the
    property.

    \sa {Qt QML}
*/
QQmlBind::QQmlBind(QObject *parent)
    : QObject(*(new QQmlBindPrivate), parent)
{
}

QQmlBind::~QQmlBind()
{
}

/*!
    \qmlproperty bool QtQml::Binding::when

    This property holds when the binding is active.
    This should be set to an expression that evaluates to true when you want the binding to be active.

    \code
    Binding {
        target: contactName; property: 'text'
        value: name; when: list.ListView.isCurrentItem
    }
    \endcode

    When the binding becomes inactive again, any direct bindings that were previously
    set on the property will be restored.
*/
bool QQmlBind::when() const
{
    Q_D(const QQmlBind);
    return d->when;
}

void QQmlBind::setWhen(bool v)
{
    Q_D(QQmlBind);
    if (!d->when.isNull && d->when == v)
        return;

    d->when = v;
    if (v && d->componentComplete)
        d->validate(this);
    eval();
}

/*!
    \qmlproperty Object QtQml::Binding::target

    The object to be updated.
*/
QObject *QQmlBind::object()
{
    Q_D(const QQmlBind);
    return d->obj;
}

void QQmlBind::setObject(QObject *obj)
{
    Q_D(QQmlBind);
    if (d->obj && d->when.isValid() && d->when) {
        /* if we switch the object at runtime, we need to restore the
           previous binding on the old object before continuing */
        d->when = false;
        eval();
        d->when = true;
    }
    d->obj = obj;
    if (d->componentComplete) {
        d->prop = QQmlProperty(d->obj, d->propName);
        d->validate(this);
    }
    eval();
}

/*!
    \qmlproperty string QtQml::Binding::property

    The property to be updated.

    This can be a group property if the expression results in accessing a
    property of a \l {QML Basic Types}{value type}. For example:

    \qml
    Item {
        id: item

        property rect rectangle: Qt.rect(0, 0, 200, 200)
    }

    Binding {
        target: item
        property: "rectangle.x"
        value: 100
    }
    \endqml
*/
QString QQmlBind::property() const
{
    Q_D(const QQmlBind);
    return d->propName;
}

void QQmlBind::setProperty(const QString &p)
{
    Q_D(QQmlBind);
    if (!d->propName.isEmpty() && d->when.isValid() && d->when) {
        /* if we switch the property name at runtime, we need to restore the
           previous binding on the old object before continuing */
        d->when = false;
        eval();
        d->when = true;
    }
    d->propName = p;
    if (d->componentComplete) {
        d->prop = QQmlProperty(d->obj, d->propName);
        d->validate(this);
    }
    eval();
}

/*!
    \qmlproperty any QtQml::Binding::value

    The value to be set on the target object and property.  This can be a
    constant (which isn't very useful), or a bound expression.
*/
QVariant QQmlBind::value() const
{
    Q_D(const QQmlBind);
    return d->value.value;
}

void QQmlBind::setValue(const QVariant &v)
{
    Q_D(QQmlBind);
    d->value = v;
    prepareEval();
}

/*!
    \qmlproperty bool QtQml::Binding::delayed
    \since 5.8

    This property holds whether the binding should be delayed.

    A delayed binding will not immediately update the target, but rather wait
    until the event queue has been cleared. This can be used as an optimization,
    or to prevent intermediary values from being assigned.

    \code
    Binding {
        target: contactName; property: 'text'
        value: givenName + " " + familyName; when: list.ListView.isCurrentItem
        delayed: true
    }
    \endcode
*/
bool QQmlBind::delayed() const
{
    Q_D(const QQmlBind);
    return d->delayed;
}

void QQmlBind::setDelayed(bool delayed)
{
    Q_D(QQmlBind);
    if (d->delayed == delayed)
        return;

    d->delayed = delayed;

    if (!d->delayed)
        eval();
}

void QQmlBind::setTarget(const QQmlProperty &p)
{
    Q_D(QQmlBind);
    d->prop = p;
}

void QQmlBind::classBegin()
{
    Q_D(QQmlBind);
    d->componentComplete = false;
}

void QQmlBind::componentComplete()
{
    Q_D(QQmlBind);
    d->componentComplete = true;
    if (!d->prop.isValid()) {
        d->prop = QQmlProperty(d->obj, d->propName);
        d->validate(this);
    }
    eval();
}

void QQmlBind::prepareEval()
{
    Q_D(QQmlBind);
    if (d->delayed) {
        if (!d->pendingEval)
            QTimer::singleShot(0, this, &QQmlBind::eval);
        d->pendingEval = true;
    } else {
        eval();
    }
}

void QQmlBind::eval()
{
    Q_D(QQmlBind);
    d->pendingEval = false;
    if (!d->prop.isValid() || d->value.isNull || !d->componentComplete)
        return;

    if (d->when.isValid()) {
        if (!d->when) {
            //restore any previous binding
            if (d->prevBind) {
                QQmlAbstractBinding::Ptr p = d->prevBind;
                d->prevBind = 0;
                QQmlPropertyPrivate::setBinding(p.data());
            }
            return;
        }

        //save any set binding for restoration
        if (!d->prevBind)
            d->prevBind = QQmlPropertyPrivate::binding(d->prop);
        QQmlPropertyPrivate::removeBinding(d->prop);
    }

    d->prop.write(d->value.value);
}

QT_END_NAMESPACE
