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

#ifndef QEVDEVGAMEPADCONTROLLER_H
#define QEVDEVGAMEPADCONTROLLER_H

#include <QtGamepad/QGamepadManager>
#include <QtGamepad/private/qgamepadbackend_p.h>
#include <QtCore/QHash>
#include <QtCore/QVector>

struct input_event;

QT_BEGIN_NAMESPACE

class QSocketNotifier;
class QDeviceDiscovery;
class QEvdevGamepadBackend;

class QEvdevGamepadDevice : public QObject
{
    Q_OBJECT

public:
    QEvdevGamepadDevice(const QByteArray &dev, QEvdevGamepadBackend *backend);
    ~QEvdevGamepadDevice();
    QByteArray deviceName() const { return m_dev; }
    int deviceId() const { return m_productId; }
    void resetConfiguration();
    bool isConfigurationNeeded();
    bool configureButton(QGamepadManager::GamepadButton button);
    bool configureAxis(QGamepadManager::GamepadAxis axis);
    bool setCancelConfigureButton(QGamepadManager::GamepadButton button);

private slots:
    void readData();

private:
    void saveData();
    void processInputEvent(input_event *e);
    bool openDevice(const QByteArray &dev);

    QByteArray m_dev;
    QEvdevGamepadBackend *m_backend;
    int m_fd;
    int m_productId;
    bool m_needsConfigure;
    QSocketNotifier *m_notifier;
    struct EvdevAxisInfo : public QGamepadBackend::AxisInfo<int>
    {
        EvdevAxisInfo();
        EvdevAxisInfo(int fd, quint16 abs, int minValue = 0, int maxValue = 1, QGamepadManager::GamepadAxis gamepadAxis = QGamepadManager::AxisInvalid);
        double normalized(int value) const;
        void setAbsInfo(int fd, int abs);
        void restoreSavedData(int fd, int abs, const QVariantMap &value);
        QVariantMap dataToSave() const;
        double flat;
        QGamepadManager::GamepadButton gamepadMinButton;
        QGamepadManager::GamepadButton gamepadMaxButton;
        QGamepadManager::GamepadButton gamepadLastButton;
    };
    typedef QHash<int, EvdevAxisInfo> AxisMap;
    AxisMap m_axisMap;

    friend QDebug operator<<(QDebug dbg, const QEvdevGamepadDevice::EvdevAxisInfo &axisInfo);

    typedef QHash<int, QGamepadManager::GamepadButton> ButtonsMap;
    ButtonsMap m_buttonsMap;

    QGamepadManager::GamepadButton m_configureButton;
    QGamepadManager::GamepadAxis m_configureAxis;
    QGamepadManager::GamepadButton m_configureCancelButton;
};

QDebug operator<<(QDebug dbg, const QEvdevGamepadDevice::EvdevAxisInfo &axisInfo);

class QEvdevGamepadBackend : public QGamepadBackend
{
    Q_OBJECT

public:
    QEvdevGamepadBackend();
    bool start() Q_DECL_OVERRIDE;
    void stop() Q_DECL_OVERRIDE;
    void resetConfiguration(int deviceId) Q_DECL_OVERRIDE;
    bool isConfigurationNeeded(int deviceId) Q_DECL_OVERRIDE;
    bool configureButton(int deviceId, QGamepadManager::GamepadButton button) Q_DECL_OVERRIDE;
    bool configureAxis(int deviceId, QGamepadManager::GamepadAxis axis) Q_DECL_OVERRIDE;
    bool setCancelConfigureButton(int deviceId, QGamepadManager::GamepadButton button) Q_DECL_OVERRIDE;

private slots:
    void handleAddedDevice(const QString &device);
    void handleRemovedDevice(const QString &device);

private:
    QEvdevGamepadDevice *newDevice(const QByteArray &device);
    QEvdevGamepadDevice *device(int deviceId);

    QDeviceDiscovery *m_discovery;
    QVector<QEvdevGamepadDevice *> m_devices;
};

QT_END_NAMESPACE

#endif // QEVDEVGAMEPADCONTROLLER_H
