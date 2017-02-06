/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Gamepad module
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

#include "qgamepadmanager.h"

#include "qgamepadbackend_p.h"
#include "qgamepadbackendfactory_p.h"

#include <QtCore/QLoggingCategory>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(gp, "qt.gamepad")

QGamepadManager::QGamepadManager() :
    QObject(0)
{
    loadBackend();

    qRegisterMetaType<QGamepadManager::GamepadButton>("QGamepadManager::GamepadButton");
    qRegisterMetaType<QGamepadManager::GamepadAxis>("QGamepadManager::GamepadAxis");

    connect(m_gamepadBackend, SIGNAL(gamepadAdded(int)), this, SLOT(forwardGamepadConnected(int)));
    connect(m_gamepadBackend, SIGNAL(gamepadRemoved(int)), this, SLOT(forwardGamepadDisconnected(int)));
    connect(m_gamepadBackend, SIGNAL(gamepadAxisMoved(int,QGamepadManager::GamepadAxis,double)), this, SLOT(forwardGamepadAxisEvent(int,QGamepadManager::GamepadAxis,double)));
    connect(m_gamepadBackend, SIGNAL(gamepadButtonPressed(int,QGamepadManager::GamepadButton,double)), this, SLOT(forwardGamepadButtonPressEvent(int,QGamepadManager::GamepadButton,double)));
    connect(m_gamepadBackend, SIGNAL(gamepadButtonReleased(int,QGamepadManager::GamepadButton)), this, SLOT(forwardGamepadButtonReleaseEvent(int,QGamepadManager::GamepadButton)));
    connect(m_gamepadBackend, &QGamepadBackend::buttonConfigured, this, &QGamepadManager::buttonConfigured);
    connect(m_gamepadBackend, &QGamepadBackend::axisConfigured, this, &QGamepadManager::axisConfigured);
    connect(m_gamepadBackend, &QGamepadBackend::configurationCanceled, this, &QGamepadManager::configurationCanceled);

    if (!m_gamepadBackend->start())
        qCWarning(gp) << "Failed to start gamepad backend";
}

QGamepadManager::~QGamepadManager()
{
    m_gamepadBackend->stop();
    m_gamepadBackend->deleteLater();
}

void QGamepadManager::loadBackend()
{
    QStringList keys = QGamepadBackendFactory::keys();
    qCDebug(gp) << "Available backends:" << keys;
    if (!keys.isEmpty()) {
        QString requestedKey = QString::fromUtf8(qgetenv("QT_GAMEPAD"));
        QString targetKey = keys.first();
        if (!requestedKey.isEmpty() && keys.contains(requestedKey))
            targetKey = requestedKey;
        if (!targetKey.isEmpty()) {
            qCDebug(gp) << "Loading backend" << targetKey;
            m_gamepadBackend = QGamepadBackendFactory::create(targetKey, QStringList());
        }
    }

    if (!m_gamepadBackend) {
        //Use dummy backend
        m_gamepadBackend = new QGamepadBackend();
        qCDebug(gp) << "Using dummy backend";
    }
}

QGamepadManager *QGamepadManager::instance()
{
    static QGamepadManager instance;
    return &instance;
}

bool QGamepadManager::isGamepadConnected(int deviceId)
{
    return m_connectedGamepads.contains(deviceId);
}

const QList<int> QGamepadManager::connectedGamepads() const
{
    return m_connectedGamepads.toList();
}

bool QGamepadManager::isConfigurationNeeded(int deviceId)
{
    return m_gamepadBackend->isConfigurationNeeded(deviceId);
}

bool QGamepadManager::configureButton(int deviceId, QGamepadManager::GamepadButton button)
{
    return m_gamepadBackend->configureButton(deviceId, button);
}

bool QGamepadManager::configureAxis(int deviceId, QGamepadManager::GamepadAxis axis)
{
    return m_gamepadBackend->configureAxis(deviceId, axis);
}

bool QGamepadManager::setCancelConfigureButton(int deviceId, QGamepadManager::GamepadButton button)
{
    return m_gamepadBackend->setCancelConfigureButton(deviceId, button);
}

void QGamepadManager::resetConfiguration(int deviceId)
{
    m_gamepadBackend->resetConfiguration(deviceId);
}

void QGamepadManager::setSettingsFile(const QString &file)
{
    m_gamepadBackend->setSettingsFile(file);
}

void QGamepadManager::forwardGamepadConnected(int deviceId)
{
    //qDebug() << "gamepad connected: " << index;
    m_connectedGamepads.insert(deviceId);
    emit gamepadConnected(deviceId);
    emit connectedGamepadsChanged();
}

void QGamepadManager::forwardGamepadDisconnected(int deviceId)
{
    //qDebug() << "gamepad disconnected: " << index;
    m_connectedGamepads.remove(deviceId);
    emit gamepadDisconnected(deviceId);
    emit connectedGamepadsChanged();
}

void QGamepadManager::forwardGamepadAxisEvent(int deviceId, QGamepadManager::GamepadAxis axis, double value)
{
    //qDebug() << "gamepad axis event: " << index << axis << value;
    emit gamepadAxisEvent(deviceId, axis, value);
}

void QGamepadManager::forwardGamepadButtonPressEvent(int deviceId, QGamepadManager::GamepadButton button, double value)
{
    //qDebug() << "gamepad button press event: " << index << button << value;
    emit gamepadButtonPressEvent(deviceId, button, value);
}

void QGamepadManager::forwardGamepadButtonReleaseEvent(int deviceId, QGamepadManager::GamepadButton button)
{
    //qDebug() << "gamepad button release event: " << index << button;
    emit gamepadButtonReleaseEvent(deviceId, button);
}

QT_END_NAMESPACE
