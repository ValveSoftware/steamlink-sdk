/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

#include <QtQml/qqmlextensionplugin.h>
#include <QtWebEngine/QQuickWebEngineProfile>

#include "qquickwebenginecertificateerror_p.h"
#include "qquickwebenginecontextmenurequest_p.h"
#include "qquickwebenginedialogrequests_p.h"
#include "qquickwebenginedownloaditem_p.h"
#include "qquickwebenginehistory_p.h"
#include "qquickwebenginefaviconprovider_p_p.h"
#include "qquickwebengineloadrequest_p.h"
#include "qquickwebenginenavigationrequest_p.h"
#include "qquickwebenginenewviewrequest_p.h"
#include "qquickwebenginesettings_p.h"
#include "qquickwebenginesingleton_p.h"
#include "qquickwebengineview_p.h"
#include "qtwebengineversion.h"

QT_BEGIN_NAMESPACE

static QObject *webEngineSingletonProvider(QQmlEngine *, QJSEngine *)
{
    return new QQuickWebEngineSingleton;
}

class QtWebEnginePlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)
public:
    virtual void initializeEngine(QQmlEngine *engine, const char *uri) Q_DECL_OVERRIDE
    {
        Q_UNUSED(uri);
        engine->addImageProvider(QQuickWebEngineFaviconProvider::identifier(), new QQuickWebEngineFaviconProvider);
    }

    virtual void registerTypes(const char *uri) Q_DECL_OVERRIDE
    {
        Q_ASSERT(QLatin1String(uri) == QLatin1String("QtWebEngine"));

        qmlRegisterType<QQuickWebEngineView>(uri, 1, 0, "WebEngineView");
        qmlRegisterUncreatableType<QQuickWebEngineLoadRequest>(uri, 1, 0, "WebEngineLoadRequest", msgUncreatableType("WebEngineLoadRequest"));
        qmlRegisterUncreatableType<QQuickWebEngineNavigationRequest>(uri, 1, 0, "WebEngineNavigationRequest", msgUncreatableType("WebEngineNavigationRequest"));

        qmlRegisterType<QQuickWebEngineView, 1>(uri, 1, 1, "WebEngineView");
        qmlRegisterType<QQuickWebEngineView, 2>(uri, 1, 2, "WebEngineView");
        qmlRegisterType<QQuickWebEngineView, 3>(uri, 1, 3, "WebEngineView");
        qmlRegisterType<QQuickWebEngineView, 4>(uri, 1, 4, "WebEngineView");
        qmlRegisterType<QQuickWebEngineProfile>(uri, 1, 1, "WebEngineProfile");
        qmlRegisterType<QQuickWebEngineProfile, 1>(uri, 1, 2, "WebEngineProfile");
        qmlRegisterType<QQuickWebEngineProfile, 2>(uri, 1, 3, "WebEngineProfile");
        qmlRegisterType<QQuickWebEngineProfile, 3>(uri, 1, 4, "WebEngineProfile");
        qmlRegisterType<QQuickWebEngineScript>(uri, 1, 1, "WebEngineScript");
        qmlRegisterUncreatableType<QQuickWebEngineCertificateError>(uri, 1, 1, "WebEngineCertificateError", msgUncreatableType("WebEngineCertificateError"));
        qmlRegisterUncreatableType<QQuickWebEngineDownloadItem>(uri, 1, 1, "WebEngineDownloadItem",
            tr("Cannot create a separate instance of WebEngineDownloadItem"));
        qmlRegisterUncreatableType<QQuickWebEngineDownloadItem, 1>(uri, 1, 2, "WebEngineDownloadItem",
            tr("Cannot create a separate instance of WebEngineDownloadItem"));
        qmlRegisterUncreatableType<QQuickWebEngineDownloadItem, 2>(uri, 1, 3, "WebEngineDownloadItem",
            tr("Cannot create a separate instance of WebEngineDownloadItem"));
        qmlRegisterUncreatableType<QQuickWebEngineDownloadItem, 3>(uri, 1, 4, "WebEngineDownloadItem",
            tr("Cannot create a separate instance of WebEngineDownloadItem"));
        qmlRegisterUncreatableType<QQuickWebEngineNewViewRequest>(uri, 1, 1, "WebEngineNewViewRequest", msgUncreatableType("WebEngineNewViewRequest"));
        qmlRegisterUncreatableType<QQuickWebEngineSettings>(uri, 1, 1, "WebEngineSettings", tr("Cannot create a separate instance of WebEngineSettings"));
        qmlRegisterUncreatableType<QQuickWebEngineSettings, 1>(uri, 1, 2, "WebEngineSettings", tr("Cannot create a separate instance of WebEngineSettings"));
        qmlRegisterUncreatableType<QQuickWebEngineSettings, 2>(uri, 1, 3, "WebEngineSettings", tr("Cannot create a separate instance of WebEngineSettings"));
        qmlRegisterUncreatableType<QQuickWebEngineSettings, 3>(uri, 1, 4, "WebEngineSettings", tr("Cannot create a separate instance of WebEngineSettings"));
        qmlRegisterSingletonType<QQuickWebEngineSingleton>(uri, 1, 1, "WebEngine", webEngineSingletonProvider);
        qmlRegisterUncreatableType<QQuickWebEngineHistory>(uri, 1, 1, "NavigationHistory",
            tr("Cannot create a separate instance of NavigationHistory"));
        qmlRegisterUncreatableType<QQuickWebEngineHistoryListModel>(uri, 1, 1, "NavigationHistoryListModel",
            tr("Cannot create a separate instance of NavigationHistory"));
        qmlRegisterUncreatableType<QQuickWebEngineFullScreenRequest>(uri, 1, 1, "FullScreenRequest",
            tr("Cannot create a separate instance of FullScreenRequest"));

        qmlRegisterUncreatableType<QQuickWebEngineContextMenuRequest>(uri, 1, 4, "ContextMenuRequest",
                                                                    msgUncreatableType("ContextMenuRequest"));
        qmlRegisterUncreatableType<QQuickWebEngineAuthenticationDialogRequest>(uri, 1, 4, "AuthenticationDialogRequest",
                                                                       msgUncreatableType("AuthenticationDialogRequest"));
        qmlRegisterUncreatableType<QQuickWebEngineJavaScriptDialogRequest>(uri, 1, 4, "JavaScriptDialogRequest",
                                                                         msgUncreatableType("JavaScriptDialogRequest"));
        qmlRegisterUncreatableType<QQuickWebEngineColorDialogRequest>(uri, 1, 4, "ColorDialogRequest",
                                                                         msgUncreatableType("ColorDialogRequest"));
        qmlRegisterUncreatableType<QQuickWebEngineFileDialogRequest>(uri, 1, 4, "FileDialogRequest",
                                                                         msgUncreatableType("FileDialogRequest"));
        qmlRegisterUncreatableType<QQuickWebEngineFormValidationMessageRequest>(uri, 1, 4, "FormValidationMessageRequest",
                                                                         msgUncreatableType("FormValidationMessageRequest"));

        // For now (1.x import), the latest revision matches the minor version of the import.
        qmlRegisterRevision<QQuickWebEngineView, LATEST_WEBENGINEVIEW_REVISION>(uri, 1, LATEST_WEBENGINEVIEW_REVISION);
    }

private:
    static QString msgUncreatableType(const char *className)
    {
        return tr("Cannot create separate instance of %1").arg(QLatin1String(className));
    }
};

QT_END_NAMESPACE

#include "plugin.moc"
