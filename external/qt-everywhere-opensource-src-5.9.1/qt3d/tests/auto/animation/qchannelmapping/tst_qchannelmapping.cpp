/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
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
#include <Qt3DAnimation/qchannelmapping.h>
#include <Qt3DAnimation/private/qchannelmapping_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qentity.h>
#include <Qt3DCore/qnodecreatedchange.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <QObject>
#include <QSignalSpy>
#include <testpostmanarbiter.h>

class tst_QChannelMapping : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void checkDefaultConstruction()
    {
        // GIVEN
        Qt3DAnimation::QChannelMapping mapping;

        // THEN
        QCOMPARE(mapping.channelName(), QString());
        QCOMPARE(mapping.target(), static_cast<Qt3DCore::QNode *>(nullptr));
        QCOMPARE(mapping.property(), QString());
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DAnimation::QChannelMapping mapping;

        {
            // WHEN
            QSignalSpy spy(&mapping, SIGNAL(channelNameChanged(QString)));
            const QString newValue(QStringLiteral("Rotation"));
            mapping.setChannelName(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(mapping.channelName(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            mapping.setChannelName(newValue);

            // THEN
            QCOMPARE(mapping.channelName(), newValue);
            QCOMPARE(spy.count(), 0);
        }

        {
            // WHEN
            QSignalSpy spy(&mapping, SIGNAL(targetChanged(Qt3DCore::QNode*)));
            auto newValue = new Qt3DCore::QEntity();
            mapping.setTarget(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(mapping.target(), newValue);
            QCOMPARE(newValue->parent(), &mapping);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            mapping.setTarget(newValue);

            // THEN
            QCOMPARE(mapping.target(), newValue);
            QCOMPARE(spy.count(), 0);
        }

        {
            // WHEN
            QSignalSpy spy(&mapping, SIGNAL(propertyChanged(QString)));
            const QString newValue(QStringLiteral("rotation"));
            mapping.setProperty(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(mapping.property(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            mapping.setProperty(newValue);

            // THEN
            QCOMPARE(mapping.property(), newValue);
            QCOMPARE(spy.count(), 0);
        }
    }

    void checkCreationData()
    {
        // GIVEN
        Qt3DAnimation::QChannelMapping mapping;
        auto target = new Qt3DCore::QEntity;

        mapping.setChannelName(QStringLiteral("Location"));
        mapping.setTarget(target);
        mapping.setProperty(QStringLiteral("translation"));

        // WHEN
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges;

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&mapping);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 2); // 1 for mapping, 1 for target

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DAnimation::QChannelMappingData>>(creationChanges.first());
            const Qt3DAnimation::QChannelMappingData data = creationChangeData->data;

            QCOMPARE(mapping.id(), creationChangeData->subjectId());
            QCOMPARE(mapping.isEnabled(), true);
            QCOMPARE(mapping.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(mapping.metaObject(), creationChangeData->metaObject());
            QCOMPARE(mapping.channelName(), data.channelName);
            QCOMPARE(mapping.target()->id(), data.targetId);
            QCOMPARE(mapping.property(), data.property);
        }

        // WHEN
        mapping.setEnabled(false);

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&mapping);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 2); // 1 for mapping, 1 for target

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DAnimation::QChannelMappingData>>(creationChanges.first());
            const Qt3DAnimation::QChannelMappingData data = creationChangeData->data;

            QCOMPARE(mapping.id(), creationChangeData->subjectId());
            QCOMPARE(mapping.isEnabled(), false);
            QCOMPARE(mapping.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(mapping.metaObject(), creationChangeData->metaObject());
            QCOMPARE(mapping.channelName(), data.channelName);
            QCOMPARE(mapping.target()->id(), data.targetId);
            QCOMPARE(mapping.property(), data.property);
        }
    }

    void checkPropertyUpdateChanges()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DAnimation::QChannelMapping mapping;
        arbiter.setArbiterOnNode(&mapping);

        {
            // WHEN
            mapping.setChannelName(QStringLiteral("Scale"));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "channelName");
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);
            QCOMPARE(change->value().toString(), mapping.channelName());

            arbiter.events.clear();

            // WHEN
            mapping.setChannelName(QStringLiteral("Scale"));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

        {
            // WHEN
            auto target = new Qt3DCore::QEntity();
            mapping.setTarget(target);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "target");
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);
            QCOMPARE(change->value().value<Qt3DCore::QNodeId>(), mapping.target()->id());

            arbiter.events.clear();

            // WHEN
            mapping.setTarget(target);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

        {
            // WHEN
            mapping.setProperty(QStringLiteral("scale"));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "property");
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);
            QCOMPARE(change->value().toString(), mapping.property());

            arbiter.events.clear();

            // WHEN
            mapping.setProperty(QStringLiteral("scale"));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }
};

QTEST_MAIN(tst_QChannelMapping)

#include "tst_qchannelmapping.moc"
