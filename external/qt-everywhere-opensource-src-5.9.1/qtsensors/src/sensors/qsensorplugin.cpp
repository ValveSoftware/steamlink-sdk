/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSensors module of the Qt Toolkit.
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

#include "qsensorplugin.h"

/*!
    \class QSensorPluginInterface
    \ingroup sensors_backend
    \inmodule QtSensors
    \since 5.1
    \brief The QSensorPluginInterface class is the pure virtual interface to sensor plugins.

    The QSensorPluginInterface class is implemented in sensor plugins to register sensor
    backends with QSensorManager.

    \sa {Creating a sensor plugin}
*/

/*!
    \internal
*/
QSensorPluginInterface::~QSensorPluginInterface()
{
}

/*!
    \fn QSensorPluginInterface::registerSensors()

    This function is called when the plugin is loaded. The plugin should register
    sensor backends by calling QSensorManager::registerBackend(). Any backends
    that utilise other sensors should be registered in the
    QSensorPluginInterface::sensorsChanged() method instead.

    \sa {Creating a sensor plugin}
*/

/*!
    \class QSensorChangesInterface
    \ingroup sensors_backend
    \inmodule QtSensors
    \since 5.1
    \brief The QSensorChangesInterface class is the pure virtual interface to sensor plugins.

    The QSensorChangesInterface class is implemented in sensor plugins to receive notification
    that registered sensor backends have changed.

    \sa {Creating a sensor plugin}
*/

/*!
    \internal
*/
QSensorChangesInterface::~QSensorChangesInterface()
{
}

/*!
    \fn QSensorChangesInterface::sensorsChanged()

    This function is called when the registered backends have changed.
    Any backends that depend on the presence of other sensors should be
    registered or unregistered in here.

    \sa {Creating a sensor plugin}
*/
