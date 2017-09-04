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

#include "qgamepadbackend_p.h"
#include <QSettings>

QT_BEGIN_NAMESPACE

static const QLatin1String GAMEPAD_GROUP("___gamepad_saved_states_v2");

QGamepadBackend::QGamepadBackend(QObject *parent) :
    QObject(parent)
{
}

bool QGamepadBackend::isConfigurationNeeded(int /*deviceId*/)
{
    return false;
}

void QGamepadBackend::resetConfiguration(int /*deviceId*/)
{
}

bool QGamepadBackend::configureButton(int /*deviceId*/, QGamepadManager::GamepadButton /*button*/)
{
    return false;
}

bool QGamepadBackend::configureAxis(int /*deviceId*/, QGamepadManager::GamepadAxis /*axis*/)
{
    return false;
}

bool QGamepadBackend::setCancelConfigureButton(int /*deviceId*/, QGamepadManager::GamepadButton /*button*/)
{
    return false;
}

void QGamepadBackend::setSettingsFile(const QString &file)
{
    m_settingsFilePath = file;
}

void QGamepadBackend::saveSettings(int productId, const QVariant &value)
{
    QScopedPointer<QSettings> s;
    if (m_settingsFilePath.isNull())
        s.reset(new QSettings());
    else
        s.reset(new QSettings(m_settingsFilePath));
    s->beginGroup(GAMEPAD_GROUP);
    QString key = QString::fromLatin1("id_%1").arg(productId);
    if (value.isNull())
        s->remove(key);
    else
        s->setValue(key, value);
}

QVariant QGamepadBackend::readSettings(int productId)
{
    QScopedPointer<QSettings> s;
    if (m_settingsFilePath.isNull())
        s.reset(new QSettings());
    else
        s.reset(new QSettings(m_settingsFilePath));
    s->beginGroup(GAMEPAD_GROUP);
    return s->value(QString::fromLatin1("id_%1").arg(productId));
}

bool QGamepadBackend::start()
{
    return true;
}

void QGamepadBackend::stop()
{

}

QT_END_NAMESPACE
