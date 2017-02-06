/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquickstatechangescript_p.h"

#include <qqml.h>
#include <qqmlcontext.h>
#include <qqmlexpression.h>
#include <qqmlinfo.h>
#include <private/qqmlcontext_p.h>
#include <private/qqmlproperty_p.h>
#include <private/qqmlbinding_p.h>
#include "qquickstate_p_p.h"

#include <QtCore/qdebug.h>
#include <QtCore/qmath.h>

#include <private/qobject_p.h>

QT_BEGIN_NAMESPACE

class QQuickStateChangeScriptPrivate : public QQuickStateOperationPrivate
{
public:
    QQuickStateChangeScriptPrivate() {}

    QQmlScriptString script;
    QString name;
};

/*!
    \qmltype StateChangeScript
    \instantiates QQuickStateChangeScript
    \inqmlmodule QtQuick
    \ingroup qtquick-states
    \brief Specifies how to run a script in a state

    A StateChangeScript is run upon entering a state. You can optionally use
    ScriptAction to specify the point in the transition at which
    the StateChangeScript should be run.

    \snippet qml/states/statechangescript.qml state and transition

    \sa ScriptAction
*/

QQuickStateChangeScript::QQuickStateChangeScript(QObject *parent)
: QQuickStateOperation(*(new QQuickStateChangeScriptPrivate), parent)
{
}

QQuickStateChangeScript::~QQuickStateChangeScript()
{
}

/*!
    \qmlproperty script QtQuick::StateChangeScript::script
    This property holds the script to run when the state is current.
*/
QQmlScriptString QQuickStateChangeScript::script() const
{
    Q_D(const QQuickStateChangeScript);
    return d->script;
}

void QQuickStateChangeScript::setScript(const QQmlScriptString &s)
{
    Q_D(QQuickStateChangeScript);
    d->script = s;
}

/*!
    \qmlproperty string QtQuick::StateChangeScript::name
    This property holds the name of the script. This name can be used by a
    ScriptAction to target a specific script.

    \sa ScriptAction::scriptName
*/
QString QQuickStateChangeScript::name() const
{
    Q_D(const QQuickStateChangeScript);
    return d->name;
}

void QQuickStateChangeScript::setName(const QString &n)
{
    Q_D(QQuickStateChangeScript);
    d->name = n;
}

void QQuickStateChangeScript::execute()
{
    Q_D(QQuickStateChangeScript);
    if (!d->script.isEmpty()) {
        QQmlExpression expr(d->script);
        expr.evaluate();
        if (expr.hasError())
            qmlInfo(this, expr.error());
    }
}

QQuickStateChangeScript::ActionList QQuickStateChangeScript::actions()
{
    ActionList rv;
    QQuickStateAction a;
    a.event = this;
    rv << a;
    return rv;
}

QQuickStateActionEvent::EventType QQuickStateChangeScript::type() const
{
    return Script;
}


#include <moc_qquickstatechangescript_p.cpp>

QT_END_NAMESPACE

