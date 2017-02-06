/****************************************************************************
**
** Copyright (C) 2015 BogDan Vatra <bogdan@kde.org>
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

#ifndef QSDLGAMEPADCONTROLLER_H
#define QSDLGAMEPADCONTROLLER_H

#include <QtCore/QHash>
#include <QtCore/QMutex>
#include <QtCore/QSet>

#include <QtCore/private/qjni_p.h>
#include <QtCore/private/qjnihelpers_p.h>

#include <QtGamepad/QGamepadManager>
#include <QtGamepad/private/qgamepadbackend_p.h>

QT_BEGIN_NAMESPACE

class QAndroidGamepadBackend : public QGamepadBackend, public QtAndroidPrivate::GenericMotionEventListener, public QtAndroidPrivate::KeyEventListener
{
    Q_OBJECT
public:
    explicit QAndroidGamepadBackend(QObject *parent = 0);
    ~QAndroidGamepadBackend();

    void addDevice(int deviceId);
    void updateDevice(int deviceId);
    void removeDevice(int deviceId);

    // QObject interface
    bool event(QEvent *) override;

    // QGamepadBackend interface
    bool isConfigurationNeeded(int deviceId) override;
    bool configureButton(int deviceId, QGamepadManager::GamepadButton button) override;
    bool configureAxis(int deviceId, QGamepadManager::GamepadAxis axis) override;
    bool setCancelConfigureButton(int deviceId, QGamepadManager::GamepadButton button) override;
    void resetConfiguration(int deviceId) override;


    // KeyEventListener interface
    bool handleKeyEvent(jobject event) override;

    // GenericMotionEventListener interface
    bool handleGenericMotionEvent(jobject event) override;

protected:
    bool start() override;
    void stop() override;

public:
    struct Mapping {
        struct AndroidAxisInfo : public AxisInfo<double> {
            AndroidAxisInfo() : AxisInfo(-1.0, 1.0) { }
            AndroidAxisInfo(double minValue, double maxValue) : AxisInfo(minValue, maxValue) { }

            inline bool setValue(double value)
            {
                if (minValue != -1.0 && maxValue != 1.0)
                    value = AxisInfo::normalized(value);

                if (qAbs(value) <= flatArea)
                    value = 0;

                if (qAbs(qAbs(value) - qAbs(lastValue)) <= fuzz)
                    return false;

                lastValue = value;
                return true;
            }
            void restoreSavedData(const QVariantMap &value);
            QVariantMap dataToSave() const;

            double flatArea = -1;
            double fuzz = 0;
            double lastValue = 0;
            QGamepadManager::GamepadButton gamepadMinButton = QGamepadManager::ButtonInvalid;
            QGamepadManager::GamepadButton gamepadMaxButton = QGamepadManager::ButtonInvalid;
            QGamepadManager::GamepadButton gamepadLastButton = QGamepadManager::ButtonInvalid;
        };
        QHash<int, AndroidAxisInfo> axisMap;
        QHash<int, QGamepadManager::GamepadButton> buttonsMap;

        QGamepadManager::GamepadButton calibrateButton = QGamepadManager::ButtonInvalid;
        QGamepadManager::GamepadAxis calibrateAxis = QGamepadManager::AxisInvalid;
        QGamepadManager::GamepadButton cancelConfigurationButton = QGamepadManager::ButtonInvalid;
        int productId = 0;
        bool needsConfigure = false;
    };

private:
    void saveData(const Mapping &deviceInfo);

private:
    QMutex m_mutex;
    QJNIObjectPrivate m_qtGamepad;
    QHash<int, Mapping> m_devices;
};

QT_END_NAMESPACE

#endif // QSDLGAMEPADCONTROLLER_H
