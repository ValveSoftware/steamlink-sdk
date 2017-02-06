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

#include "qqmldebug.h"
#include "qqmldebugconnector_p.h"
#include "qqmldebugserviceinterfaces_p.h"

#include <private/qqmlengine_p.h>

QT_BEGIN_NAMESPACE

QQmlDebuggingEnabler::QQmlDebuggingEnabler(bool printWarning)
{
    if (!QQmlEnginePrivate::qml_debugging_enabled
            && printWarning) {
        qDebug("QML debugging is enabled. Only use this in a safe environment.");
    }
    QQmlEnginePrivate::qml_debugging_enabled = true;
}

/*!
 * Retrieves the plugin keys of the debugger services provided by default. The debugger services
 * enable a debug client to use a Qml/JavaScript debugger, in order to set breakpoints, pause
 * execution, evaluate expressions and similar debugging tasks.
 * \return List of plugin keys of default debugger services.
 */
QStringList QQmlDebuggingEnabler::debuggerServices()
{
    return QStringList() << QV4DebugService::s_key << QQmlEngineDebugService::s_key
                         << QDebugMessageService::s_key;
}

/*!
 * Retrieves the plugin keys of the inspector services provided by default. The inspector services
 * enable a debug client to use a visual inspector tool for Qt Quick.
 * \return List of plugin keys of default inspector services.
 */
QStringList QQmlDebuggingEnabler::inspectorServices()
{
    return QStringList() << QQmlInspectorService::s_key;
}

/*!
 * Retrieves the names of the profiler services provided by default. The profiler services enable a
 * debug client to use a profiler and track the time taken by various QML and JavaScript constructs,
 * as well as the QtQuick SceneGraph.
 * \return List of plugin keys of default profiler services.
 */
QStringList QQmlDebuggingEnabler::profilerServices()
{
    return QStringList() << QQmlProfilerService::s_key << QQmlEngineControlService::s_key;
}

/*!
 * Restricts the services available from the debug connector. The connector will scan plugins in the
 * "qmltooling" subdirectory of the default plugin path. If this function is not called before the
 * debug connector is enabled, all services found that way will be available to any client. If this
 * function is called, only the services with plugin keys given in \a services will be available.
 *
 * Use this method to disable debugger and inspector services when profiling to get better
 * performance and more realistic profiles. The debugger service will put any JavaScript engine it
 * connects to into interpreted mode, disabling the JIT compiler.
 *
 * \sa debuggerServices(), profilerServices(), inspectorServices()
 */
void QQmlDebuggingEnabler::setServices(const QStringList &services)
{
    QQmlDebugConnector::setServices(services);
}

/*!
 * \enum QQmlDebuggingEnabler::StartMode
 *
 * Defines the debug connector's start behavior. You can interrupt QML engines starting while a
 * debug client is connecting, in order to set breakpoints in or profile startup code.
 *
 * \value DoNotWaitForClient Run any QML engines as usual while the debug services are connecting.
 * \value WaitForClient      If a QML engine starts while the debug services are connecting,
 *                           interrupt it until they are done.
 */

/*!
 * Enables debugging for QML engines created after calling this function. The debug connector will
 * listen on \a port at \a hostName and block the QML engine until it receives a connection if
 * \a mode is \c WaitForClient. If \a mode is not specified it won't block and if \a hostName is not
 * specified it will listen on all available interfaces. You can only start one debug connector at a
 * time. A debug connector may have already been started if the -qmljsdebugger= command line
 * argument was given. This method returns \c true if a new debug connector was successfully
 * started, or \c false otherwise.
 */
bool QQmlDebuggingEnabler::startTcpDebugServer(int port, StartMode mode, const QString &hostName)
{
    QVariantHash configuration;
    configuration[QLatin1String("portFrom")] = configuration[QLatin1String("portTo")] = port;
    configuration[QLatin1String("block")] = (mode == WaitForClient);
    configuration[QLatin1String("hostAddress")] = hostName;
    return startDebugConnector(QLatin1String("QQmlDebugServer"), configuration);
}

/*!
 * \since 5.6
 *
 * Enables debugging for QML engines created after calling this function. The debug connector will
 * connect to a debugger waiting on a local socket at the given \a socketFileName and block the QML
 * engine until the connection is established if \a mode is \c WaitForClient. If \a mode is not
 * specified it will not block. You can only start one debug connector at a time. A debug connector
 * may have already been started if the -qmljsdebugger= command line argument was given. This method
 * returns \c true if a new debug connector was successfully started, or \c false otherwise.
 */
bool QQmlDebuggingEnabler::connectToLocalDebugger(const QString &socketFileName, StartMode mode)
{
    QVariantHash configuration;
    configuration[QLatin1String("fileName")] = socketFileName;
    configuration[QLatin1String("block")] = (mode == WaitForClient);
    return startDebugConnector(QLatin1String("QQmlDebugServer"), configuration);
}

/*!
 * \since 5.7
 *
 * Enables debugging for QML engines created after calling this function. A debug connector plugin
 * specified by \a pluginName will be loaded and started using the given \a configuration. Supported
 * configuration entries and their semantics depend on the plugin being loaded. You can only start
 * one debug connector at a time. A debug connector may have already been started if the
 * -qmljsdebugger= command line argument was given. This method returns \c true if a new debug
 * connector was successfully started, or \c false otherwise.
 */
bool QQmlDebuggingEnabler::startDebugConnector(const QString &pluginName,
                                               const QVariantHash &configuration)
{
    QQmlDebugConnector::setPluginKey(pluginName);
    QQmlDebugConnector *connector = QQmlDebugConnector::instance();
    return connector ? connector->open(configuration) : false;
}

enum { HookCount = 3 };

// Only add to the end, and bump version if you do.
quintptr Q_QML_EXPORT qtDeclarativeHookData[] = {
    // Version of this Array. Bump if you add to end.
    1,

    // Number of entries in this array.
    HookCount,

    // TypeInformationVersion, an integral value, bumped whenever private
    // object sizes or member offsets that are used in Qt Creator's
    // data structure "pretty printing" change.
    2
};

Q_STATIC_ASSERT(HookCount == sizeof(qtDeclarativeHookData) / sizeof(qtDeclarativeHookData[0]));

QT_END_NAMESPACE
