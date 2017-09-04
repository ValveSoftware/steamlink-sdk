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
#include <Qt3DRender/qparameter.h>
#include <Qt3DRender/private/qparameter_p.h>
#include <QObject>
#include <QSignalSpy>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <Qt3DCore/qnodecreatedchange.h>
#include <Qt3DCore/qentity.h>
#include "testpostmanarbiter.h"

class tst_QParameter : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkDefaultConstruction()
    {
        // GIVEN
        Qt3DRender::QParameter parameter;

        // THEN
        QCOMPARE(parameter.name(), QString());
        QCOMPARE(parameter.value(), QVariant());
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DRender::QParameter parameter;

        {
            // WHEN
            QSignalSpy spy(&parameter, SIGNAL(nameChanged(QString)));
            const QString newValue = QStringLiteral("SomeName");
            parameter.setName(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(parameter.name(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            parameter.setName(newValue);

            // THEN
            QCOMPARE(parameter.name(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&parameter, SIGNAL(valueChanged(QVariant)));
            const QVariant newValue(454.0f);
            parameter.setValue(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(parameter.value(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            parameter.setValue(newValue);

            // THEN
            QCOMPARE(parameter.value(), newValue);
            QCOMPARE(spy.count(), 0);
        }
    }

    void checkCreationData()
    {
        // GIVEN
        Qt3DRender::QParameter parameter;

        parameter.setName(QStringLiteral("TwinCam"));
        parameter.setValue(QVariant(427));

        // WHEN
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges;

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&parameter);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 1);

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QParameterData>>(creationChanges.first());
            const Qt3DRender::QParameterData cloneData = creationChangeData->data;

            QCOMPARE(parameter.name(), cloneData.name);
            QCOMPARE(parameter.value(), cloneData.backendValue);
            QCOMPARE(parameter.id(), creationChangeData->subjectId());
            QCOMPARE(parameter.isEnabled(), true);
            QCOMPARE(parameter.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(parameter.metaObject(), creationChangeData->metaObject());
        }

        // WHEN
        parameter.setEnabled(false);

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&parameter);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 1);

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QParameterData>>(creationChanges.first());
            const Qt3DRender::QParameterData cloneData = creationChangeData->data;

            QCOMPARE(parameter.name(), cloneData.name);
            QCOMPARE(parameter.value(), cloneData.backendValue);
            QCOMPARE(parameter.id(), creationChangeData->subjectId());
            QCOMPARE(parameter.isEnabled(), false);
            QCOMPARE(parameter.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(parameter.metaObject(), creationChangeData->metaObject());
        }
    }

    void checkNameUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QParameter parameter;
        arbiter.setArbiterOnNode(&parameter);

        {
            // WHEN
            parameter.setName(QStringLiteral("Bruce"));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "name");
            QCOMPARE(change->value().value<QString>(), parameter.name());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            parameter.setName(QStringLiteral("Bruce"));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkValueUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QParameter parameter;
        arbiter.setArbiterOnNode(&parameter);

        {
            // WHEN
            parameter.setValue(QVariant(383.0f));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "value");
            QCOMPARE(change->value().value<QVariant>(), parameter.value());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            parameter.setValue(QVariant(383.0f));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

        // WHEN -> QNode -> QNodeId
        {
            Qt3DCore::QEntity testEntity;

            {
                // WHEN
                parameter.setValue(QVariant::fromValue(&testEntity));
                QCoreApplication::processEvents();

                // THEN
                QCOMPARE(arbiter.events.size(), 1);
                auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
                QCOMPARE(change->propertyName(), "value");
                QCOMPARE(change->value().value<Qt3DCore::QNodeId>(),testEntity.id());
                QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

                arbiter.events.clear();
            }

            {
                // WHEN
                parameter.setValue(QVariant::fromValue(&testEntity));
                QCoreApplication::processEvents();

                // THEN
                QCOMPARE(arbiter.events.size(), 0);
            }

        }

    }

};

QTEST_MAIN(tst_QParameter)

#include "tst_qparameter.moc"
