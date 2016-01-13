/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtQml/qqmlextensionplugin.h>

#include "qquickwebenginehistory_p.h"
#include "qquickwebenginesettings_p.h"
#include "qquickwebenginesingleton_p.h"
#include "qquickwebengineview_p.h"
#include "qquickwebengineview_p_p.h"

QT_BEGIN_NAMESPACE

static QObject *webEngineSingletonProvider(QQmlEngine *, QJSEngine *)
{
    return new QQuickWebEngineSingleton;
}

class QQuickWebEngineViewExperimentalExtension : public QObject {
    Q_OBJECT
    Q_PROPERTY(QQuickWebEngineViewExperimental* experimental READ experimental CONSTANT FINAL)
public:
    QQuickWebEngineViewExperimentalExtension(QObject *parent = 0) : QObject(parent) { }
    QQuickWebEngineViewExperimental* experimental() { return static_cast<QQuickWebEngineView*>(parent())->d_func()->experimental(); }
};

class QtWebEngineExperimentalPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionInterface/1.0")
public:
    virtual void registerTypes(const char *uri)
    {
        qWarning("\nWARNING: This project is using the experimental QML API extensions for QtWebEngine and is therefore tied to a specific QtWebEngine release.\n"
                 "WARNING: The experimental API will change from version to version, or even be removed. You have been warned!\n");

        Q_ASSERT(QLatin1String(uri) == QLatin1String("QtWebEngine.experimental"));

        qmlRegisterExtendedType<QQuickWebEngineView, QQuickWebEngineViewExperimentalExtension>(uri, 1, 0, "WebEngineView");
        qmlRegisterUncreatableType<QQuickWebEngineViewExperimental>(uri, 1, 0, "WebEngineViewExperimental",
            QObject::tr("Cannot create a separate instance of WebEngineViewExperimental"));
        qmlRegisterUncreatableType<QQuickWebEngineViewport>(uri, 1, 0, "WebEngineViewport",
            QObject::tr("Cannot create a separate instance of WebEngineViewport"));
        qmlRegisterUncreatableType<QQuickWebEngineHistory>(uri, 1, 0, "NavigationHistory",
            QObject::tr("Cannot create a separate instance of NavigationHistory"));
        qmlRegisterUncreatableType<QQuickWebEngineHistoryListModel>(uri, 1, 0, "NavigationHistoryListModel",
            QObject::tr("Cannot create a separate instance of NavigationHistory"));
        qmlRegisterUncreatableType<QQuickWebEngineSettings>(uri, 1, 0, "WebEngineSettings",
            QObject::tr("Cannot create a separate instance of WebEngineSettings"));
        qmlRegisterSingletonType<QQuickWebEngineSingleton>(uri, 1, 0, "WebEngine", webEngineSingletonProvider);
    }
};

QT_END_NAMESPACE

#include "plugin.moc"
