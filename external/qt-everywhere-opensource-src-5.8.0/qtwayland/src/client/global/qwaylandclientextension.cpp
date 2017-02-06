/****************************************************************************
**
** Copyright (C) 2016 Erik Larsson.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWaylandCompositor module of the Qt Toolkit.
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

#include "qwaylandclientextension.h"
#include "qwaylandclientextension_p.h"
#include <QtWaylandClient/private/qwaylanddisplay_p.h>
#include <QtWaylandClient/private/qwaylandintegration_p.h>
#include <QtGui/QGuiApplication>
#include <QtGui/qpa/qplatformnativeinterface.h>
#include <QtGui/private/qguiapplication_p.h>
#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

QWaylandClientExtensionPrivate::QWaylandClientExtensionPrivate()
    : QObjectPrivate()
    , waylandIntegration(NULL)
    , version(-1)
    , active(false)
{
    // Keep the possibility to use a custom waylandIntegration as a plugin,
    // but also add the possibility to run it as a QML component.
    waylandIntegration = static_cast<QtWaylandClient::QWaylandIntegration *>(QGuiApplicationPrivate::platformIntegration());
    if (!waylandIntegration)
        waylandIntegration = new QtWaylandClient::QWaylandIntegration();

    if (!waylandIntegration->nativeInterface()->nativeResourceForIntegration("wl_display"))
        qWarning() << "This application requires a Wayland platform plugin";
}

void QWaylandClientExtensionPrivate::handleRegistryGlobal(void *data, ::wl_registry *registry, uint32_t id,
                                                          const QString &interface, uint32_t version)
{
    QWaylandClientExtension *extension = static_cast<QWaylandClientExtension *>(data);
    if (interface == QLatin1String(extension->extensionInterface()->name) && !extension->d_func()->active) {
        extension->bind(registry, id, version);
        extension->d_func()->active = true;
        emit extension->activeChanged();
    }
}

void QWaylandClientExtension::addRegistryListener()
{
    Q_D(QWaylandClientExtension);
    d->waylandIntegration->display()->addRegistryListener(&QWaylandClientExtensionPrivate::handleRegistryGlobal, this);
}

QWaylandClientExtension::QWaylandClientExtension(const int ver)
    : QObject(*new QWaylandClientExtensionPrivate())
{
    Q_D(QWaylandClientExtension);
    d->version = ver;

    // The registry listener uses virtual functions and we don't want it to be called from
    // the constructor.
    QMetaObject::invokeMethod(this, "addRegistryListener", Qt::QueuedConnection);
}

QtWaylandClient::QWaylandIntegration *QWaylandClientExtension::integration() const
{
    Q_D(const QWaylandClientExtension);
    return d->waylandIntegration;
}

int QWaylandClientExtension::version() const
{
    Q_D(const QWaylandClientExtension);
    return d->version;
}

void QWaylandClientExtension::setVersion(const int ver)
{
    Q_D(QWaylandClientExtension);
    if (d->version != ver) {
        d->version = ver;
        emit versionChanged();
    }
}

bool QWaylandClientExtension::isActive() const
{
    Q_D(const QWaylandClientExtension);
    return d->active;
}

QT_END_NAMESPACE
