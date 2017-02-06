/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Layouts module of the Qt Toolkit.
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

#include "qquicklinearlayout_p.h"
#include "qquickstacklayout_p.h"

static void initResources()
{
#ifdef QT_STATIC
    Q_INIT_RESOURCE(qmake_QtQuick_Layouts);
#endif
}

QT_BEGIN_NAMESPACE

//![class decl]
class QtQuickLayoutsPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)
public:
    QtQuickLayoutsPlugin(QObject *parent = 0) : QQmlExtensionPlugin(parent)
    {
        initResources();
    }
    void registerTypes(const char *uri) Q_DECL_OVERRIDE
    {
        Q_ASSERT(QLatin1String(uri) == QLatin1String("QtQuick.Layouts"));
        Q_UNUSED(uri);

        qmlRegisterType<QQuickRowLayout>(uri, 1, 0, "RowLayout");
        qmlRegisterType<QQuickColumnLayout>(uri, 1, 0, "ColumnLayout");
        qmlRegisterType<QQuickGridLayout>(uri, 1, 0, "GridLayout");
        qmlRegisterType<QQuickStackLayout>(uri, 1, 3, "StackLayout");
        qmlRegisterUncreatableType<QQuickLayout>(uri, 1, 0, "Layout",
                                                           QStringLiteral("Do not create objects of type Layout"));
        qmlRegisterUncreatableType<QQuickLayout>(uri, 1, 2, "Layout",
                                                           QStringLiteral("Do not create objects of type Layout"));
        qmlRegisterRevision<QQuickGridLayoutBase, 1>(uri, 1, 1);
    }
};
//![class decl]

QT_END_NAMESPACE

#include "plugin.moc"
