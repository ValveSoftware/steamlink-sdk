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


#include <QtTest/QTest>
#include <Qt3DCore/qtransform.h>
#include <Qt3DCore/private/qtransform_p.h>
#include <Qt3DRender/private/transform_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include "qbackendnodetester.h"
#include "testrenderer.h"

class tst_Transform : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT

private Q_SLOTS:

    void checkInitialState()
    {
        // GIVEN
        Qt3DRender::Render::Transform backendTransform;

        // THEN
        QCOMPARE(backendTransform.isEnabled(), false);
        QVERIFY(backendTransform.peerId().isNull());
        QCOMPARE(backendTransform.transformMatrix(), QMatrix4x4());
    }

    void checkCleanupState()
    {
        // GIVEN
        Qt3DRender::Render::Transform backendTransform;

        // WHEN
        {
            Qt3DCore::QTransform transform;
            transform.setScale3D(QVector3D(1.0f, 2.0f, 3.0f));
            transform.setTranslation(QVector3D(-1.0, 5.0f, -2.0f));
            transform.setRotation(QQuaternion::fromAxisAndAngle(QVector3D(1.0f, 0.0f, 0.0), 30.0f));
            simulateInitialization(&transform, &backendTransform);
        }
        backendTransform.setEnabled(true);

        backendTransform.cleanup();

        // THEN
        QCOMPARE(backendTransform.isEnabled(), false);
        QCOMPARE(backendTransform.transformMatrix(), QMatrix4x4());
        QCOMPARE(backendTransform.rotation(), QQuaternion());
        QCOMPARE(backendTransform.scale(), QVector3D());
        QCOMPARE(backendTransform.translation(), QVector3D());
    }

    void checkInitializeFromPeer()
    {
        // GIVEN
        Qt3DCore::QTransform transform;
        transform.setScale3D(QVector3D(1.0f, 2.0f, 3.0f));
        transform.setTranslation(QVector3D(-1.0, 5.0f, -2.0f));
        transform.setRotation(QQuaternion::fromAxisAndAngle(QVector3D(1.0f, 0.0f, 0.0), 30.0f));

        {
            // WHEN
            Qt3DRender::Render::Transform backendTransform;
            simulateInitialization(&transform, &backendTransform);

            // THEN
            QCOMPARE(backendTransform.isEnabled(), true);
            QCOMPARE(backendTransform.peerId(), transform.id());
            QCOMPARE(backendTransform.transformMatrix(), transform.matrix());
            QCOMPARE(backendTransform.rotation(), transform.rotation());
            QCOMPARE(backendTransform.scale(), transform.scale3D());
            QCOMPARE(backendTransform.translation(), transform.translation());
        }
        {
            // WHEN
            Qt3DRender::Render::Transform backendTransform;
            transform.setEnabled(false);
            simulateInitialization(&transform, &backendTransform);

            // THEN
            QCOMPARE(backendTransform.peerId(), transform.id());
            QCOMPARE(backendTransform.isEnabled(), false);
        }
    }

    void checkSceneChangeEvents()
    {
        // GIVEN
        Qt3DRender::Render::Transform backendTransform;
        TestRenderer renderer;
        backendTransform.setRenderer(&renderer);

        {
            // WHEN
            const bool newValue = false;
            const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
            change->setPropertyName("enabled");
            change->setValue(newValue);
            backendTransform.sceneChangeEvent(change);

            // THEN
            QCOMPARE(backendTransform.isEnabled(), newValue);
        }
        {
            // WHEN
            const QQuaternion newValue = QQuaternion::fromAxisAndAngle(QVector3D(0.0f, 1.0f, 0.0f), 45.0f);
            const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
            change->setPropertyName("rotation");
            change->setValue(QVariant::fromValue(newValue));
            backendTransform.sceneChangeEvent(change);

            // THEN
            QCOMPARE(backendTransform.rotation(), newValue);
        }
        {
            // WHEN
            const QVector3D newValue(454.0f, 355.0f, 0.0f);
            const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
            change->setPropertyName("scale3D");
            change->setValue(QVariant::fromValue(newValue));
            backendTransform.sceneChangeEvent(change);

            // THEN
            QCOMPARE(backendTransform.scale(), newValue);
        }
        {
            // WHEN
            const QVector3D newValue(383.0f, 0.0f, 427.0f);
            const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
            change->setPropertyName("translation");
            change->setValue(QVariant::fromValue(newValue));
            backendTransform.sceneChangeEvent(change);

            // THEN
            QCOMPARE(backendTransform.translation(), newValue);
        }
    }

};

QTEST_MAIN(tst_Transform)

#include "tst_transform.moc"
