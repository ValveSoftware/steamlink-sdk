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

#include "qqmlbind_p.h"

#include <private/qqmlnullablevalue_p_p.h>
#include <private/qqmlproperty_p.h>
#include <private/qqmlbinding_p.h>

#include <qqmlengine.h>
#include <qqmlcontext.h>
#include <qqmlproperty.h>
#include <qqmlinfo.h>

#include <QtCore/qfile.h>
#include <QtCore/qdebug.h>

#include <private/qobject_p.h>

QT_BEGIN_NAMESPACE

class QQmlBindPrivate : public QObjectPrivate
{
public:
    QQmlBindPrivate() : componentComplete(true), obj(0), prevBind(0) {}
    ~QQmlBindPrivate() { if (prevBind) prevBind->destroy(); }

    QQmlNullableValue<bool> when;
    bool componentComplete;
    QPointer<QObject> obj;
    QString propName;
    QQmlNullableValue<QVariant> value;
    QQmlProperty prop;
    QQmlAbstractBinding *prevBind;
};


/*!
    \qmltype Binding
    \instantiates QQmlBind
    \inqmlmodule QtQml
    \ingroup qtquick-interceptors
    \brief Enables the arbitrary creation of property bindings

    \section1 Binding to an Inaccessible Property

    Sometimes it is necessary to bind to a property of an object that wasn't
    directly instantiated by QML - generally a property of a class exported
    to QML by C++. In these cases, regular property binding doesn't work. Binding
    allows you to bind any value to any property.

    For example, imagine a C++ application that maps an "app.enteredText"
    property into QML. You could use Binding to update the enteredText property
    like this.
    \code
    TextEdit { id: myTextField; text: "Please type here..." }
    Binding { target: app; property: "enteredText"; value: myTextField.text }
    \endcode
    Whenever the text in the TextEdit is updated, the C++ property will be
    updated also.

    \section1 "Single-branch" conditional binding

    In some circumstances you may want to control the value of a property
    only when a certain condition is true (and relinquish control in all
    other circumstances). This often isn't possible to accomplish with a direct
    binding, as you need to supply values for all possible branches.

    \code
    // produces warning: "Unable to assign [undefined] to double value"
    value: if (mouse.pressed) mouse.mouseX
    \endcode

    The above example will produce a warning whenever we release the mouse, as the value
    of the binding is undefined when the mouse isn't pressed. We can use the Binding
    type to rewrite the above code and avoid the warning.

    \code
    Binding on value {
        when: mouse.pressed
        value: mouse.mouseX
    }
    \endcode

    The Binding type will also restore any previously set direct bindings on
    the property. In that sense, it functions much like a simplified State.

    \qml
    // this is equivalent to the above Binding
    State {
        name: "pressed"
        when: mouse.pressed
        PropertyChanges {
            target: obj
            value: mouse.mouseX
        }
    }
    \endqml

    If the binding target or binding property is changed, the bound value is
    immediately pushed onto the new target.

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
    if (d->componentComplete)
        d->prop = QQmlProperty(d->obj, d->propName);
    eval();
}

/*!
    \qmlproperty string QtQml::Binding::property

    The property to be updated.
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
    if (d->componentComplete)
        d->prop = QQmlProperty(d->obj, d->propName);
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
    if (!d->prop.isValid())
        d->prop = QQmlProperty(d->obj, d->propName);
    eval();
}

void QQmlBind::eval()
{
    Q_D(QQmlBind);
    if (!d->prop.isValid() || d->value.isNull || !d->componentComplete)
        return;

    if (d->when.isValid()) {
        if (!d->when) {
            //restore any previous binding
            if (d->prevBind) {
                QQmlAbstractBinding *tmp = d->prevBind;
                d->prevBind = 0;
                tmp = QQmlPropertyPrivate::setBinding(d->prop, tmp);
                if (tmp) //should this ever be true?
                    tmp->destroy();
            }
            return;
        }

        //save any set binding for restoration
        QQmlAbstractBinding *tmp;
        tmp = QQmlPropertyPrivate::setBinding(d->prop, 0);
        if (tmp && d->prevBind)
            tmp->destroy();
        else if (!d->prevBind)
            d->prevBind = tmp;
    }

    d->prop.write(d->value.value);
}

QT_END_NAMESPACE
