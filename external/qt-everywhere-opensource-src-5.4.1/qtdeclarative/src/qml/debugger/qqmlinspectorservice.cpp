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

#include "qqmlinspectorservice_p.h"
#include "qqmlinspectorinterface_p.h"
#include "qqmldebugserver_p.h"

#include <private/qqmlglobal_p.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QPluginLoader>

// print detailed information about loading of plugins
DEFINE_BOOL_CONFIG_OPTION(qmlDebugVerbose, QML_DEBUGGER_VERBOSE)

QT_BEGIN_NAMESPACE

Q_GLOBAL_STATIC(QQmlInspectorService, serviceInstance)

QQmlInspectorService::QQmlInspectorService()
    : QQmlDebugService(QStringLiteral("QmlInspector"), 1)
    , m_currentInspectorPlugin(0)
{
    registerService();
}

QQmlInspectorService *QQmlInspectorService::instance()
{
    return serviceInstance();
}

void QQmlInspectorService::addView(QObject *view)
{
    m_views.append(view);
    updateState();
}

void QQmlInspectorService::removeView(QObject *view)
{
    m_views.removeAll(view);
    updateState();
}

void QQmlInspectorService::sendMessage(const QByteArray &message)
{
    if (state() != Enabled)
        return;

    QQmlDebugService::sendMessage(message);
}

void QQmlInspectorService::stateChanged(State /*state*/)
{
    QMetaObject::invokeMethod(this, "updateState", Qt::QueuedConnection);
}

void QQmlInspectorService::updateState()
{
    if (m_views.isEmpty()) {
        if (m_currentInspectorPlugin) {
            m_currentInspectorPlugin->deactivate();
            m_currentInspectorPlugin = 0;
        }
        return;
    }

    if (state() == Enabled) {
        if (m_inspectorPlugins.isEmpty())
            loadInspectorPlugins();

        if (m_inspectorPlugins.isEmpty()) {
            qWarning() << "QQmlInspector: No plugins found.";
            QQmlDebugServer::instance()->removeService(this);
            return;
        }

        m_currentInspectorPlugin = 0;
        foreach (QQmlInspectorInterface *inspector, m_inspectorPlugins) {
            if (inspector->canHandleView(m_views.first())) {
                m_currentInspectorPlugin = inspector;
                break;
            }
        }

        if (!m_currentInspectorPlugin) {
            qWarning() << "QQmlInspector: No plugin available for view '" << m_views.first()->metaObject()->className() << "'.";
            return;
        }
        m_currentInspectorPlugin->activate(m_views.first());
    } else {
        if (m_currentInspectorPlugin) {
            m_currentInspectorPlugin->deactivate();
            m_currentInspectorPlugin = 0;
        }
    }
}

void QQmlInspectorService::messageReceived(const QByteArray &message)
{
    QMetaObject::invokeMethod(this, "processMessage", Qt::QueuedConnection, Q_ARG(QByteArray, message));
}

void QQmlInspectorService::processMessage(const QByteArray &message)
{
    if (m_currentInspectorPlugin)
        m_currentInspectorPlugin->clientMessage(message);
}

void QQmlInspectorService::loadInspectorPlugins()
{
    QStringList pluginCandidates;
    const QStringList paths = QCoreApplication::libraryPaths();
    foreach (const QString &libPath, paths) {
        const QDir dir(libPath + QLatin1String("/qmltooling"));
        if (dir.exists())
            foreach (const QString &pluginPath, dir.entryList(QDir::Files))
                pluginCandidates << dir.absoluteFilePath(pluginPath);
    }

    foreach (const QString &pluginPath, pluginCandidates) {
        if (pluginPath.contains(QLatin1String("qmldbg_tcp")))
            continue;
        if (qmlDebugVerbose())
            qDebug() << "QQmlInspector: Trying to load plugin " << pluginPath << "...";

        QPluginLoader loader(pluginPath);
        if (!loader.load()) {
            if (qmlDebugVerbose())
                qDebug() << "QQmlInspector: Error while loading: " << loader.errorString();

            continue;
        }

        QQmlInspectorInterface *inspector =
                qobject_cast<QQmlInspectorInterface*>(loader.instance());
        if (inspector) {
            if (qmlDebugVerbose())
                qDebug() << "QQmlInspector: Plugin successfully loaded.";
            m_inspectorPlugins << inspector;
        } else {
            if (qmlDebugVerbose())
                qDebug() << "QQmlInspector: Plugin does not implement interface QQmlInspectorInterface.";

            loader.unload();
        }
    }
}

QT_END_NAMESPACE
