/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Dialogs module of the Qt Toolkit.
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

#include <QtQml/qqml.h>
#include <QtQml/qqmlextensionplugin.h>
#include <QtQml/qqmlcomponent.h>
#include "qquickmessagedialog_p.h"
#include "qquickabstractmessagedialog_p.h"
#include "qquickdialogassets_p.h"
#include "qquickplatformmessagedialog_p.h"
#include "qquickfiledialog_p.h"
#include "qquickabstractfiledialog_p.h"
#include "qquickplatformfiledialog_p.h"
#include "qquickcolordialog_p.h"
#include "qquickabstractcolordialog_p.h"
#include "qquickplatformcolordialog_p.h"
#include "qquickfontdialog_p.h"
#include "qquickabstractfontdialog_p.h"
#include "qquickplatformfontdialog_p.h"
#include "qquickdialog_p.h"
#include <private/qguiapplication_p.h>
#include <qpa/qplatformintegration.h>
#include <QTouchDevice>

//#define PURE_QML_ONLY

Q_LOGGING_CATEGORY(lcRegistration, "qt.quick.dialogs.registration")

static void initResources()
{
#ifdef QT_STATIC
    Q_INIT_RESOURCE(qmake_QtQuick_Dialogs);
#else
    Q_INIT_RESOURCE(dialogs);
#endif
}

QT_BEGIN_NAMESPACE

/*!
    \qmlmodule QtQuick.Dialogs 1.2
    \title Qt Quick Dialogs QML Types
    \ingroup qmlmodules
    \brief Provides QML types for standard file, color picker and message dialogs

    This QML module contains types for creating and interacting with system dialogs.

    To use the types in this module, import the module with the following line:

    \code
    import QtQuick.Dialogs 1.2
    \endcode
*/

class QtQuick2DialogsPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    QtQuick2DialogsPlugin() : QQmlExtensionPlugin(), m_useResources(true) { initResources(); }

    virtual void initializeEngine(QQmlEngine *engine, const char * uri) {
        qCDebug(lcRegistration) << uri << m_decorationComponentUrl;
        QQuickAbstractDialog::m_decorationComponent =
            new QQmlComponent(engine, m_decorationComponentUrl, QQmlComponent::Asynchronous);
    }

    virtual void registerTypes(const char *uri) {
        Q_ASSERT(QLatin1String(uri) == QLatin1String("QtQuick.Dialogs"));
        bool hasTopLevelWindows = QGuiApplicationPrivate::platformIntegration()->
            hasCapability(QPlatformIntegration::MultipleWindows);
        qCDebug(lcRegistration) << uri << "can use top-level windows?" << hasTopLevelWindows;
        QDir qmlDir(baseUrl().toLocalFile());
        QDir widgetsDir(baseUrl().toLocalFile());
#ifndef QT_STATIC
        widgetsDir.cd("../PrivateWidgets");
#endif
#ifdef QT_STATIC
        m_useResources = false;
#else
#ifndef ALWAYS_LOAD_FROM_RESOURCES
        // If at least one file was actually installed, then use installed qml files instead of resources.
        // This makes debugging and incremental development easier, whereas the "normal" installation
        // uses resources to save space and cut down on the number of files to deploy.
        if (qmlDir.exists(QString("DefaultFileDialog.qml")))
            m_useResources = false;
#endif
#endif
        m_decorationComponentUrl = m_useResources ?
            QUrl("qrc:/QtQuick/Dialogs/qml/DefaultWindowDecoration.qml") :
#ifndef QT_STATIC
            QUrl::fromLocalFile(qmlDir.filePath(QString("qml/DefaultWindowDecoration.qml")));
#else
            QUrl("qrc:/qt-project.org/imports/QtQuick/Dialogs/qml/DefaultWindowDecoration.qml");
#endif
        // Prefer the QPA dialog helpers if the platform supports them.
        // Else if there is a QWidget-based implementation, check whether it's
        // possible to instantiate it from Qt Quick.
        // Otherwise fall back to a pure-QML implementation.

        // MessageDialog
        qmlRegisterUncreatableType<QQuickStandardButton>(uri, 1, 1, "StandardButton",
            QLatin1String("Do not create objects of type StandardButton"));
        qmlRegisterUncreatableType<QQuickStandardIcon>(uri, 1, 1, "StandardIcon",
            QLatin1String("Do not create objects of type StandardIcon"));
#ifndef PURE_QML_ONLY
        if (QGuiApplicationPrivate::platformTheme()->usePlatformNativeDialog(QPlatformTheme::MessageDialog))
            qmlRegisterType<QQuickPlatformMessageDialog>(uri, 1, 0, "MessageDialog");
        else
#endif
            registerWidgetOrQmlImplementation<QQuickMessageDialog>(widgetsDir, qmlDir, "MessageDialog", uri, hasTopLevelWindows, 1, 1);

        // FileDialog
#ifndef PURE_QML_ONLY
        // We register the QML version of the filedialog, even if the platform has native support.
        // QQuickAbstractDialog::setVisible() will check if a native dialog can be shown, and
        // only fall back to use the QML version if showing fails.
        if (QGuiApplicationPrivate::platformTheme()->usePlatformNativeDialog(QPlatformTheme::FileDialog))
            registerQmlImplementation<QQuickPlatformFileDialog>(qmlDir, "FileDialog", uri, 1, 0);
        else
#endif
            registerWidgetOrQmlImplementation<QQuickFileDialog>(widgetsDir, qmlDir, "FileDialog", uri, hasTopLevelWindows, 1, 0);

        // ColorDialog
#ifndef PURE_QML_ONLY
        if (QGuiApplicationPrivate::platformTheme()->usePlatformNativeDialog(QPlatformTheme::ColorDialog))
            qmlRegisterType<QQuickPlatformColorDialog>(uri, 1, 0, "ColorDialog");
        else
#endif
            registerWidgetOrQmlImplementation<QQuickColorDialog>(widgetsDir, qmlDir, "ColorDialog", uri, hasTopLevelWindows, 1, 0);

        // FontDialog
#ifndef PURE_QML_ONLY
        if (QGuiApplicationPrivate::platformTheme()->usePlatformNativeDialog(QPlatformTheme::FontDialog))
            qmlRegisterType<QQuickPlatformFontDialog>(uri, 1, 1, "FontDialog");
        else
#endif
            registerWidgetOrQmlImplementation<QQuickFontDialog>(widgetsDir, qmlDir, "FontDialog", uri, hasTopLevelWindows, 1, 1);

        // Dialog
        {
            // @uri QtQuick.Dialogs.AbstractDialog
            qmlRegisterType<QQuickDialog1>(uri, 1, 2, "AbstractDialog"); // implementation wrapper
            QUrl dialogQmlPath = m_useResources ?
                QUrl("qrc:/QtQuick/Dialogs/DefaultDialogWrapper.qml") :
#ifndef QT_STATIC
                QUrl::fromLocalFile(qmlDir.filePath("DefaultDialogWrapper.qml"));
#else
                QUrl("qrc:/qt-project.org/imports/QtQuick/Dialogs/DefaultDialogWrapper.qml");
#endif
            qCDebug(lcRegistration) << "    registering" << dialogQmlPath << "as Dialog";
            qmlRegisterType(dialogQmlPath, uri, 1, 2, "Dialog");
        }
    }

protected:
    template <class WrapperType>
    void registerWidgetOrQmlImplementation(const QDir &widgetsDir, const QDir &qmlDir,
            const char *qmlName, const char *uri, bool hasTopLevelWindows, int versionMajor, int versionMinor) {
        qCDebug(lcRegistration) << qmlName << uri << ": QML in" << qmlDir.absolutePath()
                                << "using resources?" << m_useResources << "; widgets in" << widgetsDir.absolutePath();

#ifndef PURE_QML_ONLY
        if (!registerWidgetImplementation<WrapperType>(
                    widgetsDir, qmlDir, qmlName, uri, hasTopLevelWindows, versionMajor, versionMinor))
#else
        Q_UNUSED(widgetsDir)
        Q_UNUSED(hasTopLevelWindows)
#endif
        registerQmlImplementation<WrapperType>(qmlDir, qmlName, uri, versionMajor, versionMinor);
    }

    template <class WrapperType>
    bool registerWidgetImplementation(const QDir &widgetsDir, const QDir &qmlDir,
            const char *qmlName, const char *uri, bool hasTopLevelWindows, int versionMajor, int versionMinor)
    {

        bool mobileTouchPlatform = false;
#if defined(Q_OS_IOS)
        mobileTouchPlatform = true;
#elif defined(Q_OS_ANDROID) || defined(Q_OS_BLACKBERRY) || defined(Q_OS_QNX) || defined(Q_OS_WINRT)
        const auto devices = QTouchDevice::devices();
        for (const QTouchDevice *dev : devices)
            if (dev->type() == QTouchDevice::TouchScreen)
                mobileTouchPlatform = true;
#endif

        // If there is a qmldir and we have a QApplication instance (as opposed to a
        // widget-free QGuiApplication), and this isn't a mobile touch-based platform,
        // assume that the widget-based dialog will work.  Otherwise an application developer
        // can ensure that widgets are omitted from the deployment to ensure that the widget
        // dialogs won't be used.
        if (!mobileTouchPlatform && hasTopLevelWindows && widgetsDir.exists("qmldir") &&
                QCoreApplication::instance()->inherits("QApplication")) {
            QUrl dialogQmlPath =  m_useResources ?
                QUrl(QString("qrc:/QtQuick/Dialogs/Widget%1.qml").arg(qmlName)) :
#ifndef QT_STATIC
                    QUrl::fromLocalFile(qmlDir.filePath(QString("Widget%1.qml").arg(qmlName)));
#else
                    QUrl(QString("qrc:/qt-project.org/imports/QtQuick/Dialogs/Widget%1.qml").arg(qmlName));
            Q_UNUSED(qmlDir);
#endif
            if (qmlRegisterType(dialogQmlPath, uri, versionMajor, versionMinor, qmlName) >= 0) {
                qCDebug(lcRegistration) << "    registering" << qmlName << " as " << dialogQmlPath;
                return true;
            }
        }
        return false;
    }

    template <class WrapperType>
    void registerQmlImplementation(const QDir &qmlDir, const char *qmlName, const char *uri , int versionMajor, int versionMinor)
    {
        qCDebug(lcRegistration) << "Register QML version for" << qmlName << "with uri:" << uri;

        QByteArray abstractTypeName = QByteArray("Abstract") + qmlName;
        qmlRegisterType<WrapperType>(uri, versionMajor, versionMinor, abstractTypeName);
        QUrl dialogQmlPath =  m_useResources ?
                    QUrl(QString("qrc:/QtQuick/Dialogs/Default%1.qml").arg(qmlName)) :
#ifndef QT_STATIC
                    QUrl::fromLocalFile(qmlDir.filePath(QString("Default%1.qml").arg(qmlName)));
#else
                    QUrl(QString("qrc:/qt-project.org/imports/QtQuick/Dialogs/Default%1.qml").arg(qmlName));
        Q_UNUSED(qmlDir);
#endif
        qCDebug(lcRegistration) << "    registering" << qmlName << " as " << dialogQmlPath;
        qmlRegisterType(dialogQmlPath, uri, versionMajor, versionMinor, qmlName);
    }

    QUrl m_decorationComponentUrl;
    bool m_useResources;
};

QT_END_NAMESPACE

#include "plugin.moc"
