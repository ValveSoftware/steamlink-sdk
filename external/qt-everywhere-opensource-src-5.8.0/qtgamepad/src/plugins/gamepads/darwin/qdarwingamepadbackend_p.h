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

#ifndef QDARWINGAMEPADCONTROLLER_H
#define QDARWINGAMEPADCONTROLLER_H

#include <QtCore/QTimer>
#include <QtCore/QMap>

#include <QtGamepad/QGamepadManager>
#include <QtGamepad/private/qgamepadbackend_p.h>

Q_FORWARD_DECLARE_OBJC_CLASS(QT_MANGLE_NAMESPACE(DarwinGamepadManager));

QT_BEGIN_NAMESPACE

class QDarwinGamepadBackend : public QGamepadBackend
{
    Q_OBJECT
public:
    explicit QDarwinGamepadBackend(QObject *parent = 0);
    ~QDarwinGamepadBackend();

protected:
    bool start();
    void stop();

public slots:
    void darwinGamepadAdded(int index);
    void darwinGamepadRemoved(int index);
    void darwinGamepadAxisMoved(int index, QGamepadManager::GamepadAxis axis, double value);
    void darwinGamepadButtonPressed(int index, QGamepadManager::GamepadButton button, double value);
    void darwinGamepadButtonReleased(int index, QGamepadManager::GamepadButton button);
    void handlePauseButton(int index);

private:
    QT_MANGLE_NAMESPACE(DarwinGamepadManager) *m_darwinGamepadManager;
    bool m_isMonitoringActive;
    QMap<int, bool> m_pauseButtonMap;
};

QT_END_NAMESPACE

#endif // QDARWINGAMEPADCONTROLLER_H
