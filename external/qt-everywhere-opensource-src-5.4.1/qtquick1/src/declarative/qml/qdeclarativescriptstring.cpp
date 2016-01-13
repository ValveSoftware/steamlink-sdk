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

#include "qdeclarativescriptstring.h"

QT_BEGIN_NAMESPACE

class QDeclarativeScriptStringPrivate : public QSharedData
{
public:
    QDeclarativeScriptStringPrivate() : context(0), scope(0) {}

    QDeclarativeContext *context;
    QObject *scope;
    QString script;
};

/*!
\class QDeclarativeScriptString
\since 4.7
\brief The QDeclarativeScriptString class encapsulates a script and its context.

QDeclarativeScriptString is used to create QObject properties that accept a script "assignment" from QML.

Normally, the following QML would result in a binding being established for the \c script
property; i.e. \c script would be assigned the value obtained from running \c {myObj.value = Math.max(myValue, 100)}

\qml
MyType {
    script: myObj.value = Math.max(myValue, 100)
}
\endqml

If instead the property had a type of QDeclarativeScriptString,
the script itself -- \e {myObj.value = Math.max(myValue, 100)} -- would be passed to the \c script property
and the class could choose how to handle it. Typically, the class will evaluate
the script at some later time using a QDeclarativeExpression.

\code
QDeclarativeExpression expr(scriptString.context(), scriptString.script(), scriptStr.scopeObject());
expr.value();
\endcode

\sa QDeclarativeExpression
*/

/*!
Constructs an empty instance.
*/
QDeclarativeScriptString::QDeclarativeScriptString()
:  d(new QDeclarativeScriptStringPrivate)
{
}

/*!
Copies \a other.
*/
QDeclarativeScriptString::QDeclarativeScriptString(const QDeclarativeScriptString &other)
: d(other.d)
{
}

/*!
\internal
*/
QDeclarativeScriptString::~QDeclarativeScriptString()
{
}

/*!
Assigns \a other to this.
*/
QDeclarativeScriptString &QDeclarativeScriptString::operator=(const QDeclarativeScriptString &other)
{
    d = other.d;
    return *this;
}

/*!
Returns the context for the script.
*/
QDeclarativeContext *QDeclarativeScriptString::context() const
{
    return d->context;
}

/*!
Sets the \a context for the script.
*/
void QDeclarativeScriptString::setContext(QDeclarativeContext *context)
{
    d->context = context;
}

/*!
Returns the scope object for the script.
*/
QObject *QDeclarativeScriptString::scopeObject() const
{
    return d->scope;
}

/*!
Sets the scope \a object for the script.
*/
void QDeclarativeScriptString::setScopeObject(QObject *object)
{
    d->scope = object;
}

/*!
Returns the script text.
*/
QString QDeclarativeScriptString::script() const
{
    return d->script;
}

/*!
Sets the \a script text.
*/
void QDeclarativeScriptString::setScript(const QString &script)
{
    d->script = script;
}

QT_END_NAMESPACE

