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

#include "private/qdeclarativebind_p.h"

#include "private/qdeclarativenullablevalue_p_p.h"
#include "private/qdeclarativeguard_p.h"

#include <qdeclarativeengine.h>
#include <qdeclarativecontext.h>
#include <qdeclarativeproperty.h>

#include <QtCore/qfile.h>
#include <QtCore/qdebug.h>
#include <QtScript/qscriptvalue.h>
#include <QtScript/qscriptcontext.h>
#include <QtScript/qscriptengine.h>

#include <private/qobject_p.h>

QT_BEGIN_NAMESPACE

class QDeclarativeBindPrivate : public QObjectPrivate
{
public:
    QDeclarativeBindPrivate() : when(true), componentComplete(true), obj(0) {}

    bool when : 1;
    bool componentComplete : 1;
    QDeclarativeGuard<QObject> obj;
    QString prop;
    QDeclarativeNullableValue<QVariant> value;
};


/*!
    \qmltype Binding
    \instantiates QDeclarativeBind
    \ingroup qml-working-with-data
    \since 4.7
    \brief The Binding element allows arbitrary property bindings to be created.

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

    If the binding target or binding property is changed, the bound value is
    immediately pushed onto the new target.

    \sa QtDeclarative
*/
QDeclarativeBind::QDeclarativeBind(QObject *parent)
    : QObject(*(new QDeclarativeBindPrivate), parent)
{
}

QDeclarativeBind::~QDeclarativeBind()
{
}

/*!
    \qmlproperty bool Binding::when

    This property holds when the binding is active.
    This should be set to an expression that evaluates to true when you want the binding to be active.

    \code
    Binding {
        target: contactName; property: 'text'
        value: name; when: list.ListView.isCurrentItem
    }
    \endcode
*/
bool QDeclarativeBind::when() const
{
    Q_D(const QDeclarativeBind);
    return d->when;
}

void QDeclarativeBind::setWhen(bool v)
{
    Q_D(QDeclarativeBind);
    d->when = v;
    eval();
}

/*!
    \qmlproperty Object Binding::target

    The object to be updated.
*/
QObject *QDeclarativeBind::object()
{
    Q_D(const QDeclarativeBind);
    return d->obj;
}

void QDeclarativeBind::setObject(QObject *obj)
{
    Q_D(QDeclarativeBind);
    d->obj = obj;
    eval();
}

/*!
    \qmlproperty string Binding::property

    The property to be updated.
*/
QString QDeclarativeBind::property() const
{
    Q_D(const QDeclarativeBind);
    return d->prop;
}

void QDeclarativeBind::setProperty(const QString &p)
{
    Q_D(QDeclarativeBind);
    d->prop = p;
    eval();
}

/*!
    \qmlproperty any Binding::value

    The value to be set on the target object and property.  This can be a
    constant (which isn't very useful), or a bound expression.
*/
QVariant QDeclarativeBind::value() const
{
    Q_D(const QDeclarativeBind);
    return d->value.value;
}

void QDeclarativeBind::setValue(const QVariant &v)
{
    Q_D(QDeclarativeBind);
    d->value.value = v;
    d->value.isNull = false;
    eval();
}

void QDeclarativeBind::classBegin()
{
    Q_D(QDeclarativeBind);
    d->componentComplete = false;
}

void QDeclarativeBind::componentComplete()
{
    Q_D(QDeclarativeBind);
    d->componentComplete = true;
    eval();
}

void QDeclarativeBind::eval()
{
    Q_D(QDeclarativeBind);
    if (!d->obj || d->value.isNull || !d->when || !d->componentComplete)
        return;

    QDeclarativeProperty prop(d->obj, d->prop);
    prop.write(d->value.value);
}

QT_END_NAMESPACE
