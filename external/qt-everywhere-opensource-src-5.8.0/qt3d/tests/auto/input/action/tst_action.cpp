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
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>
#include <Qt3DInput/private/action_p.h>
#include <Qt3DInput/QActionInput>
#include <Qt3DInput/QAction>
#include <Qt3DCore/private/qbackendnode_p.h>
#include "testpostmanarbiter.h"

class DummyActionInput : public Qt3DInput::QActionInput
{
    Q_OBJECT
public:
    DummyActionInput(Qt3DCore::QNode *parent = nullptr)
        : Qt3DInput::QActionInput(parent)
    {}
};

class tst_Action : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT

private Q_SLOTS:

    void checkPeerPropertyMirroring()
    {
        // GIVEN
        Qt3DInput::Input::Action backendAction;
        Qt3DInput::QAction action;
        Qt3DInput::QActionInput actionInput;

        action.addInput(&actionInput);

        // WHEN
        simulateInitialization(&action, &backendAction);

        // THEN
        QCOMPARE(backendAction.peerId(), action.id());
        QCOMPARE(backendAction.isEnabled(), action.isEnabled());
        QCOMPARE(backendAction.inputs().size(), action.inputs().size());

        const int inputsCount = backendAction.inputs().size();
        if (inputsCount > 0) {
            for (int i = 0; i < inputsCount; ++i)
                QCOMPARE(backendAction.inputs().at(i), action.inputs().at(i)->id());
        }
    }

    void checkInitialAndCleanedUpState()
    {
        // GIVEN
        Qt3DInput::Input::Action backendAction;

        // THEN
        QVERIFY(backendAction.peerId().isNull());
        QCOMPARE(backendAction.actionTriggered(), false);
        QCOMPARE(backendAction.isEnabled(), false);
        QCOMPARE(backendAction.inputs().size(), 0);

        // GIVEN
        Qt3DInput::QAction action;
        Qt3DInput::QActionInput axisInput;

        action.addInput(&axisInput);

        // WHEN
        simulateInitialization(&action, &backendAction);
        backendAction.setActionTriggered(true);
        backendAction.cleanup();

        // THEN
        QCOMPARE(backendAction.actionTriggered(), false);
        QCOMPARE(backendAction.isEnabled(), false);
        QCOMPARE(backendAction.inputs().size(), 0);
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DInput::Input::Action backendAction;
        Qt3DCore::QPropertyUpdatedChangePtr updateChange;

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setPropertyName("enabled");
        updateChange->setValue(true);
        backendAction.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(backendAction.isEnabled(), true);

        // WHEN
        DummyActionInput input;
        const Qt3DCore::QNodeId inputId = input.id();
        const auto nodeAddedChange = Qt3DCore::QPropertyNodeAddedChangePtr::create(Qt3DCore::QNodeId(), &input);
        nodeAddedChange->setPropertyName("input");
        backendAction.sceneChangeEvent(nodeAddedChange);

        // THEN
        QCOMPARE(backendAction.inputs().size(), 1);
        QCOMPARE(backendAction.inputs().first(), inputId);

        // WHEN
        const auto nodeRemovedChange = Qt3DCore::QPropertyNodeRemovedChangePtr::create(Qt3DCore::QNodeId(), &input);
        nodeRemovedChange->setPropertyName("input");
        backendAction.sceneChangeEvent(nodeRemovedChange);

        // THEN
        QCOMPARE(backendAction.inputs().size(), 0);
    }

    void checkActivePropertyBackendNotification()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DInput::Input::Action backendAction;
        backendAction.setEnabled(true);
        Qt3DCore::QBackendNodePrivate::get(&backendAction)->setArbiter(&arbiter);
        const bool currentActionTriggeredValue = backendAction.actionTriggered();

        // WHEN
        backendAction.setActionTriggered(true);

        // THEN
        QVERIFY(currentActionTriggeredValue != backendAction.actionTriggered());
        QCOMPARE(arbiter.events.count(), 1);
        Qt3DCore::QPropertyUpdatedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "active");
        QCOMPARE(change->value().toBool(), backendAction.actionTriggered());

        arbiter.events.clear();

        // WHEN
        backendAction.setActionTriggered(true);

        // THEN
        QVERIFY(currentActionTriggeredValue != backendAction.actionTriggered());
        QCOMPARE(arbiter.events.count(), 0);

        arbiter.events.clear();
    }

    void shouldNotActivateWhenDisabled()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DInput::Input::Action backendAction;
        backendAction.setEnabled(false);
        Qt3DCore::QBackendNodePrivate::get(&backendAction)->setArbiter(&arbiter);

        // WHEN
        backendAction.setActionTriggered(true);

        // THEN
        QVERIFY(!backendAction.actionTriggered());
        QCOMPARE(arbiter.events.count(), 0);
    }
};

QTEST_APPLESS_MAIN(tst_Action)

#include "tst_action.moc"
