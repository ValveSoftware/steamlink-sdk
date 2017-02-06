/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
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

#ifndef TESTDEVICE_H
#define TESTDEVICE_H

#include <Qt3DCore/private/qnode_p.h>
#include <Qt3DInput/QAbstractPhysicalDevice>
#include <Qt3DInput/private/qabstractphysicaldevicebackendnode_p.h>
#include <Qt3DInput/private/qinputdeviceintegration_p.h>
#include <qbackendnodetester.h>

class TestDevice : public Qt3DInput::QAbstractPhysicalDevice
{
    Q_OBJECT
public:
    explicit TestDevice(Qt3DCore::QNode *parent = nullptr)
        : Qt3DInput::QAbstractPhysicalDevice(parent)
    {}

    int axisCount() const Q_DECL_FINAL { return 0; }
    int buttonCount() const Q_DECL_FINAL { return 0; }
    QStringList axisNames() const Q_DECL_FINAL { return QStringList(); }
    QStringList buttonNames() const Q_DECL_FINAL { return QStringList(); }
    int axisIdentifier(const QString &name) const Q_DECL_FINAL { Q_UNUSED(name) return 0; }
    int buttonIdentifier(const QString &name) const Q_DECL_FINAL { Q_UNUSED(name) return 0; }

private:
    friend class TestDeviceBackendNode;
};

class TestDeviceBackendNode : public Qt3DInput::QAbstractPhysicalDeviceBackendNode
{
public:
    explicit TestDeviceBackendNode(TestDevice *device)
        : Qt3DInput::QAbstractPhysicalDeviceBackendNode(ReadOnly)
    {
        Qt3DCore::QBackendNodeTester().simulateInitialization(device, this);
    }

    float axisValue(int axisIdentifier) const Q_DECL_FINAL
    {
        return m_axisValues.value(axisIdentifier);
    }

    void setAxisValue(int axisIdentifier, float value)
    {
        m_axisValues.insert(axisIdentifier, value);
    }

    bool isButtonPressed(int buttonIdentifier) const Q_DECL_FINAL
    {
        return m_buttonStates.value(buttonIdentifier);
    }

    void setButtonPressed(int buttonIdentifier, bool pressed)
    {
        m_buttonStates.insert(buttonIdentifier, pressed);
    }

private:
    QHash<int, float> m_axisValues;
    QHash<int, bool> m_buttonStates;
};

class TestDeviceIntegration : public Qt3DInput::QInputDeviceIntegration
{
    Q_OBJECT
public:
    explicit TestDeviceIntegration(QObject *parent = nullptr)
        : Qt3DInput::QInputDeviceIntegration(parent),
          m_devicesParent(new Qt3DCore::QNode)
    {
    }

    ~TestDeviceIntegration()
    {
        qDeleteAll(m_deviceBackendNodes);
    }

    QVector<Qt3DCore::QAspectJobPtr> jobsToExecute(qint64 time) Q_DECL_FINAL
    {
        Q_UNUSED(time);
        return QVector<Qt3DCore::QAspectJobPtr>();
    }

    TestDevice *createPhysicalDevice(const QString &name) Q_DECL_FINAL
    {
        Q_ASSERT(!deviceNames().contains(name));
        auto device = new TestDevice(m_devicesParent.data()); // Avoids unwanted reparenting
        device->setObjectName(name);
        m_devices.append(device);
        m_deviceBackendNodes.append(new TestDeviceBackendNode(device));
        return device;
    }

    QVector<Qt3DCore::QNodeId> physicalDevices() const Q_DECL_FINAL
    {
        QVector<Qt3DCore::QNodeId> ids;
        std::transform(m_devices.constBegin(), m_devices.constEnd(),
                       std::back_inserter(ids),
                       [] (TestDevice *device) { return device->id(); });
        return ids;
    }

    TestDeviceBackendNode *physicalDevice(Qt3DCore::QNodeId id) const Q_DECL_FINAL
    {
        auto it = std::find_if(m_deviceBackendNodes.constBegin(), m_deviceBackendNodes.constEnd(),
                               [id] (TestDeviceBackendNode *node) { return node->peerId() == id; });
        if (it == m_deviceBackendNodes.constEnd())
            return nullptr;
        else
            return *it;
    }

    QStringList deviceNames() const Q_DECL_FINAL
    {
        QStringList names;
        std::transform(m_devices.constBegin(), m_devices.constEnd(),
                       std::back_inserter(names),
                       [] (TestDevice *device) { return device->objectName(); });
        return names;
    }

private:
    void onInitialize() Q_DECL_FINAL {}

    QScopedPointer<Qt3DCore::QNode> m_devicesParent;
    QVector<TestDevice*> m_devices;
    QVector<TestDeviceBackendNode*> m_deviceBackendNodes;
};

#endif // TESTDEVICE_H
