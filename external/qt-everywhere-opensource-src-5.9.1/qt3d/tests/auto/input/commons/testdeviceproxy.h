/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/


#ifndef TESTDEVICEPROXY_H
#define TESTDEVICEPROXY_H

#include <Qt3DInput/private/qabstractphysicaldeviceproxy_p.h>
#include <Qt3DInput/private/qabstractphysicaldeviceproxy_p_p.h>

class TestPhysicalDevice : public Qt3DInput::QAbstractPhysicalDevice
{
    Q_OBJECT
public:
    explicit TestPhysicalDevice(Qt3DCore::QNode *parent = nullptr)
        : Qt3DInput::QAbstractPhysicalDevice(parent)
    {}

    ~TestPhysicalDevice()
    {
    }

    enum AxisValues {
        X = 0,
        Y,
        Z
    };

    enum ButtonValues {
        Left = 0,
        Right
    };

    int axisCount() const Q_DECL_FINAL { return 3; }
    int buttonCount() const Q_DECL_FINAL { return 2; }
    QStringList axisNames() const Q_DECL_FINAL { return QStringList() << QStringLiteral("x") << QStringLiteral("y") << QStringLiteral("z"); }
    QStringList buttonNames() const Q_DECL_FINAL { return QStringList() << QStringLiteral("Left") << QStringLiteral("Right"); }

    int axisIdentifier(const QString &name) const Q_DECL_FINAL
    {
        if (name == QLatin1String("x"))
            return TestPhysicalDevice::X;
        if (name == QLatin1String("y"))
            return TestPhysicalDevice::Y;
        if (name == QLatin1String("z"))
            return TestPhysicalDevice::Z;
        return -1;
    }
    int buttonIdentifier(const QString &name) const Q_DECL_FINAL
    {
        if (name == QLatin1String("Left"))
            return TestPhysicalDevice::Left;
        if (name == QLatin1String("Right"))
            return TestPhysicalDevice::Right;
        return -1;
    }
};

class TestProxyPrivate : public Qt3DInput::QAbstractPhysicalDeviceProxyPrivate
{
public:
    explicit TestProxyPrivate(const QString &name)
        : Qt3DInput::QAbstractPhysicalDeviceProxyPrivate(name)
    {}
};

class TestProxy : public Qt3DInput::QAbstractPhysicalDeviceProxy
{
    Q_OBJECT
public:
    explicit TestProxy(const QString &name = QStringLiteral("TestProxy"))
        : Qt3DInput::QAbstractPhysicalDeviceProxy(*new TestProxyPrivate(name))
    {}

    Qt3DInput::QAbstractPhysicalDevice *device() const
    {
        Q_D(const TestProxy);
        return d->m_device;
    }

    void simulateSceneChangeEvent(const Qt3DCore::QSceneChangePtr &change)
    {
        Qt3DInput::QAbstractPhysicalDeviceProxy::sceneChangeEvent(change);
    }

private:
    Q_DECLARE_PRIVATE(TestProxy)
};

#endif // TESTDEVICEPROXY_H
