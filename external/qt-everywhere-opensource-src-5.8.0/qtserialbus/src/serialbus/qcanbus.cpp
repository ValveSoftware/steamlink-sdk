/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSerialBus module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qcanbus.h"
#include "qcanbusfactory.h"

#include <QtCore/qobject.h>
#include <QtCore/qpluginloader.h>
#include <QtCore/qdebug.h>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qglobalstatic.h>
#include <QtCore/qlist.h>

#include <private/qfactoryloader_p.h>
#include <private/qlibrary_p.h>

#define QCanBusFactory_iid "org.qt-project.Qt.QCanBusFactory"

QT_BEGIN_NAMESPACE

struct QCanBusPrivate
{
public:
    QCanBusPrivate() : factory(nullptr), index(-1) { }

    QJsonObject meta;
    QCanBusFactory *factory;
    int index;
};

Q_GLOBAL_STATIC_WITH_ARGS(QFactoryLoader, qFactoryLoader,
    (QCanBusFactory_iid, QLatin1String("/canbus")))

typedef QMap<QString, QCanBusPrivate> QCanBusPluginStore;
Q_GLOBAL_STATIC(QCanBusPluginStore, qCanBusPlugins)

static QCanBus *globalInstance = nullptr;

static void loadPlugins()
{
    const QList<QJsonObject> meta = qFactoryLoader()->metaData();
    for (int i = 0; i < meta.count(); i++) {
        const QJsonObject obj = meta.at(i).value(QStringLiteral("MetaData")).toObject();
        if (obj.isEmpty())
            continue;

        QCanBusPrivate d;
        d.index = i;
        d.meta = obj;
        qCanBusPlugins()->insert(obj.value(QStringLiteral("Key")).toString(), d);
    }
}

/*!
    \class QCanBus
    \inmodule QtSerialBus
    \since 5.8

    \brief The QCanBus class handles registration and creation of bus plugins.

    QCanBus loads Qt CAN Bus plugins at runtime. The ownership of serial bus plugins is
    transferred to the loader.
*/

/*!
    Returns a pointer to the QCanBus class. The object is loaded if necessary. QCanBus
    uses the singleton design pattern.
*/
QCanBus *QCanBus::instance()
{
    if (!globalInstance)
        globalInstance = new QCanBus();
    return globalInstance;
}

/*!
    Returns a list of identifiers for all loaded plugins.
*/
QStringList QCanBus::plugins() const
{
    return qCanBusPlugins()->keys();
}

static void setErrorMessage(QString *result, const QString &message)
{
    if (!result)
        return;

    *result = message;
}

/*!
    Creates a CAN bus device. \a plugin is the name of the plugin as returned by the \l plugins()
    method. \a interfaceName is the CAN bus interface name. In case of failure, the optional
    parameter \a errorMessage returns a textual error description.

    Ownership of the returned plugin is transferred to the caller.
    Returns \c nullptr if no suitable device can be found.

    For example, the following call would connect to the SocketCAN interface vcan0:

    \code
        QCanBusDevice *device = QCanBus::instance()->createDevice(
            QStringLiteral("socketcan"), QStringLiteral("vcan0"));
        device->connectDevice();
    \endcode

    \note The \a interfaceName is plugin-dependent. See the corresponding plugin documentation
    for more information: \l {CAN Bus Plugins}.
*/
QCanBusDevice *QCanBus::createDevice(const QString &plugin, const QString &interfaceName,
                                     QString *errorMessage) const
{
    if (!qCanBusPlugins()->contains(plugin)) {
        setErrorMessage(errorMessage, tr("No such plugin: '%1'").arg(plugin));
        return nullptr;
    }

    QCanBusPrivate d = qCanBusPlugins()->value(plugin);
    if (!d.factory) {
        d.factory
            = qobject_cast<QCanBusFactory *>(qFactoryLoader->instance(d.index));

        if (d.factory)
            qCanBusPlugins()->insert(plugin, d);
    }
    if (!d.factory) {
        setErrorMessage(errorMessage, tr("No factory for plugin: '%1'").arg(plugin));
        return nullptr;
    }

    return d.factory->createDevice(interfaceName, errorMessage);
}

QCanBus::QCanBus(QObject *parent) :
    QObject(parent)
{
    loadPlugins();
}

QT_END_NAMESPACE
