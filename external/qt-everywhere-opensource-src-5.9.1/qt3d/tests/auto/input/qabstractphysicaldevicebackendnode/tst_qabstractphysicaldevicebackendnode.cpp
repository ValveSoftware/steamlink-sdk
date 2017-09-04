/****************************************************************************
**
** Copyright (C) 2016 Paul Lemire <paul.lemire350@gmail.com>
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


#include <QtTest/QTest>
#include <Qt3DInput/private/qabstractphysicaldevicebackendnode_p.h>
#include <Qt3DInput/private/qabstractphysicaldevicebackendnode_p_p.h>
#include <Qt3DInput/private/inputhandler_p.h>
#include <Qt3DInput/qaxissetting.h>
#include <Qt3DInput/qinputaspect.h>
#include <Qt3DInput/private/qinputaspect_p.h>
#include <Qt3DInput/private/inputmanagers_p.h>
#include <Qt3DInput/private/axissetting_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>
#include "testdevice.h"

class TestPhysicalDeviceBackendNode : public Qt3DInput::QAbstractPhysicalDeviceBackendNode
{
public:
    TestPhysicalDeviceBackendNode(Qt3DCore::QBackendNode::Mode mode = Qt3DCore::QBackendNode::ReadOnly)
        : Qt3DInput::QAbstractPhysicalDeviceBackendNode(mode)
    {}

    float axisValue(int axisIdentifier) const Q_DECL_OVERRIDE
    {
        if (axisIdentifier == 883)
            return 883.0f;
        return 0.0f;
    }

    bool isButtonPressed(int buttonIdentifier) const Q_DECL_OVERRIDE
    {
        if (buttonIdentifier == 454)
            return true;
        return false;
    }

};

class tst_QAbstractPhysicalDeviceBackendNode : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT

private Q_SLOTS:

    void checkInitialState()
    {
        // GIVEN
        TestPhysicalDeviceBackendNode backendQAbstractPhysicalDeviceBackendNode;

        // THEN
        QCOMPARE(backendQAbstractPhysicalDeviceBackendNode.isEnabled(), false);
        QVERIFY(backendQAbstractPhysicalDeviceBackendNode.inputAspect() == nullptr);
        QVERIFY(backendQAbstractPhysicalDeviceBackendNode.peerId().isNull());
    }

    void checkAxisValue()
    {
        // GIVEN
        TestPhysicalDeviceBackendNode backendQAbstractPhysicalDeviceBackendNode;

        // WHEN
        float axisValue = backendQAbstractPhysicalDeviceBackendNode.axisValue(883);
        // THEN
        QCOMPARE(axisValue, 883.0f);

        // WHEN
        axisValue = backendQAbstractPhysicalDeviceBackendNode.axisValue(454);
        // THEN
        QCOMPARE(axisValue, 0.0f);
    }

    void checkButtonPressed()
    {
        // GIVEN
        TestPhysicalDeviceBackendNode backendQAbstractPhysicalDeviceBackendNode;

        // WHEN
        bool buttonPressed = backendQAbstractPhysicalDeviceBackendNode.isButtonPressed(883);
        // THEN
        QCOMPARE(buttonPressed, false);

        // WHEN
        buttonPressed = backendQAbstractPhysicalDeviceBackendNode.isButtonPressed(454);
        // THEN
        QCOMPARE(buttonPressed, true);
    }

    void checkCleanupState()
    {
        // GIVEN
        TestPhysicalDeviceBackendNode backendQAbstractPhysicalDeviceBackendNode;
        Qt3DInput::QInputAspect aspect;

        // WHEN
        backendQAbstractPhysicalDeviceBackendNode.setEnabled(true);
        backendQAbstractPhysicalDeviceBackendNode.setInputAspect(&aspect);

        // THEN
        QCOMPARE(backendQAbstractPhysicalDeviceBackendNode.inputAspect(), &aspect);

        // WHEN
        backendQAbstractPhysicalDeviceBackendNode.cleanup();

        // THEN
        QCOMPARE(backendQAbstractPhysicalDeviceBackendNode.isEnabled(), false);
        QVERIFY(backendQAbstractPhysicalDeviceBackendNode.inputAspect() == nullptr);
    }

    void checkInitializeFromPeer()
    {
        // GIVEN
        TestDevice physicalDeviceNode;

        {
            // WHEN
            TestPhysicalDeviceBackendNode backendQAbstractPhysicalDeviceBackendNode;
            simulateInitialization(&physicalDeviceNode, &backendQAbstractPhysicalDeviceBackendNode);

            // THEN
            QCOMPARE(backendQAbstractPhysicalDeviceBackendNode.isEnabled(), true);
            QCOMPARE(backendQAbstractPhysicalDeviceBackendNode.peerId(), physicalDeviceNode.id());
        }
        {
            // WHEN
            TestPhysicalDeviceBackendNode backendQAbstractPhysicalDeviceBackendNode;
            physicalDeviceNode.setEnabled(false);
            simulateInitialization(&physicalDeviceNode, &backendQAbstractPhysicalDeviceBackendNode);

            // THEN
            QCOMPARE(backendQAbstractPhysicalDeviceBackendNode.peerId(), physicalDeviceNode.id());
            QCOMPARE(backendQAbstractPhysicalDeviceBackendNode.isEnabled(), false);
        }
    }

    void checkSceneChangeEvents()
    {
        // GIVEN
        TestPhysicalDeviceBackendNode backendQAbstractPhysicalDeviceBackendNode;
        Qt3DInput::QInputAspect aspect;
        backendQAbstractPhysicalDeviceBackendNode.setInputAspect(&aspect);

        {
            // WHEN
            const bool newValue = false;
            const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
            change->setPropertyName("enabled");
            change->setValue(newValue);
            backendQAbstractPhysicalDeviceBackendNode.sceneChangeEvent(change);

            // THEN
            QCOMPARE(backendQAbstractPhysicalDeviceBackendNode.isEnabled(), newValue);
        }

        {
            Qt3DInput::QAxisSetting settings1;
            Qt3DInput::QAxisSetting settings2;

            settings1.setAxes(QVector<int>() << 883);
            settings2.setAxes(QVector<int>() << 454);
            Qt3DInput::QAbstractPhysicalDeviceBackendNodePrivate *priv = static_cast<Qt3DInput::QAbstractPhysicalDeviceBackendNodePrivate *>(
                        Qt3DCore::QBackendNodePrivate::get(&backendQAbstractPhysicalDeviceBackendNode));

            // Create backend resource
            {
                Qt3DInput::QInputAspectPrivate *aspectPrivate = static_cast<Qt3DInput::QInputAspectPrivate *>(Qt3DCore::QAbstractAspectPrivate::get(&aspect));
                Qt3DInput::Input::InputHandler *handler = aspectPrivate->m_inputHandler.data();
                Qt3DInput::Input::AxisSetting *backendSetting1 = handler->axisSettingManager()->getOrCreateResource(settings1.id());
                Qt3DInput::Input::AxisSetting *backendSetting2 = handler->axisSettingManager()->getOrCreateResource(settings2.id());
                simulateInitialization(&settings1, backendSetting1);
                simulateInitialization(&settings2, backendSetting2);
            }

            // Adding AxisSettings
            {
                // WHEN
                auto change = Qt3DCore::QPropertyNodeAddedChangePtr::create(Qt3DCore::QNodeId(), &settings1);
                change->setPropertyName("axisSettings");
                backendQAbstractPhysicalDeviceBackendNode.sceneChangeEvent(change);

                // THEN
                QCOMPARE(priv->m_axisSettings.size(), 1);

                // WHEN
                change = Qt3DCore::QPropertyNodeAddedChangePtr::create(Qt3DCore::QNodeId(), &settings2);
                change->setPropertyName("axisSettings");
                backendQAbstractPhysicalDeviceBackendNode.sceneChangeEvent(change);

                // THEN
                QCOMPARE(priv->m_axisSettings.size(), 2);
            }
            // Removing AxisSettings
            {
                // WHEN
                auto change = Qt3DCore::QPropertyNodeRemovedChangePtr::create(Qt3DCore::QNodeId(), &settings1);
                change->setPropertyName("axisSettings");
                backendQAbstractPhysicalDeviceBackendNode.sceneChangeEvent(change);

                // THEN
                QCOMPARE(priv->m_axisSettings.size(), 1);

                // WHEN
                change = Qt3DCore::QPropertyNodeRemovedChangePtr::create(Qt3DCore::QNodeId(), &settings2);
                change->setPropertyName("axisSettings");
                backendQAbstractPhysicalDeviceBackendNode.sceneChangeEvent(change);

                // THEN
                QCOMPARE(priv->m_axisSettings.size(), 0);
            }

        }
    }

};

QTEST_MAIN(tst_QAbstractPhysicalDeviceBackendNode)

#include "tst_qabstractphysicaldevicebackendnode.moc"
