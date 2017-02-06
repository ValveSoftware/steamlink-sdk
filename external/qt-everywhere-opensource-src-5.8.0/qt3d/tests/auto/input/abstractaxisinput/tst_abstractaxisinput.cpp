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

#include <QtTest/QTest>
#include <qbackendnodetester.h>
#include <Qt3DCore/private/qnode_p.h>
#include <Qt3DCore/private/qscene_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DInput/private/abstractaxisinput_p.h>
#include <Qt3DInput/QAbstractAxisInput>
#include <Qt3DInput/private/qabstractaxisinput_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include "testdevice.h"

class DummyAxisInput : public Qt3DInput::QAbstractAxisInput
{
    Q_OBJECT
public:
    explicit DummyAxisInput(QNode *parent = nullptr)
        : Qt3DInput::QAbstractAxisInput(*new Qt3DInput::QAbstractAxisInputPrivate, parent)
    {
    }

private:
    Qt3DCore::QNodeCreatedChangeBasePtr createNodeCreationChange() const Q_DECL_OVERRIDE
    {
        auto creationChange = Qt3DCore::QNodeCreatedChangePtr<Qt3DInput::QAbstractAxisInputData>::create(this);
        auto &data = creationChange->data;
        data.sourceDeviceId = qIdForNode(sourceDevice());
        return creationChange;
    }
};

class DummyAxisInputBackend : public Qt3DInput::Input::AbstractAxisInput
{
public:
    explicit DummyAxisInputBackend()
        : AbstractAxisInput()
    {
    }

    float process(Qt3DInput::Input::InputHandler *inputHandler, qint64 currentTime) Q_DECL_OVERRIDE
    {
        Q_UNUSED(inputHandler);
        Q_UNUSED(currentTime);
        return 0.0f;
    }
};


class tst_AbstractAxisInput : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT

private Q_SLOTS:

    void checkPeerPropertyMirroring()
    {
        // GIVEN
        DummyAxisInputBackend backendAxisInput;
        DummyAxisInput axisInput;
        TestDevice sourceDevice;

        axisInput.setSourceDevice(&sourceDevice);

        // WHEN
        simulateInitialization(&axisInput, &backendAxisInput);

        // THEN
        QCOMPARE(backendAxisInput.peerId(), axisInput.id());
        QCOMPARE(backendAxisInput.isEnabled(), axisInput.isEnabled());
        QCOMPARE(backendAxisInput.sourceDevice(), sourceDevice.id());
    }

    void checkInitialAndCleanedUpState()
    {
        // GIVEN
        DummyAxisInputBackend backendAxisInput;

        // THEN
        QVERIFY(backendAxisInput.peerId().isNull());
        QCOMPARE(backendAxisInput.isEnabled(), false);
        QCOMPARE(backendAxisInput.sourceDevice(), Qt3DCore::QNodeId());

        // GIVEN
        DummyAxisInput axisInput;
        TestDevice sourceDevice;

        axisInput.setSourceDevice(&sourceDevice);

        // WHEN
        simulateInitialization(&axisInput, &backendAxisInput);
        backendAxisInput.cleanup();

        // THEN
        QCOMPARE(backendAxisInput.isEnabled(), false);
        QCOMPARE(backendAxisInput.sourceDevice(), Qt3DCore::QNodeId());
    }

    void checkPropertyChanges()
    {
        // GIVEN
        DummyAxisInputBackend backendAxisInput;

        // WHEN
        Qt3DCore::QPropertyUpdatedChangePtr updateChange(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setPropertyName("enabled");
        updateChange->setValue(true);
        backendAxisInput.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(backendAxisInput.isEnabled(), true);

        // WHEN
        TestDevice device;
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setPropertyName("sourceDevice");
        updateChange->setValue(QVariant::fromValue(device.id()));
        backendAxisInput.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(backendAxisInput.sourceDevice(), device.id());
    }
};

QTEST_APPLESS_MAIN(tst_AbstractAxisInput)

#include "tst_abstractaxisinput.moc"
