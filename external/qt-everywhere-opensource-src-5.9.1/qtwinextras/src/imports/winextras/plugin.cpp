/****************************************************************************
 **
 ** Copyright (C) 2016 Ivan Vizir <define-true-false@yandex.com>
 ** Copyright (C) 2016 The Qt Company Ltd.
 ** Contact: https://www.qt.io/licensing/
 **
 ** This file is part of the QtWinExtras module of the Qt Toolkit.
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

#include "qquickdwmfeatures_p.h"
#include "qquicktaskbarbutton_p.h"
#include "qquickjumplist_p.h"
#include "qquickjumplistitem_p.h"
#include "qquickjumplistcategory_p.h"
#include "qquickthumbnailtoolbar_p.h"
#include "qquickthumbnailtoolbutton_p.h"
#include "qquickwin_p.h"

#include <QtQml/QtQml>

static void initResources()
{
#ifdef QT_STATIC
    Q_INIT_RESOURCE(qmake_QtWinExtras);
#endif
}

QT_BEGIN_NAMESPACE

class QWinExtrasQmlPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    QWinExtrasQmlPlugin(QObject *parent = 0) : QQmlExtensionPlugin(parent) { initResources(); }
    void registerTypes(const char *uri) Q_DECL_OVERRIDE
    {
        Q_ASSERT(uri == QLatin1String("QtWinExtras"));
        qmlRegisterUncreatableType<QQuickWin>(uri, 1, 0, "QtWin", "Cannot create an instance of the QtWin namespace.");
        qmlRegisterType<QQuickDwmFeatures>(uri, 1, 0, "DwmFeatures");
        qmlRegisterType<QQuickTaskbarButton>(uri, 1, 0, "TaskbarButton");
        qmlRegisterUncreatableType<QWinTaskbarProgress>(uri, 1, 0, "TaskbarProgress", "Cannot create TaskbarProgress - use TaskbarButton.progress instead.");
        qmlRegisterUncreatableType<QQuickTaskbarOverlay>(uri, 1, 0, "TaskbarOverlay", "Cannot create TaskbarOverlay - use TaskbarButton.overlay instead.");
        qmlRegisterType<QQuickJumpList>(uri, 1, 0, "JumpList");
        qmlRegisterType<QQuickJumpListItem>(uri, 1, 0, "JumpListItem");
        qmlRegisterType<QQuickJumpListCategory>(uri, 1, 0, "JumpListCategory");
        qmlRegisterType<QQuickThumbnailToolBar>(uri, 1, 0, "ThumbnailToolBar");
        qmlRegisterType<QQuickThumbnailToolButton>(uri, 1, 0, "ThumbnailToolButton");
    }
};

QT_END_NAMESPACE

#include "plugin.moc"
