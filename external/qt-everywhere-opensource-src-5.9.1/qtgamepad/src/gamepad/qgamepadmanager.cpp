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

#include <private/qobject_p.h>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(gp, "qt.gamepad")

class QGamepadManagerPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QGamepadManager)
public:
    QGamepadManagerPrivate()
        : gamepadBackend(nullptr)
    {
        loadBackend();
    }

    void loadBackend();

    QGamepadBackend *gamepadBackend;
    QSet<int> connectedGamepads;

    //private slots
    void _q_forwardGamepadConnected(int deviceId);
    void _q_forwardGamepadDisconnected(int deviceId);
    void _q_forwardGamepadAxisEvent(int deviceId, QGamepadManager::GamepadAxis axis, double value);
    void _q_forwardGamepadButtonPressEvent(int deviceId, QGamepadManager::GamepadButton button, double value);
    void _q_forwardGamepadButtonReleaseEvent(int deviceId, QGamepadManager::GamepadButton button);
};

void QGamepadManagerPrivate::_q_forwardGamepadConnected(int deviceId)
{
    Q_Q(QGamepadManager);
    connectedGamepads.insert(deviceId);
    emit q->gamepadConnected(deviceId);
    emit q->connectedGamepadsChanged();
}

void QGamepadManagerPrivate::_q_forwardGamepadDisconnected(int deviceId)
{
    Q_Q(QGamepadManager);
    connectedGamepads.remove(deviceId);
    emit q->gamepadDisconnected(deviceId);
    emit q->connectedGamepadsChanged();
}

void QGamepadManagerPrivate::_q_forwardGamepadAxisEvent(int deviceId, QGamepadManager::GamepadAxis axis, double value)
{
    Q_Q(QGamepadManager);
    emit q->gamepadAxisEvent(deviceId, axis, value);
}

void QGamepadManagerPrivate::_q_forwardGamepadButtonPressEvent(int deviceId, QGamepadManager::GamepadButton button, double value)
{
    Q_Q(QGamepadManager);
    emit q->gamepadButtonPressEvent(deviceId, button, value);
}

void QGamepadManagerPrivate::_q_forwardGamepadButtonReleaseEvent(int deviceId, QGamepadManager::GamepadButton button)
{
    Q_Q(QGamepadManager);
    emit q->gamepadButtonReleaseEvent(deviceId, button);
}

void QGamepadManagerPrivate::loadBackend()
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
            gamepadBackend = QGamepadBackendFactory::create(targetKey, QStringList());
        }
    }

    if (!gamepadBackend) {
        //Use dummy backend
        gamepadBackend = new QGamepadBackend();
        qCDebug(gp) << "Using dummy backend";
    }
}

/*!
   \class QGamepadManager
   \inmodule QtGamepad
   \brief Queries attached gamepads and related events.

   QGamepadManager provides a high-level interface for querying the attached
   gamepads and events related to all of the connected devices.
 */

/*!
 * Constructor for QGamepadManager.
 */

QGamepadManager::QGamepadManager() :
    QObject(*new QGamepadManagerPrivate(), nullptr)
{
    Q_D(QGamepadManager);

    qRegisterMetaType<QGamepadManager::GamepadButton>("QGamepadManager::GamepadButton");
    qRegisterMetaType<QGamepadManager::GamepadAxis>("QGamepadManager::GamepadAxis");

    connect(d->gamepadBackend, SIGNAL(gamepadAdded(int)), this, SLOT(_q_forwardGamepadConnected(int)));
    connect(d->gamepadBackend, SIGNAL(gamepadRemoved(int)), this, SLOT(_q_forwardGamepadDisconnected(int)));
    connect(d->gamepadBackend, SIGNAL(gamepadAxisMoved(int,QGamepadManager::GamepadAxis,double)), this, SLOT(_q_forwardGamepadAxisEvent(int,QGamepadManager::GamepadAxis,double)));
    connect(d->gamepadBackend, SIGNAL(gamepadButtonPressed(int,QGamepadManager::GamepadButton,double)), this, SLOT(_q_forwardGamepadButtonPressEvent(int,QGamepadManager::GamepadButton,double)));
    connect(d->gamepadBackend, SIGNAL(gamepadButtonReleased(int,QGamepadManager::GamepadButton)), this, SLOT(_q_forwardGamepadButtonReleaseEvent(int,QGamepadManager::GamepadButton)));

    connect(d->gamepadBackend, &QGamepadBackend::buttonConfigured, this, &QGamepadManager::buttonConfigured);
    connect(d->gamepadBackend, &QGamepadBackend::axisConfigured, this, &QGamepadManager::axisConfigured);
    connect(d->gamepadBackend, &QGamepadBackend::configurationCanceled, this, &QGamepadManager::configurationCanceled);

    if (!d->gamepadBackend->start())
        qCWarning(gp) << "Failed to start gamepad backend";
}

/*!
 * Destructor for QGamepadManager.
 */

QGamepadManager::~QGamepadManager()
{
    Q_D(QGamepadManager);
    d->gamepadBackend->stop();
    d->gamepadBackend->deleteLater();
}

/*!
    Returns the instance of the QGamepadManager.
*/
QGamepadManager *QGamepadManager::instance()
{
    static QGamepadManager instance;
    return &instance;
}

/*!
    Returns a boolean indicating whether the gamepad with
    the specified \a deviceId is connected or not.
*/
bool QGamepadManager::isGamepadConnected(int deviceId) const
{
    Q_D(const QGamepadManager);
    return d->connectedGamepads.contains(deviceId);
}

/*!
    Returns a QList containing the \l {QGamepad::}{deviceId}
    values of the connected gamepads.
*/
const QList<int> QGamepadManager::connectedGamepads() const
{
    Q_D(const QGamepadManager);
    return d->connectedGamepads.toList();
}

/*!
    Returns a boolean indicating whether configuration
    is needed for the specified \a deviceId.
*/
bool QGamepadManager::isConfigurationNeeded(int deviceId) const
{
    Q_D(const QGamepadManager);
    return d->gamepadBackend->isConfigurationNeeded(deviceId);
}

/*!
    Configures the specified \a button on the gamepad with
    this \a deviceId.
    Returns \c true in case of success.
*/
bool QGamepadManager::configureButton(int deviceId, QGamepadManager::GamepadButton button)
{
    Q_D(QGamepadManager);
    return d->gamepadBackend->configureButton(deviceId, button);
}

/*!
    Configures \a axis on the gamepad with the specified \a deviceId.
    Returns \c true in case of success.
*/
bool QGamepadManager::configureAxis(int deviceId, QGamepadManager::GamepadAxis axis)
{
    Q_D(QGamepadManager);
    return d->gamepadBackend->configureAxis(deviceId, axis);
}

/*!
    Configures \a button as the cancel button on the gamepad with
    id \a deviceId.
    Returns \c true in case of success.
*/
bool QGamepadManager::setCancelConfigureButton(int deviceId, QGamepadManager::GamepadButton button)
{
    Q_D(QGamepadManager);
    return d->gamepadBackend->setCancelConfigureButton(deviceId, button);
}

/*!
    Resets the configuration on the gamepad with the
    specified \a deviceId.
*/
void QGamepadManager::resetConfiguration(int deviceId)
{
    Q_D(QGamepadManager);
    d->gamepadBackend->resetConfiguration(deviceId);
}

/*!
    Sets the name of the \a file that stores the button and axis
    configuration data.
*/
void QGamepadManager::setSettingsFile(const QString &file)
{
    Q_D(QGamepadManager);
    d->gamepadBackend->setSettingsFile(file);
}

QT_END_NAMESPACE

#include "moc_qgamepadmanager.cpp"
