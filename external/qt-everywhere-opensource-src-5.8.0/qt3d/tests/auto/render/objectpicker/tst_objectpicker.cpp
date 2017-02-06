/****************************************************************************
**
** Copyright (C) 2015 Paul Lemire paul.lemire350@gmail.com
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
#include <Qt3DRender/private/objectpicker_p.h>
#include <Qt3DRender/qpickevent.h>
#include <Qt3DRender/qobjectpicker.h>
#include <Qt3DCore/private/qbackendnode_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include "testpostmanarbiter.h"
#include "testrenderer.h"

class tst_ObjectPicker : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT
private Q_SLOTS:

    void checkPeerPropertyMirroring()
    {
        // GIVEN
        Qt3DRender::Render::ObjectPicker objectPicker;
        Qt3DRender::QObjectPicker picker;
        picker.setHoverEnabled(true);

        // WHEN
        simulateInitialization(&picker, &objectPicker);

        // THEN
        QVERIFY(!objectPicker.peerId().isNull());
        QCOMPARE(objectPicker.isHoverEnabled(), true);
        QCOMPARE(objectPicker.isDirty(), true);
    }

    void checkInitialAndCleanedUpState()
    {
        // GIVEN
        Qt3DRender::Render::ObjectPicker objectPicker;

        // THEN
        QVERIFY(objectPicker.peerId().isNull());
        QCOMPARE(objectPicker.isHoverEnabled(), false);
        QCOMPARE(objectPicker.isDirty(), false);

        // GIVEN
        Qt3DRender::QObjectPicker picker;
        picker.setHoverEnabled(true);

        // WHEN
        simulateInitialization(&picker, &objectPicker);
        objectPicker.cleanup();

        // THEN
        QCOMPARE(objectPicker.isHoverEnabled(), false);
        QCOMPARE(objectPicker.isDirty(), false);
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DRender::Render::ObjectPicker objectPicker;
        TestRenderer renderer;
        objectPicker.setRenderer(&renderer);

        QVERIFY(!objectPicker.isDirty());

        // WHEN
        Qt3DCore::QPropertyUpdatedChangePtr updateChange(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setValue(true);
        updateChange->setPropertyName("hoverEnabled");
        objectPicker.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(objectPicker.isHoverEnabled(), true);
        QVERIFY(objectPicker.isDirty());
        QVERIFY(renderer.dirtyBits() != 0);

        objectPicker.unsetDirty();
        QVERIFY(!objectPicker.isDirty());
    }

    void checkBackendPropertyNotifications()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::Render::ObjectPicker objectPicker;
        Qt3DCore::QBackendNodePrivate::get(&objectPicker)->setArbiter(&arbiter);
        Qt3DRender::QPickEventPtr event(new Qt3DRender::QPickEvent);
        QVERIFY(!objectPicker.isDirty());

        // WHEN
        objectPicker.onPressed(event);

        // THEN
        QCOMPARE(arbiter.events.count(), 1);
        Qt3DCore::QPropertyUpdatedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "pressed");

        arbiter.events.clear();

        // WHEN
        objectPicker.onReleased(event);

        // THEN
        QCOMPARE(arbiter.events.count(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "released");

        arbiter.events.clear();

        // WHEN
        objectPicker.onClicked(event);

        // THEN
        QCOMPARE(arbiter.events.count(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "clicked");

        arbiter.events.clear();

        // WHEN
        objectPicker.onEntered();

        // THEN
        QCOMPARE(arbiter.events.count(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "entered");

        arbiter.events.clear();

        // WHEN
        objectPicker.onExited();

        // THEN
        QCOMPARE(arbiter.events.count(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "exited");

        arbiter.events.clear();
    }
};


QTEST_APPLESS_MAIN(tst_ObjectPicker)

#include "tst_objectpicker.moc"
