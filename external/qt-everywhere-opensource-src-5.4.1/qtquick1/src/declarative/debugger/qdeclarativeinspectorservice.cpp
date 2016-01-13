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

#include "private/qdeclarativeinspectorservice_p.h"
#include "private/qdeclarativeinspectorinterface_p.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QPluginLoader>

#include <QtDeclarative/QDeclarativeView>

QT_BEGIN_NAMESPACE

Q_GLOBAL_STATIC(QDeclarativeInspectorService, serviceInstance)

QDeclarativeInspectorService::QDeclarativeInspectorService()
    : QDeclarativeDebugService(QLatin1String("QDeclarativeObserverMode"))
    , m_inspectorPlugin(0)
{
}

QDeclarativeInspectorService *QDeclarativeInspectorService::instance()
{
    return serviceInstance();
}

void QDeclarativeInspectorService::addView(QDeclarativeView *view)
{
    m_views.append(view);
    updateStatus();
}

void QDeclarativeInspectorService::removeView(QDeclarativeView *view)
{
    m_views.removeAll(view);
    updateStatus();
}

void QDeclarativeInspectorService::sendMessage(const QByteArray &message)
{
    if (status() != Enabled)
        return;

    QDeclarativeDebugService::sendMessage(message);
}

void QDeclarativeInspectorService::statusChanged(Status status)
{
    Q_UNUSED(status)
    updateStatus();
}

void QDeclarativeInspectorService::updateStatus()
{
    if (m_views.isEmpty()) {
        if (m_inspectorPlugin)
            m_inspectorPlugin->deactivate();
        return;
    }

    if (status() == Enabled) {
        if (!m_inspectorPlugin)
            m_inspectorPlugin = loadInspectorPlugin();

        if (!m_inspectorPlugin) {
            qWarning() << "Error while loading inspector plugin";
            return;
        }

        m_inspectorPlugin->activate();
    } else {
        if (m_inspectorPlugin)
            m_inspectorPlugin->deactivate();
    }
}

void QDeclarativeInspectorService::messageReceived(const QByteArray &message)
{
    emit gotMessage(message);
}

QDeclarativeInspectorInterface *QDeclarativeInspectorService::loadInspectorPlugin()
{
    QStringList pluginCandidates;
    const QStringList paths = QCoreApplication::libraryPaths();
    foreach (const QString &libPath, paths) {
        const QDir dir(libPath + QLatin1String("/qml1tooling"));
        if (dir.exists())
            foreach (const QString &pluginPath, dir.entryList(QDir::Files))
                pluginCandidates << dir.absoluteFilePath(pluginPath);
    }

    foreach (const QString &pluginPath, pluginCandidates) {
        QPluginLoader loader(pluginPath);
        if (!loader.load())
            continue;

        QDeclarativeInspectorInterface *inspector =
                qobject_cast<QDeclarativeInspectorInterface*>(loader.instance());

        if (inspector)
            return inspector;
        loader.unload();
    }
    return 0;
}

QT_END_NAMESPACE
