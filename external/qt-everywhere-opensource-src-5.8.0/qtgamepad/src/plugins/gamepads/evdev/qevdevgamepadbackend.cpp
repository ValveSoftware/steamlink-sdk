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

#include "qevdevgamepadbackend_p.h"
#include <QtCore/QSocketNotifier>
#include <QtCore/QLoggingCategory>
#include <QtDeviceDiscoverySupport/private/qdevicediscovery_p.h>
#include <QtCore/private/qcore_unix_p.h>
#include <linux/input.h>

#include <cmath>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcEGB, "qt.gamepad")

QEvdevGamepadDevice::EvdevAxisInfo::EvdevAxisInfo()
    : QGamepadBackend::AxisInfo<int>(0, 1, QGamepadManager::AxisInvalid)
{
}

QEvdevGamepadDevice::EvdevAxisInfo::EvdevAxisInfo(int fd, quint16 abs, int min, int max, QGamepadManager::GamepadAxis gamepadAxis)
    : QGamepadBackend::AxisInfo<int>(min, max, gamepadAxis)
    , flat(0)
    , gamepadMinButton(QGamepadManager::ButtonInvalid)
    , gamepadMaxButton(QGamepadManager::ButtonInvalid)
    , gamepadLastButton(QGamepadManager::ButtonInvalid)
{
    setAbsInfo(fd, abs);
}

double QEvdevGamepadDevice::EvdevAxisInfo::normalized(int value) const
{
    double val = AxisInfo::normalized(value);
    if (qAbs(val) <= flat)
        val = 0;
    return val;
}

void QEvdevGamepadDevice::EvdevAxisInfo::setAbsInfo(int fd, int abs)
{
    input_absinfo absInfo;
    memset(&absInfo, 0, sizeof(input_absinfo));
    if (ioctl(fd, EVIOCGABS(abs), &absInfo) >= 0) {
        minValue = absInfo.minimum;
        maxValue = absInfo.maximum;
        if (maxValue - minValue)
            flat = std::abs(absInfo.flat / double(maxValue - minValue));
    }
}

void QEvdevGamepadDevice::EvdevAxisInfo::restoreSavedData(int fd, int abs, const QVariantMap &value)
{
    gamepadAxis = QGamepadManager::GamepadAxis(value[QLatin1Literal("axis")].toInt());
    gamepadMinButton = QGamepadManager::GamepadButton(value[QLatin1Literal("minButton")].toInt());
    gamepadMaxButton = QGamepadManager::GamepadButton(value[QLatin1Literal("maxButton")].toInt());
    setAbsInfo(fd, abs);
}

QVariantMap QEvdevGamepadDevice::EvdevAxisInfo::dataToSave() const
{
    QVariantMap data;
    data[QLatin1Literal("axis")] = gamepadAxis;
    data[QLatin1Literal("minButton")] = gamepadMinButton;
    data[QLatin1Literal("maxButton")] = gamepadMaxButton;
    return data;
}

QEvdevGamepadBackend::QEvdevGamepadBackend()
{
}

bool QEvdevGamepadBackend::start()
{
    qCDebug(lcEGB) << "start";
    QByteArray device = qgetenv("QT_GAMEPAD_DEVICE");
    if (device.isEmpty()) {
        qCDebug(lcEGB) << "Using device discovery";
        m_discovery = QDeviceDiscovery::create(QDeviceDiscovery::Device_Joystick, this);
        if (m_discovery) {
            const QStringList devices = m_discovery->scanConnectedDevices();
            for (const QString &devStr : devices) {
                device = devStr.toUtf8();
                m_devices.append(newDevice(device));
            }
            connect(m_discovery, SIGNAL(deviceDetected(QString)), this, SLOT(handleAddedDevice(QString)));
            connect(m_discovery, SIGNAL(deviceRemoved(QString)), this, SLOT(handleRemovedDevice(QString)));
        } else {
            qWarning("No device specified, set QT_GAMEPAD_DEVICE");
            return false;
        }
    } else {
        qCDebug(lcEGB) << "Using device" << device;
        m_devices.append(newDevice(device));
    }

    return true;
}

QEvdevGamepadDevice *QEvdevGamepadBackend::newDevice(const QByteArray &device)
{
    qCDebug(lcEGB) << "Opening device" << device;
    return new QEvdevGamepadDevice(device, this);
}

QEvdevGamepadDevice *QEvdevGamepadBackend::device(int deviceId)
{
    for (QEvdevGamepadDevice *device : qAsConst(m_devices))
        if (device->deviceId() == deviceId)
            return device;
    return nullptr;
}

void QEvdevGamepadBackend::resetConfiguration(int deviceId)
{
    if (QEvdevGamepadDevice *dev = device(deviceId))
        return dev->resetConfiguration();
}

bool QEvdevGamepadBackend::isConfigurationNeeded(int deviceId)
{
    if (QEvdevGamepadDevice *dev = device(deviceId))
        return dev->isConfigurationNeeded();
    return false;
}

bool QEvdevGamepadBackend::configureButton(int deviceId, QGamepadManager::GamepadButton button)
{
    if (QEvdevGamepadDevice *dev = device(deviceId))
        return dev->configureButton(button);
    return false;
}

bool QEvdevGamepadBackend::configureAxis(int deviceId, QGamepadManager::GamepadAxis axis)
{
    if (QEvdevGamepadDevice *dev = device(deviceId))
        return dev->configureAxis(axis);
    return false;
}

bool QEvdevGamepadBackend::setCancelConfigureButton(int deviceId, QGamepadManager::GamepadButton button)
{
    if (QEvdevGamepadDevice *dev = device(deviceId))
        return dev->setCancelConfigureButton(button);
    return false;
}

void QEvdevGamepadBackend::stop()
{
    qCDebug(lcEGB) << "stop";
    qDeleteAll(m_devices);
    m_devices.clear();
}

void QEvdevGamepadBackend::handleAddedDevice(const QString &device)
{
    // This does not imply that an actual controller is connected.
    // When connecting the wireless receiver 4 devices will show up right away.
    // Therefore gamepadAdded() will be emitted only later, when something is read.
    qCDebug(lcEGB) << "Connected device" << device;
    m_devices.append(newDevice(device.toUtf8()));
}

void QEvdevGamepadBackend::handleRemovedDevice(const QString &device)
{
    qCDebug(lcEGB) << "Disconnected device" << device;
    QByteArray dev = device.toUtf8();
    for (int i = 0; i < m_devices.count(); ++i) {
        if (m_devices[i]->deviceName() == dev) {
            delete m_devices[i];
            m_devices.removeAt(i);
            break;
        }
    }
}

QEvdevGamepadDevice::QEvdevGamepadDevice(const QByteArray &dev, QEvdevGamepadBackend *backend)
    : m_dev(dev),
      m_backend(backend),
      m_fd(-1),
      m_productId(0),
      m_needsConfigure(true),
      m_notifier(0),
      m_configureButton(QGamepadManager::ButtonInvalid),
      m_configureAxis(QGamepadManager::AxisInvalid)
{
    openDevice(dev);
}

QEvdevGamepadDevice::~QEvdevGamepadDevice()
{
    if (m_fd != -1)
        QT_CLOSE(m_fd);

    if (m_productId)
        emit m_backend->gamepadRemoved(m_productId);
}

void QEvdevGamepadDevice::resetConfiguration()
{
    m_axisMap.insert(ABS_X, EvdevAxisInfo(m_fd, ABS_X, -32768, 32767, QGamepadManager::AxisLeftX));
    m_axisMap.insert(ABS_Y, EvdevAxisInfo(m_fd, ABS_Y, -32768, 32767, QGamepadManager::AxisLeftY));
    m_axisMap.insert(ABS_RX, EvdevAxisInfo(m_fd, ABS_RX, -32768, 32767, QGamepadManager::AxisRightX));
    m_axisMap.insert(ABS_RY, EvdevAxisInfo(m_fd, ABS_RY, -32768, 32767, QGamepadManager::AxisRightY));
    m_axisMap.insert(ABS_Z, EvdevAxisInfo(m_fd, ABS_Z, 0, 255));
    m_axisMap[ABS_Z].gamepadMinButton = QGamepadManager::ButtonL2;
    m_axisMap[ABS_Z].gamepadMaxButton = QGamepadManager::ButtonL2;

    m_axisMap.insert(ABS_RZ, EvdevAxisInfo(m_fd, ABS_RZ, 0, 255));
    m_axisMap[ABS_RZ].gamepadMinButton = QGamepadManager::ButtonR2;
    m_axisMap[ABS_RZ].gamepadMaxButton = QGamepadManager::ButtonR2;

    m_axisMap.insert(ABS_HAT0X, EvdevAxisInfo(m_fd, ABS_HAT0X, -1, 1));
    m_axisMap[ABS_HAT0X].gamepadMinButton = QGamepadManager::ButtonLeft;
    m_axisMap[ABS_HAT0X].gamepadMaxButton = QGamepadManager::ButtonRight;

    m_axisMap.insert(ABS_HAT0Y, EvdevAxisInfo(m_fd, ABS_HAT0Y, -1, 1));
    m_axisMap[ABS_HAT0Y].gamepadMinButton = QGamepadManager::ButtonUp;
    m_axisMap[ABS_HAT0Y].gamepadMaxButton = QGamepadManager::ButtonDown;

    m_buttonsMap[BTN_START] = QGamepadManager::ButtonStart;
    m_buttonsMap[BTN_SELECT] = QGamepadManager::ButtonSelect;
    m_buttonsMap[BTN_MODE] = QGamepadManager::ButtonGuide;
    m_buttonsMap[BTN_X] = QGamepadManager::ButtonX;
    m_buttonsMap[BTN_Y] = QGamepadManager::ButtonY;
    m_buttonsMap[BTN_A] = QGamepadManager::ButtonA;
    m_buttonsMap[BTN_B] = QGamepadManager::ButtonB;
    m_buttonsMap[BTN_TL] = QGamepadManager::ButtonL1;
    m_buttonsMap[BTN_TR] = QGamepadManager::ButtonR1;
    m_buttonsMap[BTN_TL2] = QGamepadManager::ButtonL2;
    m_buttonsMap[BTN_TR2] = QGamepadManager::ButtonR2;
    m_buttonsMap[BTN_THUMB] = m_buttonsMap[BTN_THUMBL] = QGamepadManager::ButtonL3;
    m_buttonsMap[BTN_THUMBR] = QGamepadManager::ButtonR3;
    m_buttonsMap[BTN_TRIGGER_HAPPY1] = QGamepadManager::ButtonLeft;
    m_buttonsMap[BTN_TRIGGER_HAPPY2] = QGamepadManager::ButtonRight;
    m_buttonsMap[BTN_TRIGGER_HAPPY3] = QGamepadManager::ButtonUp;
    m_buttonsMap[BTN_TRIGGER_HAPPY4] = QGamepadManager::ButtonDown;

    if (m_productId)
        m_backend->saveSettings(m_productId, QVariant());
}

bool QEvdevGamepadDevice::isConfigurationNeeded()
{
    return m_needsConfigure;
}

bool QEvdevGamepadDevice::configureButton(QGamepadManager::GamepadButton button)
{
    m_configureButton = button;
    return true;
}

bool QEvdevGamepadDevice::configureAxis(QGamepadManager::GamepadAxis axis)
{
    m_configureAxis = axis;
    return true;
}

bool QEvdevGamepadDevice::setCancelConfigureButton(QGamepadManager::GamepadButton button)
{
    m_configureCancelButton = button;
    return true;
}

bool QEvdevGamepadDevice::openDevice(const QByteArray &dev)
{
    m_fd = QT_OPEN(dev.constData(), O_RDONLY | O_NDELAY, 0);

    if (m_fd >= 0) {
        m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
        connect(m_notifier, SIGNAL(activated(int)), this, SLOT(readData()));
        qCDebug(lcEGB) << "Successfully opened" << dev;
    } else {
        qErrnoWarning(errno, "Gamepad: Cannot open input device %s", qPrintable(dev));
        return false;
    }

    input_id id;
    if (ioctl(m_fd, EVIOCGID, &id) >= 0) {
        m_productId = id.product;

        QVariant settings = m_backend->readSettings(m_productId);
        if (!settings.isNull()) {
            m_needsConfigure = false;
            QVariantMap data = settings.toMap()[QLatin1Literal("axes")].toMap();
            for (QVariantMap::const_iterator it = data.begin(); it != data.end(); ++it) {
                const int key = it.key().toInt();
                m_axisMap[key].restoreSavedData(m_fd, key, it.value().toMap());
            }

            data = settings.toMap()[QLatin1Literal("buttons")].toMap();
            for (QVariantMap::const_iterator it = data.begin(); it != data.end(); ++it)
                m_buttonsMap[it.key().toInt()] = QGamepadManager::GamepadButton(it.value().toInt());
        }

        emit m_backend->gamepadAdded(m_productId);
    } else {
        QT_CLOSE(m_fd);
        m_fd = -1;
        return false;
    }

    if (m_needsConfigure)
        resetConfiguration();

    qCDebug(lcEGB) << "Axis limits:" << m_axisMap;

    return true;
}

QDebug operator<<(QDebug dbg, const QEvdevGamepadDevice::EvdevAxisInfo &axisInfo)
{
    dbg.nospace() << "AxisInfo(min=" << axisInfo.minValue << ", max=" << axisInfo.maxValue << ")";
    return dbg.space();
}

void QEvdevGamepadDevice::readData()
{
    input_event buffer[32];
    int events = 0, n = 0;
    for (; ;) {
        events = QT_READ(m_fd, reinterpret_cast<char*>(buffer) + n, sizeof(buffer) - n);
        if (events <= 0)
            goto err;
        n += events;
        if (n % sizeof(::input_event) == 0)
            break;
    }

    n /= sizeof(::input_event);

    for (int i = 0; i < n; ++i)
        processInputEvent(&buffer[i]);

    return;

err:
    if (!events) {
        qWarning("Gamepad: Got EOF from input device");
        return;
    } else if (events < 0) {
        if (errno != EINTR && errno != EAGAIN) {
            qErrnoWarning(errno, "Gamepad: Could not read from input device");
            if (errno == ENODEV) { // device got disconnected -> stop reading
                delete m_notifier;
                m_notifier = 0;
                QT_CLOSE(m_fd);
                m_fd = -1;
            }
        }
    }
}

void QEvdevGamepadDevice::saveData()
{
    if (!m_productId)
        return ;

    QVariantMap settings, data;
    for (AxisMap::const_iterator it = m_axisMap.begin(); it != m_axisMap.end(); ++it)
        data[QString::number(it.key())] = it.value().dataToSave();
    settings[QLatin1Literal("axes")] = data;

    data.clear();
    for (ButtonsMap::const_iterator it = m_buttonsMap.begin(); it != m_buttonsMap.end(); ++it)
        data[QString::number(it.key())] = it.value();

    settings[QLatin1Literal("buttons")] = data;

    m_backend->saveSettings(m_productId, settings);
}

void QEvdevGamepadDevice::processInputEvent(input_event *e)
{
    if (e->type == EV_KEY) {
        QGamepadManager::GamepadButton btn = QGamepadManager::ButtonInvalid;
        ButtonsMap::const_iterator it = m_buttonsMap.find(e->code);
        if (it != m_buttonsMap.end())
            btn = it.value();

        const bool pressed = e->value;
        if (m_configureCancelButton != QGamepadManager::ButtonInvalid &&
                m_configureCancelButton != m_configureButton &&
                !pressed && btn == m_configureCancelButton &&
                (m_configureButton != QGamepadManager::ButtonInvalid ||
                 m_configureAxis != QGamepadManager::AxisInvalid)) {
            m_configureButton = QGamepadManager::ButtonInvalid;
            m_configureAxis = QGamepadManager::AxisInvalid;
            emit m_backend->configurationCanceled(m_productId);
            return;
        }

        if (!pressed && m_configureButton != QGamepadManager::ButtonInvalid) {
            m_buttonsMap[e->code] = m_configureButton;
            QGamepadManager::GamepadButton but = m_configureButton;
            m_configureButton = QGamepadManager::ButtonInvalid;
            saveData();
            emit m_backend->buttonConfigured(m_productId, but);
        }

        it = m_buttonsMap.find(e->code);
        if (it != m_buttonsMap.end())
            btn = it.value();

        if (btn != QGamepadManager::ButtonInvalid) {
            if (pressed)
                emit m_backend->gamepadButtonPressed(m_productId, btn, 1.0);
            else
                emit m_backend->gamepadButtonReleased(m_productId, btn);
        }
    } else if (e->type == EV_ABS) {
        if (m_configureAxis != QGamepadManager::AxisInvalid) {
            EvdevAxisInfo inf(m_fd, e->code, -32768, 32767, m_configureAxis);
            if (std::abs(inf.normalized(e->value)) == 1) {
                m_axisMap.insert(e->code, EvdevAxisInfo(m_fd, e->code, -32768, 32767, m_configureAxis));

                QGamepadManager::GamepadAxis axis = m_configureAxis;
                m_configureAxis = QGamepadManager::AxisInvalid;

                saveData();
                emit m_backend->axisConfigured(m_productId, axis);
            } else {
                return;
            }
        }

        AxisMap::iterator it = m_axisMap.find(e->code);
        if (m_configureButton != QGamepadManager::ButtonInvalid) {
            EvdevAxisInfo axisInfo;
            if (it != m_axisMap.end())
                axisInfo = it.value();
            else
                axisInfo = EvdevAxisInfo(m_fd, e->code);

            axisInfo.gamepadAxis = QGamepadManager::AxisInvalid;

            bool save = false;
            if (e->value == axisInfo.minValue) {
                axisInfo.gamepadMinButton = m_configureButton;
                if (axisInfo.gamepadMaxButton != QGamepadManager::ButtonInvalid)
                    axisInfo.gamepadMaxButton = m_configureButton;
                save = true;
            } else if (e->value == axisInfo.maxValue) {
                axisInfo.gamepadMaxButton = m_configureButton;
                if (axisInfo.gamepadMinButton != QGamepadManager::ButtonInvalid)
                    axisInfo.gamepadMinButton = m_configureButton;
                save = true;
            }

            if (save) {
                QGamepadManager::GamepadButton but = m_configureButton;
                m_configureButton = QGamepadManager::ButtonInvalid;
                if (but == QGamepadManager::ButtonL2 || but == QGamepadManager::ButtonR2)
                    m_axisMap.insert(e->code, axisInfo);
                saveData();
                emit m_backend->buttonConfigured(m_productId, but);
            }
        }

        it = m_axisMap.find(e->code);
        if (it == m_axisMap.end())
            return;

        EvdevAxisInfo &info = it.value();

        double val = info.normalized(e->value);

        if (info.gamepadAxis != QGamepadManager::AxisInvalid)
            emit m_backend->gamepadAxisMoved(m_productId, info.gamepadAxis, val);

        if (info.gamepadMaxButton == info.gamepadMinButton &&
                info.gamepadMaxButton != QGamepadManager::ButtonInvalid) {
            if (val)
                emit m_backend->gamepadButtonPressed(m_productId, info.gamepadMaxButton, std::abs(val));
            else
                emit m_backend->gamepadButtonReleased(m_productId, info.gamepadMaxButton);
        } else {
            if (info.gamepadMaxButton != QGamepadManager::ButtonInvalid
                    && val == 1.0) {
                info.gamepadLastButton = info.gamepadMaxButton;
                emit m_backend->gamepadButtonPressed(m_productId, info.gamepadMaxButton, val);
            } else if (info.gamepadMinButton != QGamepadManager::ButtonInvalid
                       && val == -1.0) {
                info.gamepadLastButton = info.gamepadMinButton;
                emit m_backend->gamepadButtonPressed(m_productId, info.gamepadMinButton, -val);
            } else if (!val && info.gamepadLastButton != QGamepadManager::ButtonInvalid) {
                QGamepadManager::GamepadButton but = info.gamepadLastButton;
                info.gamepadLastButton = QGamepadManager::ButtonInvalid;
                emit m_backend->gamepadButtonReleased(m_productId, but);
            }
        }
    }
}

QT_END_NAMESPACE
