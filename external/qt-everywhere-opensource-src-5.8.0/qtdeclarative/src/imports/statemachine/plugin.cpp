/****************************************************************************
**
** Copyright (C) 2016 Ford Motor Company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "finalstate.h"
#include "signaltransition.h"
#include "state.h"
#include "statemachine.h"
#include "timeouttransition.h"

#include <QHistoryState>
#include <QQmlExtensionPlugin>
#include <qqml.h>

static void initResources()
{
#ifdef QT_STATIC
    Q_INIT_RESOURCE(qmake_QtQml_StateMachine);
#endif
}

QT_BEGIN_NAMESPACE

class QtQmlStateMachinePlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    QtQmlStateMachinePlugin(QObject *parent = 0) : QQmlExtensionPlugin(parent) { initResources(); }
    void registerTypes(const char *uri) Q_DECL_OVERRIDE
    {
        qmlRegisterType<State>(uri, 1, 0, "State");
        qmlRegisterType<StateMachine>(uri, 1, 0, "StateMachine");
        qmlRegisterType<QHistoryState>(uri, 1, 0, "HistoryState");
        qmlRegisterType<FinalState>(uri, 1, 0, "FinalState");
        qmlRegisterUncreatableType<QState>(uri, 1, 0, "QState", "Don't use this, use State instead");
        qmlRegisterUncreatableType<QAbstractState>(uri, 1, 0, "QAbstractState", "Don't use this, use State instead");
        qmlRegisterUncreatableType<QSignalTransition>(uri, 1, 0, "QSignalTransition", "Don't use this, use SignalTransition instead");
        qmlRegisterCustomType<SignalTransition>(uri, 1, 0, "SignalTransition", new SignalTransitionParser);
        qmlRegisterType<TimeoutTransition>(uri, 1, 0, "TimeoutTransition");
        qmlProtectModule(uri, 1);
    }
};

QT_END_NAMESPACE

#include "plugin.moc"
