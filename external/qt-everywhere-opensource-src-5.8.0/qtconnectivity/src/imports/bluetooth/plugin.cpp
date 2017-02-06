/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
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

#include <QtCore/QLoggingCategory>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlExtensionPlugin>

#include "qdeclarativebluetoothdiscoverymodel_p.h"
#include "qdeclarativebluetoothservice_p.h"
#include "qdeclarativebluetoothsocket_p.h"

static void initResources()
{
#ifdef QT_STATIC
    Q_INIT_RESOURCE(qmake_QtBluetooth);
#endif
}

QT_USE_NAMESPACE

class QBluetoothQmlPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)
public:
    QBluetoothQmlPlugin(QObject *parent = 0) : QQmlExtensionPlugin(parent) { initResources(); }
    void registerTypes(const char *uri)
    {
        // @uri QtBluetooth

        Q_ASSERT(uri == QStringLiteral("QtBluetooth"));

        int major = 5;
        int minor = 0;

        // Register the 5.0 types
        //5.0 is silent and not advertised
        qmlRegisterType<QDeclarativeBluetoothDiscoveryModel >(uri, major, minor, "BluetoothDiscoveryModel");
        qmlRegisterType<QDeclarativeBluetoothService        >(uri, major, minor, "BluetoothService");
        qmlRegisterType<QDeclarativeBluetoothSocket         >(uri, major, minor, "BluetoothSocket");

        // Register the 5.2 types
        minor = 2;
        qmlRegisterType<QDeclarativeBluetoothDiscoveryModel >(uri, major, minor, "BluetoothDiscoveryModel");
        qmlRegisterType<QDeclarativeBluetoothService        >(uri, major, minor, "BluetoothService");
        qmlRegisterType<QDeclarativeBluetoothSocket         >(uri, major, minor, "BluetoothSocket");

        // Register the 5.8 types
        // introduces 5.8 version, other existing 5.2 exports become automatically available under 5.2-5.7
        minor = 8;
        qmlRegisterType<QDeclarativeBluetoothDiscoveryModel >(uri, major, minor, "BluetoothDiscoveryModel");
    }
};

Q_LOGGING_CATEGORY(QT_BT_QML, "qt.bluetooth.qml")

#include "plugin.moc"
