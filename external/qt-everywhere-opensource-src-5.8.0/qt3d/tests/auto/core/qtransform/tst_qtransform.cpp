/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
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

#include <QtTest/QtTest>
#include <Qt3DCore/qtransform.h>
#include <Qt3DCore/qcomponent.h>
#include <Qt3DCore/private/qtransform_p.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <QtCore/qscopedpointer.h>
#include "testpostmanarbiter.h"

using namespace Qt3DCore;

class tst_QTransform : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultConstruction()
    {
        // GIVEN
        Qt3DCore::QTransform transform;

        // THEN
        QCOMPARE(transform.isShareable(), false);
        QCOMPARE(transform.matrix(), QMatrix4x4());
        QCOMPARE(transform.scale(), 1.0f);
        QCOMPARE(transform.scale3D(), QVector3D(1.0f, 1.0f, 1.0f));
        QCOMPARE(transform.rotation(), QQuaternion());
        QCOMPARE(transform.rotationX(), 0.0f);
        QCOMPARE(transform.rotationY(), 0.0f);
        QCOMPARE(transform.rotationZ(), 0.0f);
        QCOMPARE(transform.translation(), QVector3D(0.0f, 0.0f, 0.0f));
    }

    void checkCloning_data()
    {
        QTest::addColumn<Qt3DCore::QTransform *>("transform");

        Qt3DCore::QTransform *defaultConstructed = new Qt3DCore::QTransform();
        QTest::newRow("defaultConstructed") << defaultConstructed;

        Qt3DCore::QTransform *matrixPropertySet = new Qt3DCore::QTransform();
        matrixPropertySet->setMatrix(Qt3DCore::QTransform::rotateAround(QVector3D(0.1877f, 0.6868f, 0.3884f), 45.0f, QVector3D(0.0f, 0.0f, 1.0f)));
        QTest::newRow("matrixPropertySet") << matrixPropertySet;

        Qt3DCore::QTransform *translationSet = new Qt3DCore::QTransform();
        translationSet->setTranslation(QVector3D(0.1877f, 0.6868f, 0.3884f));
        QTest::newRow("translationSet") << translationSet;

        Qt3DCore::QTransform *scaleSet = new Qt3DCore::QTransform();
        scaleSet->setScale3D(QVector3D(0.1f, 0.6f, 0.3f));
        QTest::newRow("scaleSet") << scaleSet;

        Qt3DCore::QTransform *rotationSet = new Qt3DCore::QTransform();
        scaleSet->setRotation(Qt3DCore::QTransform::fromAxisAndAngle(0.0f, 0.0f, 1.0f, 30.0f));
        QTest::newRow("rotationSet") << rotationSet;

        Qt3DCore::QTransform *eulerRotationSet = new Qt3DCore::QTransform();
        eulerRotationSet->setRotationX(90.0f);
        eulerRotationSet->setRotationY(10.0f);
        eulerRotationSet->setRotationZ(1.0f);
        QTest::newRow("eulerRotationSet") << eulerRotationSet;
    }

    void checkCloning()
    {
        // GIVEN
        QFETCH(Qt3DCore::QTransform *, transform);

        // WHEN
        Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(transform);
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges = creationChangeGenerator.creationChanges();

        // THEN
        QCOMPARE(creationChanges.size(), 1);

        const Qt3DCore::QNodeCreatedChangePtr<Qt3DCore::QTransformData> creationChangeData =
                qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DCore::QTransformData>>(creationChanges.first());
        const Qt3DCore::QTransformData &cloneData = creationChangeData->data;

        // THEN
        QCOMPARE(creationChangeData->subjectId(), transform->id());
        QCOMPARE(creationChangeData->isNodeEnabled(), transform->isEnabled());
        QCOMPARE(creationChangeData->metaObject(), transform->metaObject());
        QCOMPARE(creationChangeData->parentId(), transform->parentNode() ? transform->parentNode()->id() : Qt3DCore::QNodeId());
        QCOMPARE(transform->translation(), cloneData.translation);
        QCOMPARE(transform->scale3D(), cloneData.scale);
        QCOMPARE(transform->rotation(), cloneData.rotation);
    }

    void checkPropertyUpdates()
    {
        // GIVEN
        TestArbiter arbiter;
        QScopedPointer<Qt3DCore::QTransform> transform(new Qt3DCore::QTransform());
        arbiter.setArbiterOnNode(transform.data());

        // WHEN
        transform->setTranslation(QVector3D(454.0f, 427.0f, 383.0f));
        QCoreApplication::processEvents();

        // THEN
        Qt3DCore::QPropertyUpdatedChangePtr change;
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "translation");
        QCOMPARE(change->value().value<QVector3D>(), QVector3D(454.0f, 427.0f, 383.0f));

        arbiter.events.clear();

        // WHEN
        QQuaternion q = Qt3DCore::QTransform::fromAxisAndAngle(QVector3D(0.0f, 1.0f, 0.0f), 90.0f);
        transform->setRotation(q);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "rotation");
        QCOMPARE(change->value().value<QQuaternion>(), q);

        arbiter.events.clear();

        // WHEN
        transform->setScale3D(QVector3D(883.0f, 1200.0f, 1340.0f));
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "scale3D");
        QCOMPARE(change->value().value<QVector3D>(), QVector3D(883.0f, 1200.0f, 1340.0f));

        arbiter.events.clear();

        // WHEN
        // Force the transform to update its matrix
        (void)transform->matrix();

        transform->setMatrix(QMatrix4x4());
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 3);
        change = arbiter.events.takeFirst().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "scale3D");
        QCOMPARE(change->value().value<QVector3D>(), QVector3D(1.0f, 1.0f, 1.0f));
        change = arbiter.events.takeFirst().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "rotation");
        QCOMPARE(change->value().value<QQuaternion>(), QQuaternion());
        change = arbiter.events.takeFirst().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "translation");
        QCOMPARE(change->value().value<QVector3D>(), QVector3D());

        arbiter.events.clear();

        // WHEN
        transform->setRotationX(20.0f);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "rotation");
        QCOMPARE(change->value().value<QQuaternion>().toEulerAngles().x(), 20.0f);

        arbiter.events.clear();
    }

    void checkSignalEmittion()
    {
        // GIVEN
        QScopedPointer<Qt3DCore::QTransform> transform(new Qt3DCore::QTransform());

        int rotationXChangedCount = 0;
        int rotationYChangedCount = 0;
        int rotationZChangedCount = 0;
        int rotationChangedCount = 0;
        int matrixChangedCount = 0;
        int scaleChangedCount = 0;
        int scale3DChangedCount = 0;
        int translationChangedCount = 0;

        QObject::connect(transform.data(), &Qt3DCore::QTransform::rotationChanged, [&] { ++rotationChangedCount; });
        QObject::connect(transform.data(), &Qt3DCore::QTransform::rotationXChanged, [&] { ++rotationXChangedCount; });
        QObject::connect(transform.data(), &Qt3DCore::QTransform::rotationYChanged, [&] { ++rotationYChangedCount; });
        QObject::connect(transform.data(), &Qt3DCore::QTransform::rotationZChanged, [&] { ++rotationZChangedCount; });
        QObject::connect(transform.data(), &Qt3DCore::QTransform::matrixChanged, [&] { ++matrixChangedCount; });
        QObject::connect(transform.data(), &Qt3DCore::QTransform::scale3DChanged, [&] { ++scale3DChangedCount; });
        QObject::connect(transform.data(), &Qt3DCore::QTransform::scaleChanged, [&] { ++scaleChangedCount; });
        QObject::connect(transform.data(), &Qt3DCore::QTransform::translationChanged, [&] { ++translationChangedCount; });

        // WHEN
        transform->setRotationX(180.0f);

        // THEN
        QCOMPARE(rotationXChangedCount, 1);
        QCOMPARE(rotationYChangedCount, 0);
        QCOMPARE(rotationZChangedCount, 0);
        QCOMPARE(rotationChangedCount, 1);
        QCOMPARE(matrixChangedCount, 1);
        QCOMPARE(scaleChangedCount, 0);
        QCOMPARE(scale3DChangedCount, 0);
        QCOMPARE(translationChangedCount, 0);

        // WHEN
        transform->setRotationY(180.0f);

        // THEN
        QCOMPARE(rotationXChangedCount, 1);
        QCOMPARE(rotationYChangedCount, 1);
        QCOMPARE(rotationZChangedCount, 0);
        QCOMPARE(rotationChangedCount, 2);
        QCOMPARE(matrixChangedCount, 2);
        QCOMPARE(scaleChangedCount, 0);
        QCOMPARE(scale3DChangedCount, 0);
        QCOMPARE(translationChangedCount, 0);

        // WHEN
        transform->setRotationZ(180.0f);

        // THEN
        QCOMPARE(rotationXChangedCount, 1);
        QCOMPARE(rotationYChangedCount, 1);
        QCOMPARE(rotationZChangedCount, 1);
        QCOMPARE(rotationChangedCount, 3);
        QCOMPARE(matrixChangedCount, 3);
        QCOMPARE(scaleChangedCount, 0);
        QCOMPARE(scale3DChangedCount, 0);
        QCOMPARE(translationChangedCount, 0);

        // WHEN
        transform->setRotation(Qt3DCore::QTransform::fromEulerAngles(15.0f, 25.0f, 84.0f));

        // THEN
        QCOMPARE(rotationXChangedCount, 2);
        QCOMPARE(rotationYChangedCount, 2);
        QCOMPARE(rotationZChangedCount, 2);
        QCOMPARE(rotationChangedCount, 4);
        QCOMPARE(matrixChangedCount, 4);
        QCOMPARE(scaleChangedCount, 0);
        QCOMPARE(scale3DChangedCount, 0);
        QCOMPARE(translationChangedCount, 0);

        // WHEN
        transform->setMatrix(QMatrix4x4());

        // THEN
        QCOMPARE(rotationXChangedCount, 3);
        QCOMPARE(rotationYChangedCount, 3);
        QCOMPARE(rotationZChangedCount, 3);
        QCOMPARE(rotationChangedCount, 5);
        QCOMPARE(matrixChangedCount, 5);
        QCOMPARE(scaleChangedCount, 1);
        QCOMPARE(scale3DChangedCount, 1);
        QCOMPARE(translationChangedCount, 1);

        // WHEN
        transform->setScale(18.0f);

        // THEN
        QCOMPARE(rotationXChangedCount, 3);
        QCOMPARE(rotationYChangedCount, 3);
        QCOMPARE(rotationZChangedCount, 3);
        QCOMPARE(rotationChangedCount, 5);
        QCOMPARE(matrixChangedCount, 6);
        QCOMPARE(scaleChangedCount, 2);
        QCOMPARE(scale3DChangedCount, 2);
        QCOMPARE(translationChangedCount, 1);

        // WHEN
        transform->setScale3D(QVector3D(15.0f, 18.0f, 15.0f));

        // THEN
        QCOMPARE(rotationXChangedCount, 3);
        QCOMPARE(rotationYChangedCount, 3);
        QCOMPARE(rotationZChangedCount, 3);
        QCOMPARE(rotationChangedCount, 5);
        QCOMPARE(matrixChangedCount, 7);
        QCOMPARE(scaleChangedCount, 2);
        QCOMPARE(scale3DChangedCount, 3);
        QCOMPARE(translationChangedCount, 1);

        // WHEN
        transform->setTranslation(QVector3D(350.0f, 383.0f, 454.0f));

        // THEN
        QCOMPARE(rotationXChangedCount, 3);
        QCOMPARE(rotationYChangedCount, 3);
        QCOMPARE(rotationZChangedCount, 3);
        QCOMPARE(rotationChangedCount, 5);
        QCOMPARE(matrixChangedCount, 8);
        QCOMPARE(scaleChangedCount, 2);
        QCOMPARE(scale3DChangedCount, 3);
        QCOMPARE(translationChangedCount, 2);
    }

    void checkCompositionDecomposition()
    {
        // GIVEN
        Qt3DCore::QTransform t;
        Qt3DCore::QTransform t2;
        QMatrix4x4 m = Qt3DCore::QTransform::rotateAround(QVector3D(0.1877f, 0.6868f, 0.3884f), 45.0f, QVector3D(0.0f, 0.0f, 1.0f));

        // WHEN
        t.setMatrix(m);
        t2.setScale3D(t.scale3D());
        t2.setRotation(t.rotation());
        t2.setTranslation(t.translation());

        // THEN
        QCOMPARE(t.scale3D(), t2.scale3D());
        QCOMPARE(t.rotation(), t2.rotation());
        QCOMPARE(t.translation(), t2.translation());

        // Note: t.matrix() != t2.matrix() since different matrices
        // can result in the same scale, rotation, translation
    }
};

QTEST_MAIN(tst_QTransform)

#include "tst_qtransform.moc"
