/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWaylandCompositor module of the Qt Toolkit.
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
#include <QtCore/QDir>

#include <QtQml/qqmlextensionplugin.h>

#include <QtQuick/QQuickItem>

#include <QtWaylandCompositor/QWaylandQuickCompositor>
#include <QtWaylandCompositor/QWaylandQuickItem>
#include <QtWaylandCompositor/QWaylandQuickSurface>
#include <QtWaylandCompositor/QWaylandClient>
#include <QtWaylandCompositor/QWaylandQuickOutput>
#include <QtWaylandCompositor/QWaylandCompositorExtension>
#include <QtWaylandCompositor/QWaylandQuickExtension>
#include <QtWaylandCompositor/QWaylandSeat>
#if QT_CONFIG(draganddrop)
#include <QtWaylandCompositor/QWaylandDrag>
#endif
#include <QtWaylandCompositor/QWaylandKeymap>
#include <QtWaylandCompositor/QWaylandQuickShellSurfaceItem>
#include <QtWaylandCompositor/QWaylandResource>

#include <QtWaylandCompositor/QWaylandQtWindowManager>
#include <QtWaylandCompositor/QWaylandWlShell>
#include <QtWaylandCompositor/QWaylandTextInputManager>
#include <QtWaylandCompositor/QWaylandXdgShellV5>
#include <QtWaylandCompositor/QWaylandIviApplication>
#include <QtWaylandCompositor/QWaylandIviSurface>

#include <QtWaylandCompositor/qtwaylandcompositorglobal.h>
#include "qwaylandmousetracker_p.h"

QT_BEGIN_NAMESPACE

Q_COMPOSITOR_DECLARE_QUICK_EXTENSION_CONTAINER_CLASS(QWaylandQuickCompositor)
Q_COMPOSITOR_DECLARE_QUICK_EXTENSION_CLASS(QWaylandQtWindowManager)
Q_COMPOSITOR_DECLARE_QUICK_EXTENSION_CLASS(QWaylandIviApplication)
Q_COMPOSITOR_DECLARE_QUICK_EXTENSION_CLASS(QWaylandWlShell)
Q_COMPOSITOR_DECLARE_QUICK_EXTENSION_CLASS(QWaylandXdgShellV5)
Q_COMPOSITOR_DECLARE_QUICK_EXTENSION_CLASS(QWaylandTextInputManager)

class QmlUrlResolver
{
public:
    QmlUrlResolver(bool useResource, const QDir &qmlDir, const QString &qrcPath)
        : m_useResource(useResource)
        , m_qmlDir(qmlDir)
        , m_qrcPath(qrcPath)
    { }

    QUrl get(const QString &fileName)
    {
        return m_useResource ? QUrl(m_qrcPath + fileName) :
            QUrl::fromLocalFile(m_qmlDir.filePath(fileName));
    }
private:
    bool m_useResource;
    const QDir m_qmlDir;
    const QString m_qrcPath;
};


//![class decl]
class QWaylandCompositorPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)
public:
    virtual void registerTypes(const char *uri)
    {
        Q_ASSERT(QLatin1String(uri) == QLatin1String("QtWayland.Compositor"));
        defineModule(uri);

        bool useResource = true;
        QDir qmlDir(baseUrl().toLocalFile());
        if (qmlDir.exists(QStringLiteral("WaylandCursorItem.qml")))
            useResource = false;

        QmlUrlResolver resolver(useResource, qmlDir, QStringLiteral("qrc:/QtWayland/Compositor/"));

        qmlRegisterType(resolver.get(QStringLiteral("WaylandOutputWindow.qml")), uri, 1, 0, "WaylandOutputWindow");
        qmlRegisterType(resolver.get(QStringLiteral("WaylandCursorItem.qml")), uri, 1, 0, "WaylandCursorItem");
    }

    static void defineModule(const char *uri)
    {
        qmlRegisterType<QWaylandQuickCompositorQuickExtensionContainer>(uri, 1, 0, "WaylandCompositor");
        qmlRegisterType<QWaylandQuickItem>(uri, 1, 0, "WaylandQuickItem");
        qmlRegisterType<QWaylandMouseTracker>(uri, 1, 0, "WaylandMouseTracker");
        qmlRegisterType<QWaylandQuickOutput>(uri, 1, 0, "WaylandOutput");
        qmlRegisterType<QWaylandQuickSurface>(uri, 1, 0, "WaylandSurface");
        qmlRegisterType<QWaylandKeymap>(uri, 1, 0, "WaylandKeymap");

        qmlRegisterUncreatableType<QWaylandCompositorExtension>(uri, 1, 0, "WaylandExtension", QObject::tr("Cannot create instance of WaylandExtension"));
        qmlRegisterUncreatableType<QWaylandClient>(uri, 1, 0, "WaylandClient", QObject::tr("Cannot create instance of WaylandClient"));
        qmlRegisterUncreatableType<QWaylandOutput>(uri, 1, 0, "WaylandOutputBase", QObject::tr("Cannot create instance of WaylandOutputBase, use WaylandOutput instead"));
        qmlRegisterUncreatableType<QWaylandSeat>(uri, 1, 0, "WaylandSeat", QObject::tr("Cannot create instance of WaylandSeat"));
#if QT_CONFIG(draganddrop)
        qmlRegisterUncreatableType<QWaylandDrag>(uri, 1, 0, "WaylandDrag", QObject::tr("Cannot create instance of WaylandDrag"));
#endif
        qmlRegisterUncreatableType<QWaylandCompositor>(uri, 1, 0, "WaylandCompositorBase", QObject::tr("Cannot create instance of WaylandCompositorBase, use WaylandCompositor instead"));
        qmlRegisterUncreatableType<QWaylandSurface>(uri, 1, 0, "WaylandSurfaceBase", QObject::tr("Cannot create instance of WaylandSurfaceBase, use WaylandSurface instead"));
        qmlRegisterUncreatableType<QWaylandShell>(uri, 1, 0, "Shell", QObject::tr("Cannot create instance of Shell"));
        qmlRegisterUncreatableType<QWaylandShellSurface>(uri, 1, 0, "ShellSurface", QObject::tr("Cannot create instance of ShellSurface"));
        qmlRegisterUncreatableType<QWaylandResource>(uri, 1, 0, "WaylandResource", QObject::tr("Cannot create instance of WaylandResource"));

        //This should probably be somewhere else
        qmlRegisterType<QWaylandQtWindowManagerQuickExtension>(uri, 1, 0, "QtWindowManager");
        qmlRegisterType<QWaylandIviApplicationQuickExtension>(uri, 1, 0, "IviApplication");
        qmlRegisterType<QWaylandIviSurface>(uri, 1, 0, "IviSurface");
        qmlRegisterType<QWaylandWlShellQuickExtension>(uri, 1, 0, "WlShell");
        qmlRegisterType<QWaylandWlShellSurface>(uri, 1, 0, "WlShellSurface");
        qmlRegisterType<QWaylandQuickShellSurfaceItem>(uri, 1, 0, "ShellSurfaceItem");
        qmlRegisterUncreatableType<QWaylandXdgShellV5>(uri, 1, 0, "XdgShellV5Base", QObject::tr("Cannot create instance of XdgShellV5Base"));
        qmlRegisterType<QWaylandXdgShellV5QuickExtension>(uri, 1, 0, "XdgShellV5");
        qmlRegisterType<QWaylandXdgSurfaceV5>(uri, 1, 0, "XdgSurfaceV5");
        qmlRegisterType<QWaylandXdgPopupV5>(uri, 1, 0, "XdgPopupV5");
        qmlRegisterType<QWaylandTextInputManagerQuickExtension>(uri, 1, 0, "TextInputManager");
    }
};
//![class decl]

QT_END_NAMESPACE

#include "qwaylandquickcompositorplugin.moc"
