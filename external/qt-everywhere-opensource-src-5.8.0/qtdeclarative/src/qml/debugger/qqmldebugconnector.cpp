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

#include "qqmldebugpluginmanager_p.h"
#include "qqmldebugconnector_p.h"
#include "qqmldebugservicefactory_p.h"
#include <QtCore/QPluginLoader>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QDebug>
#include <QtCore/QJsonArray>
#include <QtCore/QDataStream>

#include <private/qcoreapplication_p.h>
#include <private/qqmlengine_p.h>

QT_BEGIN_NAMESPACE

// Connectors. We could add more plugins here, and distinguish by arguments to instance()
Q_QML_DEBUG_PLUGIN_LOADER(QQmlDebugConnector)

// Services
Q_QML_DEBUG_PLUGIN_LOADER(QQmlDebugService)

int QQmlDebugConnector::s_dataStreamVersion = QDataStream::Qt_4_7;

struct QQmlDebugConnectorParams {
    QString pluginKey;
    QStringList services;
    QString arguments;
    QQmlDebugConnector *instance;

    QQmlDebugConnectorParams() : instance(0)
    {
        if (qApp) {
            QCoreApplicationPrivate *appD =
                    static_cast<QCoreApplicationPrivate*>(QObjectPrivate::get(qApp));
            if (appD)
                arguments = appD->qmljsDebugArgumentsString();
        }
    }
};

Q_GLOBAL_STATIC(QQmlDebugConnectorParams, qmlDebugConnectorParams)

void QQmlDebugConnector::setPluginKey(const QString &key)
{
    QQmlDebugConnectorParams *params = qmlDebugConnectorParams();
    if (params) {
        if (params->instance)
            qWarning() << "QML debugger: Cannot set plugin key after loading the plugin.";
        else
            params->pluginKey = key;
    }
}

void QQmlDebugConnector::setServices(const QStringList &services)
{
    QQmlDebugConnectorParams *params = qmlDebugConnectorParams();
    if (params)
        params->services = services;
}

QString QQmlDebugConnector::commandLineArguments()
{
    QQmlDebugConnectorParams *params = qmlDebugConnectorParams();
    if (!params)
        return QString();
    return params->arguments;
}

QQmlDebugConnector *QQmlDebugConnector::instance()
{
    QQmlDebugConnectorParams *params = qmlDebugConnectorParams();
    if (!params)
        return 0;

    if (!QQmlEnginePrivate::qml_debugging_enabled) {
        if (!params->arguments.isEmpty()) {
            qWarning().noquote() << QString::fromLatin1(
                                        "QML Debugger: Ignoring \"-qmljsdebugger=%1\". Debugging "
                                        "has not been enabled.").arg(params->arguments);
            params->arguments.clear();
        }
        return 0;
    }

    if (!params->instance) {
        if (!params->pluginKey.isEmpty()) {
            params->instance = loadQQmlDebugConnector(params->pluginKey);
        } else if (params->arguments.isEmpty()) {
            return 0; // no explicit class name given and no command line arguments
        } else {
            params->instance = loadQQmlDebugConnector(
                        params->arguments.startsWith(QLatin1String("native")) ?
                            QStringLiteral("QQmlNativeDebugConnector") :
                            QStringLiteral("QQmlDebugServer"));
        }

        if (params->instance) {
            foreach (const QJsonObject &object, metaDataForQQmlDebugService()) {
                foreach (const QJsonValue &key, object.value(QLatin1String("MetaData")).toObject()
                         .value(QLatin1String("Keys")).toArray()) {
                    QString keyString = key.toString();
                    if (params->services.isEmpty() || params->services.contains(keyString))
                        loadQQmlDebugService(keyString);
                }
            }
        }
    }

    return params->instance;
}

QQmlDebugConnectorFactory::~QQmlDebugConnectorFactory()
{
    // This is triggered when the plugin is unloaded.
    QQmlDebugConnectorParams *params = qmlDebugConnectorParams();
    if (params) {
        params->pluginKey.clear();
        params->arguments.clear();
        params->services.clear();
        delete params->instance;
        params->instance = 0;
    }
}

QT_END_NAMESPACE
